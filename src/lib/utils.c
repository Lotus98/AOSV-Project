/** @file utils.c
 *  @brief Contains the helper functions for the UMS library.
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#include "utils.h"
#include "shared.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

int open_device ()
{
        int fd;

        fd = open(DRIVER_PATH, O_RDWR);
        if (fd < 0) {
                perror("Open DRIVER_PATH");
                return FAILURE;
        }
        return fd;
}

void *worker_wrap_routine (void *arg)
{
        /*  TODO:
         *  - Any clean up to do after routine is executed
         */
        struct ums_arg *wrap_arg = (struct ums_arg *)arg;
        struct ums_thread *thread = wrap_arg->ums_thread;

        // Get TID
        thread->tid = gettid();
        if (sem_post(wrap_arg->tid_sem) != 0) {
                perror("Incrementing semaphore");
                pthread_exit(NULL);
        }


        // Idle
        WORKER_IDLE();

        // Execute routine
        wrap_arg->ums_routine(wrap_arg->arg);

        // Cleanup
        free(arg);

        return NULL;
}
