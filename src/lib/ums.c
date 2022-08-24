/** @file ums.c
 *  @brief Contains the main functions for the UMS library.
 *
 *  Contains the implementations of the main functions to be used by the user
 *  applications when using UMS
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */

#include "list.h"
#include "shared.h"
#include "bitmap.h"
#include "ums.h"
#include "utils.h"

#include <asm-generic/errno-base.h>
#include <fcntl.h>
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
        if (pthread_mutex_init(&(worker->mutex), NULL) != 0) {
                perror("Initializing worker mutex");
                return FAILURE;
        }
        worker->state = WORKER_IDLE;
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

        // Get worker's mutex.
        pthread_mutex_lock(&worker->mutex);
        // Increment refcnt
        worker->refcnt++;
        list_add(&worker_node->list, &head->list);
        // Release mutex.
        pthread_mutex_unlock(&worker->mutex);

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
        int cpuid, nworkers = 0, ret_pthread;
        ums_worker_node_t *worker_node;
        struct list_head *pos;
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
        ums_schedulers[cpuid]->cpuid = (unsigned int)cpuid;
        ums_schedulers[cpuid]->ums_thread = malloc(sizeof(struct ums_thread));
        if (!ums_schedulers[cpuid]->ums_thread) {
                perror("Allocating struct ums_thread for scheduler");
                free(ums_schedulers[cpuid]);
                UNSET_BIT(cpus_map, cpuid);
                return FAILURE;
        }
        ums_schedulers[cpuid]->current_worker = NULL;
        list_for_each(pos, &worker_list->list) {
                nworkers++;
        }
        ums_schedulers[cpuid]->nworkers = (unsigned int)nworkers;
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
        ret_pthread = pthread_create(&sched_arg->ums_thread->pthread, NULL, sched_wrap_routine, sched_arg);
        if (ret_pthread != 0) {
                perror("Creating pthread");
                free(sched_arg->sem);
                free(sched_arg);
                free(ums_schedulers[cpuid]->ums_thread);
                free(ums_schedulers[cpuid]);
                UNSET_BIT(cpus_map, cpuid);
                return ret_pthread;
        }
        // Wait for the ums_thread->tid to be populated and for the scheduler to be registered.
        if (sem_wait(sched_arg->sem) != 0) {
                perror("[Main thread] Waiting on semaphore");
                abort();
        }
        // Destroy semaphore (it is not needed anymore)
        if (sem_destroy(sched_arg->sem) != 0) {
                perror("[Main thread] Destroying semaphore");
        }
        free(sched_arg->sem);

        // Register worker list.
        list_for_each_entry(worker_node, &worker_list->list, list) {
                struct ums_usr_worker *usr_worker;

                usr_worker = malloc(sizeof(*usr_worker));
                usr_worker->tid = worker_node->worker->thread.tid;
                usr_worker->cpuid = (unsigned int)cpuid;
                ioctl(dev_fd, REGISTER_WORKER, usr_worker);
                free(usr_worker);
        }

        return SUCCESS;
}

int ExecuteUmsThread (struct ums_worker *worker)
{
        unsigned int cpuid;
        long retval;

        getcpu(&cpuid, NULL);
        // Check and change worker state
        pthread_mutex_lock(&worker->mutex);
        if (worker->state != WORKER_IDLE) {
                errno = EBUSY;
                return FAILURE;
        }
        worker->state = WORKER_RUNNING;
        pthread_mutex_unlock(&worker->mutex);

        // Set current_worker.
        ums_schedulers[cpuid]->current_worker = worker;

        retval = ioctl(dev_fd, EXECUTE_THREAD, &worker->thread.tid);
        if (!retval) {
                perror("Couldn't execute given worker");
                ums_schedulers[cpuid]->current_worker = NULL;
                return FAILURE;
        }

        return SUCCESS;
}

int UmsThreadYield (void)
{
        unsigned int cpuid;
        struct ums_worker *worker;

        getcpu(&cpuid, NULL);
        worker = ums_schedulers[cpuid]->current_worker;
        // Change worker's state
        pthread_mutex_lock(&worker->mutex);
        worker->state = WORKER_IDLE;
        pthread_mutex_unlock(&worker->mutex);

        // Unset current_worker
        ums_schedulers[cpuid]->current_worker = NULL;

        // Yield worker
        ioctl(dev_fd, THREAD_YIELD);

        return SUCCESS;
}

struct list_head *DequeueUmsCompletionListItems (void)
{
        struct list_head *head = NULL;
        ums_worker_node_t *worker_node;
        unsigned int *tid_list, cpuid;

        getcpu(&cpuid, NULL);
        // The "+ 1" is to get the number of available workers
        tid_list = calloc(ums_schedulers[cpuid]->nworkers + 1, sizeof(*tid_list));
        // In the first position copy the nmemb size of the array. (Little hack for LKM)
        *tid_list = ums_schedulers[cpuid]->nworkers;

        // IOCTL call
        ioctl(dev_fd, DEQUEUE_LIST, tid_list);
        if (*tid_list == 0) // All the workers are terminated
                return NULL;

        // Create list
        head = malloc(sizeof(*head));
        for (size_t i = 1; i <= ums_schedulers[cpuid]->nworkers; i++) {
                if (tid_list[i] == 0)
                        break;
                worker_node = find_worker_tid(ums_schedulers[cpuid]->worker_list, (pid_t)tid_list[i]);
                if (!worker_node) {
                        fprintf(stderr, "Worker[TID]: %d not found in scheduler[ID]: %d\n", tid_list[i], cpuid);
                        continue;
                }
                list_add(&worker_node->list, head);
        }

        return head;
}
