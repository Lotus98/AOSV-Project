/** @file utils.c
 *  @brief Helper functions and data structures used by LKM.
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#include "utils.h"
#include "asm-generic/errno-base.h"
#include "linux/fs.h"
#include "linux/gfp.h"
#include "linux/hashtable.h"
#include "linux/kernel.h"
#include "linux/kref.h"
#include "linux/pid.h"
#include "linux/sched.h"
#include "linux/types.h"
#include "shared.h"
#include <linux/slab.h>

long register_ums_process (pid_t pid)
{
        struct ums_proc *process;

        // Look if process is already registered
        process = find_ums_proc(pid);
        if (process)
                return -EEXIST;

        // Initialize struct ums_proc
        process = kmalloc(sizeof(*process), GFP_KERNEL);
        if (!process) {
                pr_err(LOG_MSG "Couldn't allocate struct ums_proc for PID: %d\n", pid);
                return -ENOMEM;
        }
        process->pid = pid;
        // Allocate array of schedulers
        process->schedulers = kcalloc(ncpus, sizeof(struct ums_sched *), GFP_KERNEL);
        if (!process->schedulers) {
                pr_err(LOG_MSG "Couldn't allocate array of schedulers for PID: %d\n", pid);
                kfree(process);
                return -ENOMEM;
        }
        hash_init(process->workers);
        rwlock_init(&process->hash_lock);

        // Insert node in the processes hashtable
        hash_add(ums_procs, &process->node, process->pid);

        return SUCCESS;
}

long unregister_ums_process (pid_t pid)
{
        struct ums_proc *process;
        ums_worker_node_t *worker_node;
        struct hlist_node *tmp;
        int bkt;

        // Find the struct ums_proc of the PID
        process = find_ums_proc(pid);
        if (!process) // The process is not registered
                return -ESRCH;

        // Remove node from the hashtable
        hash_del(&process->node);

        // For each scheduler: empty the worker hashtable, then delete the scheduler
        // This routine is executed before exiting so we are taking for granted every
        // thread has already terminated.
        for (int i = 0; i < ncpus; i++) {
                struct ums_sched *sched = process->schedulers[i];
                if (!sched) // There is no scheduler registered for that CPU.
                        continue;
                hash_for_each_safe(sched->worker_list, bkt, tmp, worker_node, node) {
                        kref_put(&worker_node->worker->refcnt, worker_release);
                        hash_del(&worker_node->node);
                        kfree(worker_node);
                }
                // Free scheduler data struct.
                kfree(sched);
        }

        // Clear the worker hashtable saved in the ums_proc struct (no need to lock, this is the last thread)
        hash_for_each_safe(process->workers, bkt, tmp, worker_node, node) {
                kref_put(&worker_node->worker->refcnt, worker_release); // This is the one freeing the ums_worker
                hash_del(&worker_node->node);
                kfree(worker_node);
        }
        kfree(process);

        return SUCCESS;
}

long init_worker_node (void)
{
        struct task_struct *task;
        struct ums_proc *process;
        struct ums_worker *worker;
        ums_worker_node_t *worker_node;
        pid_t pid, tid;

        tid = current->pid;
        pid = current->tgid;
        // Get the corresponding struct ums_proc
        process = find_ums_proc(pid);
        if (!process) // The process was not Initialized.
                return -ESRCH;

        // Initialize struct ums_worker
        worker = kmalloc(sizeof(*worker), GFP_KERNEL);
        if (!worker)
                return -ENOMEM;
        task = pid_task(find_vpid(tid), PIDTYPE_PID);
        worker->task = task;
        worker->state = UMS_WORKER_IDLE;
        kref_init(&worker->refcnt);

        // Initialize worker node
        worker_node = kmalloc(sizeof(*worker_node), GFP_KERNEL);
        if (!worker_node)
                return -ENOMEM;
        worker_node->worker = worker;
        worker_node->tid = tid;

        /* Get the write lock to insert worker node. Doubtfully an interrupt will
         * need access to the hashtable so we can lock with interrupts enabled
         * without fear of a deadlock.
         */
        write_lock(&process->hash_lock);
        hash_add(process->workers, &worker_node->node, tid);
        write_unlock(&process->hash_lock);

        return SUCCESS;
}

struct ums_proc *find_ums_proc (pid_t pid)
{
        struct ums_proc *process;

        // Find the corresponding structure
        hash_for_each_possible(ums_procs, process, node, pid) {
                if (process->pid == pid)
                        break;
        }
        if (!process) // The process is not registered
                return NULL;

        return process;
}

void worker_release (struct kref *refcnt)
{
        struct ums_worker *worker = container_of(refcnt, struct ums_worker, refcnt);
        kfree(worker);

        return;
}
