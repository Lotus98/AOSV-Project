/** @file ums.c
 *  @brief Contains the main functions for the UMS library.
 *
 *  Contains the implementations of the main functions to be used by the user
 *  applications when using UMS
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#include "ums.h"
#include "shared.h"
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

int ums_init ()
{
        driver_fd = open_device();
        if (driver_fd < 0) {
                return FAILURE;
        }
        return SUCCESS;
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
        PRINTDBG("Tid should be populated");
        // Destroy semaphore (it is not needed anymore)
        if (sem_destroy(wrapper_arg->tid_sem) != 0) {
                perror("[Main thread] Destroying semaphore");
                return FAILURE;
        }
        free(wrapper_arg->tid_sem);

        return SUCCESS;
}
