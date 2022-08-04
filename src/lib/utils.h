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
        ioctl(dev_fd, SET_WORKER_IDLE);         \
} while(0);

// Prototypes
/** @brief Opens the driver's device.
 *
 *  @return int Returns a file descriptor to the open device.
 */
int open_device (void);

/** @brief wraps the original ums pthread routine for a worker thread.
 *
 *  This function is used to wrap the original ums_worker_create start_routine
 *  to correctly populate the needed data structures and to set the thread into
 *  an IDLE state, ready to be scheduled.
 *
 *  @param arg: A wrapper argument (of type struct ums_worker_arg) to the original *arg.
 */
void *worker_wrap_routine (void *arg);

/** TODO
 */
void *sched_wrap_routine (void *arg);

/** @brief Finds the first occurence of a bit set to 0 in a bitmap.
 *  @param map: The target bitmap.
 *  @param size: The size of the bitmap
 *  @return int: The index (starting from 0) of the wanted bit or -1 if none were found.
 */
int find_next_zero_bit(unsigned long *map, int size);

#endif // !LIB_UTILS_H
