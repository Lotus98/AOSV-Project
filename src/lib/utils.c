/** @file utils.c
 *  @brief Contains the helper functions for the UMS library.
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#include "utils.h"
#include "shared.h"

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

void *wrap_routine (void *arg)
{
        /*  TODO:
         *  - Synchronize gettid (maybe with semaphores: semctl)
         *  - put to sleep thread (IOCTL WORKER_IDLE).
         *  - Any clean up to do after routine is executed
         */
        struct ums_arg *wrap_arg = (struct ums_arg *)arg;
        struct ums_thread *thread = wrap_arg->ums_thread;

        // Get TID
        thread->tid = gettid();


        // Idle
        WORKER_IDLE();

        // Execute routine
        wrap_arg->ums_routine(wrap_arg->arg);

        // Cleanup
        free(arg);

        return NULL;
}
