/** @file utils.c
 *  @brief Helper functions and data structures used by LKM.
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#include "utils.h"
#include "shared.h"
#include <linux/slab.h>

int register_ums_process (pid_t pid)
{
        struct ums_proc *process;

        // Initialize struct ums_proc
        process = kmalloc(sizeof(*process), GFP_KERNEL);
        if (!process) {
                pr_err(LOG_MSG "Couldn't allocate struct ums_proc for PID: %d\n", pid);
                return FAILURE;
        }
        process->pid = pid;
        process->schedulers = kcalloc(ncpus, sizeof(struct ums_sched *), GFP_KERNEL);
        if (!process->schedulers) {
                pr_err(LOG_MSG "Couldn't allocate array of schedulers for PID: %d\n", pid);
                kfree(process);
                return FAILURE;
        }
        hash_init(process->workers);
        rwlock_init(&process->hash_lock);

        // Insert node in the processes hashtable
        hash_add(ums_procs, &process->node, process->pid);

        return SUCCESS;
}
