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
#include <semaphore.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include "bitmap.h"
#include "list.h"


// Defines
#define SUCCESS 0
#define FAILURE -1
#define DRIVER_PATH "/dev/umsdev" ///< File path of the IOCTL device.

#undef PRINTDBG
#ifdef DEBUG
#define PRINTDBG(...) fprintf(stderr, "[DEBUG]" __VA_ARGS__)
#else
#define PRINTDBG(...) do {} while(0)
#endif // DEBUG


// IOCTL commands
#define SET_WORKER_IDLE _IO(0x1337, 'a') ///< Set state of the calling thread to TASK_IDLE.


// Data structures needed by the lib
/// Structure defining a UMS specific thread.
struct ums_thread {
        pthread_t pthread; ///< The related pthread.
        pid_t tid; ///< The TID of the created thread.
};

/// Structure used to wrap the arguments of pthread_create within ums_worker_create.
struct ums_arg {
        /// The reference to the struct ums_thread corresponding to the worker.
        struct ums_thread *ums_thread;
        void *(*ums_routine) (void *); ///< The routine passed to ums_worker_create.
        void *arg; ///< The argument passed to ums_worker_create.
        sem_t *tid_sem; ///< Semaphore used to coordinate ums_thread->tid population.
};

/// Structure defining a worker.
struct ums_worker {
        struct ums_thread thread; ///< Corresponding thread.
        int refcnt; ///< Reference counter, used to keep track of how many lists contain this worker.
        pthread_rwlock_t rwlock;
};

typedef struct ums_worker_node {
        struct ums_worker *worker; ///< Worker assigned to the node.
        struct list_head list; ///< struct list pointers.
} ums_worker_node_t;

/// Structure used to keep track of the head of a worker list.
typedef struct ums_list_head {
        pthread_rwlock_t rwlock;
        struct list_head list;
} ums_list_head_t;

// Global variables
int driver_fd;
int nprocs;
bitmap_t ums_procs;

#endif // !LIB_SHARED_H
