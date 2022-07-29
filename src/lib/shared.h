/** @file shared.h
 *  @brief Contains data shared among most of the files of the UMS library.
 *
 *  This file contains the headers, macros, data structures and global
 *  variables that need to be shared among all files.
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#ifndef LIB_SHARED_H
#define LIB_SHARED_H

/// @cond OMIT
#define _GNU_SOURCE
/// @endcond

// Includes
#include <asm-generic/errno-base.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>


// Defines
#define SUCCESS 0
#define FAILURE -1
#define DRIVER_PATH "/dev/umsdev" ///< File path of the IOCTL device.

#undef PRINTDBG
#ifdef DEBUG
#define PRINTDBG(fmt, args...) fprintf(stderr, fmt "\n", ##args);
#else
#define PRINTDBG(fmt, ...)
#endif // DEBUG


// IOCTL commands
#define SET_WORKER_IDLE _IO(0x1337, 'a') ///< Set state of the calling thread to TASK_IDLE


// Data structures needed by the lib
/// Structure defining a UMS specific thread.
struct ums_thread {
        pthread_t pthread; ///< The related pthread.
        pid_t tid; ///< The TID of the created thread.
};

/// Structure used to wrap the arguments of pthread_create within ums_thread_create
struct ums_arg {
        /// The reference to the struct ums_thread corresponding to the thread itself
        struct ums_thread *ums_thread;
        void *(*ums_routine) (void *); ///< The routine passed to ums_thread_create
        void *arg; ///< The argument passed to ums_thread_create
};

// Global variables
int driver_fd;

#endif // !LIB_SHARED_H
