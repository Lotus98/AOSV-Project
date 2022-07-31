/** @file ums.c
 *  @brief Contains the main functions for the UMS library.
 *
 *  Contains the implementations of the main functions to be used by the user
 *  applications when using UMS
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */

/*  TODO:
 *  - Implement EnterUmsSchedulingMode()
 *
 */
#include "ums.h"
#include "bitmap.h"
#include "list.h"
#include "shared.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int ums_init ()
{
        // Open IOCTL device
        driver_fd = open_device();
        if (driver_fd < 0) {
                return FAILURE;
        }

        // Configure processors related information
        nprocs = get_nprocs();
        ums_procs = DECLARE_BITMAP((unsigned long)nprocs);

        return SUCCESS;
}

void ums_destroy ()
{
        // Close IOCTL device
        close(driver_fd);
        // Free bitmap
        free(ums_procs);

        return;
}

int ums_thread_create (struct ums_thread *thread,
                       void *(*start_routine) (void *),
                       void *arg)
{
        struct ums_arg *wrapper_arg;
        int ret_pthread;

        // Tid will be populated by worker_wrap_routine
        thread->tid = -1;

        // Initializing the wrapper argument to be passed to worker_wrap_routine
        wrapper_arg = malloc(sizeof(*wrapper_arg));
        if (!wrapper_arg) {
                perror("Allocating struct ums_arg wrapper_arg");
                return -ENOMEM;
        }
        wrapper_arg->ums_thread = thread;
        wrapper_arg->ums_routine = start_routine;
        wrapper_arg->arg = arg;
        wrapper_arg->tid_sem = malloc(sizeof(*wrapper_arg->tid_sem));
        if (!wrapper_arg->tid_sem) {
                perror("Allocating sem_t tid_sem");
                return -ENOMEM;
        }
        if (sem_init(wrapper_arg->tid_sem, 0, 0) != 0) {
                perror("Initializing semaphore");
                return FAILURE;
        }

        ret_pthread = pthread_create(&thread->pthread, NULL, worker_wrap_routine, wrapper_arg);
        if (ret_pthread != 0) {
                perror("Creating pthread");
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
                return FAILURE;
        }
        free(wrapper_arg->tid_sem);

        return SUCCESS;
}

int ums_worker_list_init(ums_list_head_t *head)
{
        INIT_LIST_HEAD(&head->list);
        if (pthread_rwlock_init(&head->rwlock, NULL) != 0) {
                perror("Creating rwlock for workers list head");
                return FAILURE;
        }
        return SUCCESS;
}

int ums_worker_list_insert(ums_list_head_t *head, struct ums_thread *thread)
{
        ums_worker_node_t *worker_node = malloc(sizeof(*worker_node));
        if (!worker_node) {
                perror("Allocating worker");
                return FAILURE;
        }
        worker_node->worker.thread = thread;
        worker_node->worker.refcnt = 0;
        if (pthread_rwlock_init(&(worker_node->worker.rwlock), NULL) != 0) {
                perror("Initializing worker rwlock");
                free(worker_node);
                return FAILURE;
        }
        // Acquire list rwlock
        if (pthread_rwlock_wrlock(&head->rwlock) != 0) {
                perror("Getting head rwlock");
                pthread_rwlock_destroy(&worker_node->worker.rwlock);
                free(worker_node);
                return FAILURE;
        }

        list_add(&worker_node->list, &head->list);

        // Release list rwlock
        if (pthread_rwlock_unlock(&head->rwlock) != 0) {
                perror("Releasing head rwlock");
                pthread_rwlock_destroy(&worker_node->worker.rwlock);
                free(worker_node);
                return FAILURE;
        }

        return SUCCESS;
}
