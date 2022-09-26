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

#include <linux/hashtable.h>
#include <linux/sched.h>
#include <linux/kref.h>
#include <linux/slab.h>


#define DEVICE_NAME "umsdev" ///< The device name in "/dev".
#define LOG_MSG "umsdev: " ///< Log message to identify driver's messages in dmesg.

#define SUCCESS 0
#define FAILURE -1

#define HBITS 8 ///< "bits" used by a general hashtable (8 = 256 entries).

/// Representation of the possible states a UMS worker can be in.
enum state {WORKER_RUNNING, WORKER_IDLE, WORKER_TERMINATED, WORKER_SCHEDULED};

// #ifndef PRINTDBG
#define PRINTDBG(fmt, args...) pr_debug(LOG_MSG "CPU[%d]" fmt "\n", current->cpu, ##args)
// #endif // !PRINTDBG


// Data structures needed for the functioning of the module.
/// Structure defining the components that identify a process that is using the UMS driver.
struct ums_proc {
        struct ums_sched **schedulers; ///< An array of pointers to all the active scheduler threads.
        struct hlist_node node; ///< Node element for placing the process in the global hashtable.
        pid_t pid; ///< The process PID.
        /*  An hashtable containing all the workers registered by a process.
         *  This is used to allow faster and more efficient lookup of shared workers.
         *  Naturally, this is less memory efficient, but not so bad, since it means
         *  we are just holding a double copy of a ums_worker_node_t which is relatively
         *  small.
         */
        DECLARE_HASHTABLE(workers, HBITS); ///< Hashtable of all workers initilized by a process.
        rwlock_t hash_lock; ///< Lock used to access the workers hashtable.
        /* The waitqueue may be used by scheduler threads calling DequeueUmsCompletionListItems.
         *  In fact there might not be available workers, becuase they might be running
         *  on a different scheduler thread, therefore the call should be blocking.
         *  We achieve this by waiting for the condition on which every worker on
         *  a scheduler's list is terminated or there is an idle one.
         */
        wait_queue_head_t wq; ///< A wait queue used to implement the blocking call DequeueUmsCompletionListItems.
        struct procfs_proc_umsdata *proc_data; ///< The procfs data relative to the process.
};

/// Structure defining a scheduler thread in UMS mode.
struct ums_sched {
        struct task_struct *sched_task; ///< The task_struct of the scheduler thread.
        struct ums_worker *current_worker; ///< The worker currently running on the scheduler context, NULL if none.
        struct pt_regs sched_regs; ///< Backup of the scheduler's state, used to implement the context switch.
        DECLARE_HASHTABLE(worker_list, HBITS); ///< The completion list (implemented as an hashtable).
        rwlock_t lock; ///< Lock used to access the hashtable.
        struct procfs_sched_umsdata *sched_data; ///< The procfs data related to the scheduler.
};

/// Structure defining a worker thread.
struct ums_worker {
        struct task_struct *task; ///< The task_struct of the worker thread.
        enum state state; ///< The current state of the worker. (IDLE, TERMINATED, SCHEDULED, RUNNING)
        struct kref refcnt; ///< Reference counter for the worker.
        rwlock_t rwlock; ///< Lock used to keep coherent the state of a worker.
        struct procfs_worker_umsdata *worker_data; ///< The procfs data of the worker.
};

/// Structure used to hold a shared worker in a hashtable.
typedef struct ums_worker_node {
        struct ums_worker *worker;
        pid_t tid; ///< The PID of the worker thread. It is used as a key for the hashtable.
        struct hlist_node node; ///< Node element for placing the worker in a hashtable.
        struct proc_dir_entry *info_file; ///< The directory entry that represents the node in ProcFS.
} ums_worker_node_t;

/// Structure defining a tuple used to hold registration information about a worker.
struct ums_usr_worker {
        unsigned int cpuid; ///< The cpuid related to the scheduler on which we are registering the worker.
        pid_t tid; ///< The PID of the worker thread.
};

// Data structures needed to manage the procfs components.
/// Structure defining the data needed by a process to manage its procfs instance.
struct procfs_proc_umsdata {
        struct proc_dir_entry *proc_dir; ///< Process directory (/proc/ums/<pid>/).
        struct proc_dir_entry *scheds_dir; ///< Schedulers directory (/proc/ums/<pid>/schedulers/).
};

/// Structure defining the procfs data of a scheduler.
struct procfs_sched_umsdata {
        struct proc_dir_entry *sched_dir; ///< Scheduler's directory (/proc/ums/<pid>/schedulers/<id>/).
        struct proc_dir_entry *workers_dir; ///< Workers directory (/proc/ums/<pid>/schedulers/<id>/workers/).
        struct proc_dir_entry *info_file; ///< Scheduler's informations and statistics.
        unsigned long num_switches; ///< The total number of context switches performed.
        ktime_t last_switch; ///< The time required to perform the last context switch.
};

/// Structure defining the procfs data of a worker.
struct procfs_worker_umsdata {
        ktime_t tot_running; ///< The total running time of the worker.
        ktime_t start_time; ///< The time which the worker started running.
        unsigned long num_switches; ///< The total number of context switches performed.
};


// Global variables
extern DECLARE_HASHTABLE(ums_procs, HBITS); ///< Hashtable to keep all the processes that are registered in UMS mode.
extern int ncpus; ///< The number of online CPUs in the system.


#endif // !DEV_SHARED_H
