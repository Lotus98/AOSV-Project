/**
 *  @file procfs.c
 *  @brief File containing the functions and definitions needed to instantiate the procfs UMS data.
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#include <linux/proc_fs.h>
#include "asm-generic/errno-base.h"
#include "asm/string_64.h"
#include "linux/gfp.h"
#include "linux/hashtable.h"
#include "linux/kernel.h"
#include "linux/list.h"
#include "linux/slab.h"
#include "linux/types.h"
#include "linux/uaccess.h"
#include "procfs.h"
#include "shared.h"
#include "utils.h"

struct proc_dir_entry *procfs_base_dir;

static char *worker_state_to_str (enum state state)
{
        switch (state) {
                case WORKER_RUNNING:
                        return "Running";
                case WORKER_IDLE:
                        return "Idle";
                case WORKER_TERMINATED:
                        return "Terminated";
                default:
                        return NULL;
        }
}

struct proc_dir_entry *init_procfs(void)
{
        struct proc_dir_entry *dir_entry;

        dir_entry = proc_mkdir("ums", NULL);

        return dir_entry;
}

ssize_t sched_proc_read (struct file *filp, char __user *buffer, size_t length, loff_t *offset)
{
        char *out_buf;
        struct ums_proc *process;
        unsigned int cpuid;
        struct ums_sched *scheduler;
        struct ums_worker *worker;
        ssize_t buf_len, leftover;
        pid_t pid;

        if (*offset < 0) {
                return -EINVAL;
        }

        // Retrieve pid of process
        if (kstrtoint(filp->f_path.dentry->d_parent->d_parent->d_parent->d_iname, 10, &pid) != 0)
                return FAILURE;
        PRINTDBG("[PROCFS] Pid: %d\n", pid);
        // Retrieve tid of worker
        if (kstrtouint(filp->f_path.dentry->d_parent->d_iname, 10, &cpuid) != 0)
                return FAILURE;
        PRINTDBG("[PROCFS] Cpuid: %d\n", cpuid);

        // Find the right process
        process = find_ums_proc(pid);
        PRINTDBG("Found process: %d\n", process->pid);
        // Find the right scheduler
        scheduler = process->schedulers[cpuid];

        worker = scheduler->current_worker;
        if (!worker) {
                out_buf = kasprintf(GFP_KERNEL, "Total switches:        \t%lu\n"
                                                "Time last switch:      \t%lldns\n"
                                                "State:                 \tIdle\n",
                                                scheduler->sched_data->num_switches,
                                                scheduler->sched_data->last_switch);

        } else {
                out_buf = kasprintf(GFP_KERNEL, "Total switches:        \t%lu\n"
                                                "Time last switch:      \t%lldns\n"
                                                "State:                 \tRunning\n"
                                                "Worker running:        \t%d\n",
                                                scheduler->sched_data->num_switches,
                                                scheduler->sched_data->last_switch,
                                                worker->task->pid);
        }
        PRINTDBG("Out buf ready: %s\n", out_buf);

        buf_len = strlen(out_buf);
        if (*offset >= buf_len || length == 0 || buf_len == 0) {
                return 0;
        }
        if (buf_len > length) {
                buf_len = length;
        }
        PRINTDBG("Copying to user\n");
        leftover = copy_to_user(buffer, out_buf, buf_len);
        PRINTDBG("Done copying to user\n");
        *offset += buf_len + leftover;
        kfree(out_buf);

        return buf_len - leftover;
}

ssize_t worker_proc_read (struct file *filp, char __user *buffer, size_t length, loff_t *offset)
{
        char *out_buf;
        struct ums_proc *process;
        ums_worker_node_t *worker_node;
        struct ums_worker *worker;
        ssize_t buf_len, leftover;
        pid_t pid;
        pid_t tid;

        if (*offset < 0) {
                return -EINVAL;
        }
        // Retrieve pid of process
        if (kstrtoint(filp->f_path.dentry->d_parent->d_parent->d_parent->d_parent->d_iname, 10, &pid) != 0)
                return FAILURE;
        PRINTDBG("[PROCFS] Pid: %d\n", pid);
        // Retrieve tid of worker
        if (kstrtoint(filp->f_path.dentry->d_iname, 10, &tid) != 0)
                return FAILURE;
        PRINTDBG("[PROCFS] Tid: %d\n", tid);

        // Find the right process
        hash_for_each_possible(ums_procs, process, node, pid) {
                if (process->pid == pid)
                        break;
        }

        // Find the right worker
        hash_for_each_possible(process->workers, worker_node, node, tid) {
                if (worker_node->tid == tid)
                        break;
        }
        worker = worker_node->worker;

        read_lock(&worker->rwlock);
        out_buf = kasprintf(GFP_KERNEL, "State:                 \t%s\n"
                                        "Total running time:    \t%lldns\n"
                                        "Total switches:        \t%lu\n",
                                        worker_state_to_str(worker->state),
                                        worker->worker_data->tot_running,
                                        worker->worker_data->num_switches);
        read_unlock(&worker->rwlock);

        buf_len = strlen(out_buf);
        if (*offset >= buf_len || length == 0 || buf_len == 0) {
                return 0;
        }
        if (buf_len > length) {
                buf_len = length;
        }
        leftover = copy_to_user(buffer, out_buf, buf_len);
        *offset += buf_len + leftover;
        kfree(out_buf);

        return buf_len - leftover;
}
