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
#include <stdbool.h>
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
#define INIT_WORKER _IO(0x1337, 'a') ///< Set state of the calling thread to TASK_IDLE.
#define REGISTER_PROC _IO(0x1337, 'b') ///< Register a process to be in UMS mode.
#define UNREGISTER_PROC _IO(0x1337, 'c') ///< Unregister a process that is in UMS mode.


// Data structures needed by the lib
/// Structure defining a UMS specific thread.
struct ums_thread {
        pthread_t pthread; ///< The related pthread.
        pid_t tid; ///< The TID of the created thread.
};

/// Structure used to wrap the arguments of pthread_create within ums_worker_create.
struct ums_worker_arg {
        /// The reference to the struct ums_thread corresponding to the worker.
        struct ums_thread *ums_thread;
        void *(*ums_routine) (void *); ///< The routine passed to ums_worker_create.
        void *arg; ///< The argument passed to ums_worker_create.
        sem_t *tid_sem; ///< Semaphore used to coordinate ums_thread->tid population.
};

struct ums_sched_arg {
        struct ums_thread *ums_thread;
        void (*sched_routine) (void); ///< The scheduler function.
        int cpuid; ///< The CPU to which the thread will be bound.
        sem_t *sem; ///< Semaphore used to coordinate the main thread with the scheduler thread.
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

/// Structure to define a scheduler thread and its context.
struct ums_sched {
        struct ums_thread *ums_thread; ///< The ums_thread related to the scheduler.
        int cpuid; ///< The id of the assigned CPU.
        ums_list_head_t *worker_list;
};

// Global variables
int dev_fd; ///< The file descriptor of "/dev/umsdev"
int ncpus; ///< The number of available CPUs in the system
bitmap_t cpus_map; ///< A bitmap representing the state of the CPUs in the UMS context of this process.
struct ums_sched **ums_schedulers; ///< An array of pointers to the schedulers in use.

#endif // !LIB_SHARED_H
