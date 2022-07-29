/** @file utils.h
 *  @brief All helper functions for ums library.
 *
 *  This file contains the headers for the helper functions and data structures
 *  needed for the UMS library.
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#ifndef LIB_UTILS_H
#define LIB_UTILS_H

// Includes
#include "shared.h"
#include <sys/ioctl.h>

// Macros
/// Changes the state of the current thread to TASK_IDLE to avoid being scheduled.
#define WORKER_IDLE() do                        \
{                                               \
        ioctl(driver_fd, SET_WORKER_IDLE);      \
} while(0)

// Prototypes
/** @brief Opens the driver's device.
 *
 *  @return int Returns a file descriptor to the open device.
 */
int open_device (void);

/** @brief wraps the original ums pthread routine.
 *
 *  This function is used to wrap the original ums_thread_create start_routine
 *  to correctly populate the needed data structures and to set the thread into
 *  an IDLE state, ready to be scheduled.
 *
 *  @param arg: A wrapper argument (of type struct ums_arg) to the original *arg.
 */
void *wrap_routine (void *arg);

#endif // !LIB_UTILS_H
