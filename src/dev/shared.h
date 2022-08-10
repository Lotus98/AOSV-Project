/**
 *  @file shared.h
 *  @brief Header file containing shared data between most LKM files.
 *
 *  This file contains the informations and data needed by all the files of the LKM.
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#ifndef DEV_SHARED_H
#define DEV_SHARED_H

#include <asm-generic/errno.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/hashtable.h>
#include <linux/kref.h>
#include <linux/types.h>
#include <linux/rwlock_types.h>
#include <linux/cpumask.h>


#define DEVICE_NAME "umsdev" ///< The device name in "/dev".
#define LOG_MSG "umsdev: " ///< Log message to identify driver's messages in dmesg.

#define SUCCESS 0
#define FAILURE -1

#define HBITS 8 ///< "bits" used by a general hashtable (8 = 256 entries).

// Worker states
#define UMS_WORKER_RUNNING      0
#define UMS_WORKER_IDLE         1

// #ifndef PRINTDBG
#define PRINTDBG(fmt, args...) pr_debug(LOG_MSG fmt, ##args)
// #endif // !PRINTDBG


// Data structures
/// Defines the components that identify a process using UMS.
struct ums_proc {
        pid_t pid; ///< The process PID.
        struct ums_sched **schedulers; ///< An array of pointers representing all the active scheduler threads.
        struct hlist_node node; ///< Node for the bucket in the processes hashtable.
        /*  An hashtable containing all the workers registered by a process.
         *  This is used to allow faster and more efficient lookup of shared workers.
         *  Naturally, this is less memory efficient, but not so bad, since it means
         *  we are just holding a double copy of a ums_worker_node_t which is relatively
         *  small.
         */
        DECLARE_HASHTABLE(workers, HBITS); ///< Hashtable of all workers registered by a process.
        rwlock_t hash_lock; ///< lock used to access the workers hashtable.
};

/// Defines a scheduler thread.
struct ums_sched {
        struct task_struct *sched_task; ///< The task_struct of the scheduler thread.
        struct ums_worker *current_worker; ///< The current worker.
        DECLARE_HASHTABLE(worker_list, HBITS); ///< The completion list (implemented as an hashtable).
        rwlock_t lock; ///< Lock for the hashtable.
};

/// Defines a worker thread.
struct ums_worker {
        struct task_struct *task; ///< The task_struct of the thread.
        unsigned long state; ///< The current executing state of the worker.
        // Maybe in future save the value of tid here to make it quicker to access.
        struct kref refcnt; ///< Reference counter for the worker.
};

typedef struct ums_worker_node {
        struct ums_worker *worker; ///< We use a pointer to abstract.
        struct hlist_node node; ///< node of the hashtable's bucket.
} ums_worker_node_t;

// Global variables
extern DECLARE_HASHTABLE(ums_procs, HBITS); ///< Hashtable to keep all the processes that are in UMS mode.
extern int ncpus; ///< The number of online CPUs in the system.


#endif // !DEV_SHARED_H
