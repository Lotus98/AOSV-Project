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

#include "asm/ptrace.h"
#include "linux/spinlock_types.h"
#include <asm-generic/errno.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/hashtable.h>
#include <linux/kref.h>
#include <linux/types.h>
#include <linux/rwlock_types.h>
#include <linux/cpumask.h>
#include <linux/wait.h>


#define DEVICE_NAME "umsdev" ///< The device name in "/dev".
#define LOG_MSG "umsdev: " ///< Log message to identify driver's messages in dmesg.

#define SUCCESS 0
#define FAILURE -1

#define HBITS 8 ///< "bits" used by a general hashtable (8 = 256 entries).

// Worker states
enum state {WORKER_RUNNING, WORKER_IDLE, WORKER_TERMINATED};

// #ifndef PRINTDBG
#define PRINTDBG(fmt, args...) pr_debug(LOG_MSG "CPU[%d]" fmt "\n", current->cpu, ##args)
// #endif // !PRINTDBG


// Data structures
/// Defines the components that identify a process using UMS.
struct ums_proc {
        struct ums_sched **schedulers; ///< An array of pointers representing all the active scheduler threads.
        struct hlist_node node; ///< Node for the bucket in the processes hashtable.
        pid_t pid; ///< The process PID.
        /*  An hashtable containing all the workers registered by a process.
         *  This is used to allow faster and more efficient lookup of shared workers.
         *  Naturally, this is less memory efficient, but not so bad, since it means
         *  we are just holding a double copy of a ums_worker_node_t which is relatively
         *  small.
         */
        DECLARE_HASHTABLE(workers, HBITS); ///< Hashtable of all workers registered by a process.
        rwlock_t hash_lock; ///< lock used to access the workers hashtable.
        /** The waitqueue may be used by scheduler threads calling DequeueUmsCompletionListItems.
         *  In fact there might not be available workers, becuase they might be running
         *  on a different scheduler thread, therefore the call should be blocking.
         *  We achieve this by waiting for the condition on which every worker on
         *  a scheduler's list is terminated or there is an idle one.
         */
        wait_queue_head_t wq;
};

/// Defines a scheduler thread.
struct ums_sched {
        struct task_struct *sched_task; ///< The task_struct of the scheduler thread.
        struct ums_worker *current_worker; ///< The worker currently running on the scheduler context, NULL if none.
        struct pt_regs sched_regs; ///< Backup of the scheduler's state, used in the context switch.
        DECLARE_HASHTABLE(worker_list, HBITS); ///< The completion list (implemented as an hashtable).
        rwlock_t lock; ///< Lock for the hashtable.
};

/// Defines a worker thread.
struct ums_worker {
        struct task_struct *task; ///< The task_struct of the thread.
        enum state state; ///< The current state of the worker.
        bool scheduled; ///< A boolean stating if the worker is already dequeued and ready to be executed by a scheduler.
        struct kref refcnt; ///< Reference counter for the worker.
        rwlock_t rwlock; ///< Lock used to keep coherent the state of a worker.
};

/// Defines an hashtable node representing a worker.
typedef struct ums_worker_node {
        struct ums_worker *worker;
        pid_t tid; ///< The key for hashtables. Provides also quicker access to worker TID.
        struct hlist_node node; ///< The node of the hashtable's bucket.
} ums_worker_node_t;

/// Defines a tuple used to send worker thread registration informations to the LKM.
struct ums_usr_worker {
        unsigned int cpuid; ///< The cpuid related to the scheduler on which we are registering the worker.
        pid_t tid; ///< The TID of the target worker.
};


// Global variables
extern DECLARE_HASHTABLE(ums_procs, HBITS); ///< Hashtable to keep all the processes that are in UMS mode.
extern int ncpus; ///< The number of online CPUs in the system.


#endif // !DEV_SHARED_H
