/** @file ums.c
 *  @brief Contains the main functions for the UMS library.
 *
 *  Contains the implementations of the main functions to be used by the user
 *  applications when using UMS
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */

#include "shared.h"
#include "bitmap.h"
#include "ums.h"
#include "utils.h"

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/sysinfo.h>
#include <unistd.h>

int ums_init ()
{
        // Open IOCTL device
        dev_fd = open_device();
        if (dev_fd < 0) {
                return FAILURE;
        }

        // Configure processors related information
        ncpus = (size_t)get_nprocs();
        cpus_map = DECLARE_BITMAP((unsigned long)ncpus);
        if (!cpus_map) {
                perror("Allocating cpus_map during initialization");
                return FAILURE;
        }
        ums_schedulers = calloc((unsigned long)ncpus, sizeof(ums_schedulers));
        if (!ums_schedulers) {
                perror("Allocating ums_schedulers during initialization");
                free(cpus_map);
                close(dev_fd);
                return FAILURE;
        }

        // Register process TGID in UMS mode.
        if (ioctl(dev_fd, REGISTER_PROC) != SUCCESS) {
                perror("Couldn't register process to UMS mode.");
                free(cpus_map);
                close(dev_fd);
                return FAILURE;
        }

        return SUCCESS;
}

void ums_destroy ()
{
        // Free bitmap
        free(cpus_map);
        // Free ums_schedulers
        for (size_t i = 0; i < ncpus; i++) {
                if (ums_schedulers[i]) {
                        free(ums_schedulers[i]);
                }
        }
        free(ums_schedulers);

        // Unregister ums process
        ioctl(dev_fd, UNREGISTER_PROC);

        // Close IOCTL device
        close(dev_fd);

        return;
}

int ums_worker_create (struct ums_worker *worker,
                       void *(*start_routine) (void *),
                       void *arg)
{
        struct ums_worker_arg *wrapper_arg;
        struct ums_thread *thread;
        int ret_pthread;

        // Initialize worker
        worker->refcnt = 0;
        if (pthread_rwlock_init(&(worker->rwlock), NULL) != 0) {
                perror("Initializing worker rwlock");
                return FAILURE;
        }

        thread = &worker->thread;
        // Tid will be populated by worker_wrap_routine
        thread->tid = -1;

        // Initializing the wrapper argument to be passed to worker_wrap_routine
        wrapper_arg = malloc(sizeof(*wrapper_arg));
        if (!wrapper_arg) {
                perror("Allocating struct ums_worker_arg wrapper_arg");
                return -ENOMEM;
        }
        wrapper_arg->ums_thread = thread;
        wrapper_arg->ums_routine = start_routine;
        wrapper_arg->arg = arg;
        wrapper_arg->tid_sem = malloc(sizeof(*wrapper_arg->tid_sem));
        if (!wrapper_arg->tid_sem) {
                perror("Allocating sem_t tid_sem");
                free(wrapper_arg);
                return -ENOMEM;
        }
        if (sem_init(wrapper_arg->tid_sem, 0, 0) != 0) {
                perror("Initializing semaphore");
                free(wrapper_arg->tid_sem);
                free(wrapper_arg);
                return FAILURE;
        }

        ret_pthread = pthread_create(&thread->pthread, NULL, worker_wrap_routine, wrapper_arg);
        if (ret_pthread != 0) {
                perror("Creating pthread");
                free(wrapper_arg->tid_sem);
                free(wrapper_arg);
                return ret_pthread;
        }

        // Wait for the ums_thread->tid to be populated
        if (sem_wait(wrapper_arg->tid_sem) != 0) {
                perror("[Main thread] Waiting on semaphore");
                // Abort execution if tid semaphore didn't work
                abort();
        }
        // Destroy semaphore (it is not needed anymore)
        if (sem_destroy(wrapper_arg->tid_sem) != 0) {
                perror("[Main thread] Destroying semaphore");
        }
        free(wrapper_arg->tid_sem);

        return SUCCESS;
}

int ums_worker_list_init(ums_list_head_t *head)
{
        INIT_LIST_HEAD(&head->list);
        if (pthread_rwlock_init(&head->rwlock, NULL) != 0) {
                perror("Creating rwlock for worker list head");
                return FAILURE;
        }
        return SUCCESS;
}

int ums_worker_list_insert(ums_list_head_t *head, struct ums_worker *worker)
{
        ums_worker_node_t *worker_node = malloc(sizeof(*worker_node));
        if (!worker_node) {
                perror("Allocating worker");
                return FAILURE;
        }
        worker_node->worker = worker;
        // Acquire list rwlock
        if (pthread_rwlock_wrlock(&head->rwlock) != 0) {
                perror("Getting head rwlock");
                free(worker_node);
                return FAILURE;
        }

        list_add(&worker_node->list, &head->list);

        // Release list rwlock
        if (pthread_rwlock_unlock(&head->rwlock) != 0) {
                perror("Releasing head rwlock");
                free(worker_node);
                return FAILURE;
        }

        return SUCCESS;
}

int EnterUmsSchedulingMode(void (*scheduler_routine)(), ums_list_head_t *worker_list)
{
        /*  TODO:
         *  - Create wrapper for pthread_create for scheduler.
         *      - Register scheduler and worker list in LKM
         */
        int cpuid, ret_pthread;
        struct ums_sched_arg *sched_arg;

        cpuid = find_next_zero_bit(cpus_map, ncpus);
        if (cpuid < 0) {
                fprintf(stderr, "[Error] UMSLIB: Cannot run a new scheduler, all cpus are allocated\n");
                return FAILURE;
        }
        SET_BIT(cpus_map, cpuid);
        ums_schedulers[cpuid] = malloc(sizeof(**ums_schedulers));
        if (!ums_schedulers[cpuid]) {
                perror("Allocating struct ums_sched for requested scheduler");
                return FAILURE;
        }
        ums_schedulers[cpuid]->cpuid = cpuid;
        ums_schedulers[cpuid]->ums_thread = malloc(sizeof(struct ums_thread));
        if (!ums_schedulers[cpuid]->ums_thread) {
                perror("Allocating struct ums_thread for scheduler");
                free(ums_schedulers[cpuid]);
                UNSET_BIT(cpus_map, cpuid);
                return FAILURE;
        }
        ums_schedulers[cpuid]->worker_list = worker_list;
        // Initilized by the wrapper
        ums_schedulers[cpuid]->ums_thread->tid = -1;

        // Initialize wrapper arg struct
        sched_arg = malloc(sizeof(*sched_arg));
        if (!sched_arg) {
                perror("Allocating sched_arg wrapper arguments");
                free(ums_schedulers[cpuid]->ums_thread);
                free(ums_schedulers[cpuid]);
                UNSET_BIT(cpus_map, cpuid);
                return FAILURE;
        }
        sched_arg->ums_thread = ums_schedulers[cpuid]->ums_thread;
        sched_arg->sched_routine = scheduler_routine;
        sched_arg->cpuid = (unsigned int)cpuid;
        sched_arg->sem = malloc(sizeof(*sched_arg->sem));
        if (!sched_arg->sem) {
                perror("Initializing semaphore");
                free(sched_arg);
                free(ums_schedulers[cpuid]->ums_thread);
                free(ums_schedulers[cpuid]);
                UNSET_BIT(cpus_map, cpuid);
                return FAILURE;
        }
        if (sem_init(sched_arg->sem, 0, 0) != 0) {
                perror("Initializing semaphore");
                free(sched_arg->sem);
                free(sched_arg);
                free(ums_schedulers[cpuid]->ums_thread);
                free(ums_schedulers[cpuid]);
                UNSET_BIT(cpus_map, cpuid);
                return FAILURE;
        }

        // Create scheduler thread.
        ret_pthread = pthread_create(&sched_arg->ums_thread->pthread, NULL, sched_wrap_routine, NULL);
        if (ret_pthread != 0) {
                perror("Creating pthread");
                free(sched_arg->sem);
                free(sched_arg);
                free(ums_schedulers[cpuid]->ums_thread);
                free(ums_schedulers[cpuid]);
                UNSET_BIT(cpus_map, cpuid);
                return ret_pthread;
        }
        // Wait for the ums_thread->tid to be populated
        if (sem_wait(sched_arg->sem) != 0) {
                perror("[Main thread] Waiting on semaphore");
                abort();
        }
        // Destroy semaphore (it is not needed anymore)
        if (sem_destroy(sched_arg->sem) != 0) {
                perror("[Main thread] Destroying semaphore");
        }
        free(sched_arg->sem);

        /*  TODO:
         *  - Register worker_list into LKM, having care it is bound to the scheduler.
         */


        return SUCCESS;
}
