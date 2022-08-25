/** @file utils.c
 *  @brief Helper functions and data structures used by LKM.
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#include "utils.h"
#include "asm-generic/errno-base.h"
#include "asm-generic/errno.h"
#include "asm/current.h"
#include "asm/processor.h"
#include "asm/ptrace.h"
#include "asm/string_64.h"
#include "linux/fs.h"
#include "linux/gfp.h"
#include "linux/hashtable.h"
#include "linux/kernel.h"
#include "linux/kref.h"
#include "linux/list.h"
#include "linux/pid.h"
#include "linux/printk.h"
#include "linux/sched.h"
#include "linux/spinlock.h"
#include "linux/stddef.h"
#include "linux/types.h"
#include "linux/wait.h"
#include "shared.h"
#include <linux/slab.h>
#include <linux/sched/task.h>
#include <linux/sched/task_stack.h>

static struct ums_proc *find_ums_proc (pid_t pid)
{
        // /** @brief Finds a UMS process given its PID.
        //  *  @param pid: The pid of the process that holds resources (TGID).
        //  *  @return struct ums_proc: The wanted ums_proc struct.
        //  *  @return NULL: If the process is not in the UMS processes hashtable.
        //  */
        // struct ums_proc *find_ums_proc (pid_t pid);

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

static struct ums_worker *get_worker (struct ums_proc *process, pid_t tid) {
        // IMPORTANT: This function increments also the worker's refcnt.
        struct ums_worker *worker;
        ums_worker_node_t *worker_node;

        // Get read lock on worker list.
        read_lock(&process->hash_lock);
        hash_for_each_possible(process->workers, worker_node, node, tid) {
                if (worker_node->tid == tid)
                        break;
        }
        // Release read lock worker list
        read_unlock(&process->hash_lock);
        if (!worker_node)
                return NULL;

        kref_get(&worker_node->worker->refcnt);
        worker = worker_node->worker;

        return worker;
}

static bool wait_queue_check ()
{
        struct ums_proc *process;
        struct ums_sched *sched;
        int bkt;
        bool terminated = true;
        ums_worker_node_t *worker_node;

        // Get data structs
        process = find_ums_proc(current->tgid);
        sched = process->schedulers[current->cpu];

        // Check if there are idle workers
        hash_for_each(sched->worker_list, bkt, worker_node, node) {
                read_lock(&worker_node->worker->rwlock);
                if (worker_node->worker->state == WORKER_IDLE) {
                        read_unlock(&worker_node->worker->rwlock);
                        return true;
                }
                if (worker_node->worker->state == WORKER_RUNNING)
                        terminated = false;
                read_unlock(&worker_node->worker->rwlock);
        }
        if (terminated)
                return true;
        else
                return false;
}

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

        // Initialize wait queue
        init_waitqueue_head(&process->wq);

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
        PRINTDBG("Finding the ums_proc struct for process[PID]: %d", pid);
        process = find_ums_proc(pid);
        if (!process) // The process is not registered
                return -ESRCH;

        PRINTDBG("Found ums_proc process: %d", process->pid);
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
        PRINTDBG("Preparing to remove and wakeup all workers");
        hash_for_each_safe(process->workers, bkt, tmp, worker_node, node) {
                /* TEST (This code is used to test workers creation)*/
                // PRINTDBG("Waking up worker[TID]: %d\n", worker_node->tid);
                if (worker_node->worker->state != WORKER_TERMINATED) {
                        wake_up_process(worker_node->worker->task);
                }
                /* END TEST */
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
        task = current;
        worker->task = task;
        worker->state = WORKER_IDLE;
        rwlock_init(&worker->rwlock);
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

long register_ums_scheduler(unsigned int cpuid)
{
        pid_t pid;
        struct ums_proc *process;
        struct ums_sched *sched;

        pid = current->tgid;
        process = find_ums_proc(pid);

        // Initialize ums_sched scheduler data.
        sched = kmalloc(sizeof(*sched), GFP_KERNEL);
        if (!sched) {
                pr_err("Couldn't allocate memory for scheduler struct");
                return -ENOMEM;
        }
        sched->sched_task = current;
        sched->current_worker = NULL;
        rwlock_init(&sched->lock);

        // Insert scheduler in struct ums_proc.
        if (process->schedulers[cpuid]) {
                pr_err("Error scheduler already registered on given CPUID");
                return -EEXIST;
        }

        process->schedulers[cpuid] = sched;

        return SUCCESS;
}

long register_usr_worker (struct ums_usr_worker *usr_worker)
{
        /*  This function does not need to acquire the lock for the worker, since we are
        *  neither changing its state, nor reading it.
        */
        struct ums_proc *process;
        struct ums_sched *scheduler;
        struct ums_worker *worker;
        ums_worker_node_t *worker_node;

        // Allocate worker node
        worker_node = kmalloc(sizeof(*worker_node), GFP_KERNEL);
        if (!worker_node)
                return -ENOMEM;

        // Find corresponding process
        process = find_ums_proc(current->tgid);
        if (!process) {
                kfree(worker_node);
                return -ESRCH;
        }

        // Find the wanted worker (Remember this already performs a kref_get)
        worker = get_worker(process, usr_worker->tid);
        if (!worker) {
                kfree(worker_node);
                return -ENODATA;
        }

        // Get the corresponding scheduler.
        scheduler = process->schedulers[usr_worker->cpuid];
        if (!scheduler) {
                kfree(worker_node);
                kref_put(&worker->refcnt, worker_release); // This needs to be done or we will never free the worker.
                return -ENODATA;
        }

        // Initialize worker_node
        worker_node->worker = worker;
        worker_node->tid = usr_worker->tid;

        // Get write lock on scheduler's worker list.
        write_lock(&scheduler->lock);
        hash_add(scheduler->worker_list, &worker_node->node, worker_node->tid);
        // Release write lock on scheduler's worker list.
        write_unlock(&scheduler->lock);

        return SUCCESS;
}

long execute_thread (pid_t tid)
{
        unsigned int cpuid = current->cpu;
        struct ums_proc *process;
        struct ums_sched *scheduler;
        ums_worker_node_t *worker_node;

        process = find_ums_proc(current->tgid);
        scheduler = process->schedulers[cpuid];
        // Take a read lock on the scheduler's hashtable
        read_lock(&scheduler->lock);
        hash_for_each_possible(scheduler->worker_list, worker_node, node, tid) {
                if (worker_node->tid == tid)
                        break;
        }
        if (!worker_node)
                return -ENODATA;
        // Release worker list lock
        read_unlock(&scheduler->lock);

        // Acquire spinlock on worker.
        write_lock(&worker_node->worker->rwlock);
        if (worker_node->worker->state != WORKER_IDLE) {
                write_unlock(&worker_node->worker->rwlock);
                return -EBUSY;
        }
        // Change state
        worker_node->worker->state = WORKER_RUNNING;
        // Release worker lock.
        write_unlock(&worker_node->worker->rwlock);
        // update scheduler's current_worker.
        scheduler->current_worker = worker_node->worker;

        // Save scheduler's context.
        memcpy(&scheduler->sched_regs, task_pt_regs(scheduler->sched_task), sizeof(struct pt_regs));
        // Perform context switch.
        memcpy(task_pt_regs(scheduler->sched_task), task_pt_regs(worker_node->worker->task), sizeof(struct pt_regs));

        return SUCCESS;
}

void thread_yield ()
{
        unsigned int cpuid = current->cpu;
        struct ums_proc *process;
        struct ums_sched *scheduler;
        struct ums_worker *worker;

        process = find_ums_proc(current->tgid);
        scheduler = process->schedulers[cpuid];
        worker = scheduler->current_worker;
        // update scheduler's current_worker.
        scheduler->current_worker = NULL;

        // Save worker's context.
        memcpy(task_pt_regs(worker->task), task_pt_regs(scheduler->sched_task), sizeof(struct pt_regs));
        // Perform context switch.
        memcpy(task_pt_regs(scheduler->sched_task), &scheduler->sched_regs, sizeof(struct pt_regs));

        // Acquire spinlock on worker.
        write_lock(&worker->rwlock);
        // Change state
        worker->state = WORKER_IDLE;
        // Release worker lock.
        write_unlock(&worker->rwlock);

        // Wake up process wait queue.
        wake_up_all(&process->wq);

        return;
}

void dequeue_list (unsigned int *tid_list)
{
        struct ums_proc *process;
        struct ums_sched *scheduler;
        int bkt;
        bool terminated;
        ums_worker_node_t *worker_node;

        process = find_ums_proc(current->tgid);
        scheduler = process->schedulers[current->cpu];
        tid_list[0] = 0; // The number of available workers
        do {
                // Check if it is needed to sleep
                wait_event(process->wq, wait_queue_check());
                // All workers might be terminated so set terminated to true
                terminated = true;

                // Start populating tid_list
                hash_for_each(scheduler->worker_list, bkt, worker_node, node) {
                        read_lock(&worker_node->worker->rwlock);
                        if (worker_node->worker->state == WORKER_RUNNING)
                                terminated = false;
                        else if (worker_node->worker->state == WORKER_IDLE) {
                                tid_list[++tid_list[0]] = worker_node->tid;
                        }
                        read_unlock(&worker_node->worker->rwlock);
                }

        } while (!terminated && (tid_list[0] == 0) );

        return;
}

void terminate_worker (void)
{
        /*  This function is exactly the same as thread_yield() but changes the
         *  worker's state to WORKER_TERMINATED and wakes up the process.
         */
        unsigned int cpuid = current->cpu;
        struct ums_proc *process;
        struct ums_sched *scheduler;
        struct ums_worker *worker;

        process = find_ums_proc(current->tgid);
        scheduler = process->schedulers[cpuid];
        worker = scheduler->current_worker;
        // update scheduler's current_worker.
        scheduler->current_worker = NULL;

        // Save worker's context.
        memcpy(task_pt_regs(worker->task), task_pt_regs(scheduler->sched_task), sizeof(struct pt_regs));
        // Perform context switch.
        memcpy(task_pt_regs(scheduler->sched_task), &scheduler->sched_regs, sizeof(struct pt_regs));

        // Acquire spinlock on worker.
        write_lock(&worker->rwlock);
        // Change state
        worker->state = WORKER_TERMINATED;
        // Release worker lock.
        write_unlock(&worker->rwlock);

        // Wake up process wait queue.
        wake_up_all(&process->wq);

        // Wake up thread process.
        wake_up_process(worker->task);

        return;
}

void worker_release (struct kref *refcnt)
{
        struct ums_worker *worker = container_of(refcnt, struct ums_worker, refcnt);
        kfree(worker);

        return;
}
