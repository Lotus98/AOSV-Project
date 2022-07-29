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
        /* TODO:
         * - Synchronize the tid population (?with semaphores: semctl -> Check CSAP example)
         */
        struct ums_arg *wrapper_arg;
        int ret_pthread;

        // Tid will be populated by wrap_routine
        thread->tid = -1;

        // Initializing the wrapper argument to be passed to wrap_routine
        wrapper_arg = malloc(sizeof(struct ums_arg));
        if (!wrapper_arg) {
                perror("Allocating struct ums_arg wrapper_arg");
                return -ENOMEM;
        }
        wrapper_arg->ums_thread = thread;
        wrapper_arg->ums_routine = start_routine;
        wrapper_arg->arg = arg;

        ret_pthread = pthread_create(&(thread->pthread), NULL, wrap_routine, wrapper_arg);
        if (ret_pthread != 0) {
                perror("Creating pthread");
                free(wrapper_arg);
                return ret_pthread;
        }

        return SUCCESS;
}
