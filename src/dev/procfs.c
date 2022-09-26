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
#include "linux/string.h"
#include "linux/time.h"
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
        char *out_buf, *tmp_buf;
        pid_t pid;
        unsigned int cpuid, bkt, cnt_worker = 0;
        struct ums_proc *process;
        struct ums_sched *scheduler;
        ums_worker_node_t *worker_node;
        struct ums_worker *worker;
        ssize_t buf_len = 0, leftover = length; // buf_len = The number of bytes written; leftover = The bytes remaining available.

        if (*offset < 0) {
                return -EINVAL;
        }

        // Allocate dynamically the size of the output buffer to be the same as the requested length.
        out_buf = kmalloc(length, GFP_KERNEL);
        *out_buf = '\0';
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
                tmp_buf = kasprintf(GFP_KERNEL, "Total switches:        \t%lu\n"
                                                "Time last switch:      \t%lldns\n"
                                                "State:                 \tIdle\n"
                                                "Completion list:       \t[ ",
                                                scheduler->sched_data->num_switches,
                                                scheduler->sched_data->last_switch);

        } else {
                tmp_buf = kasprintf(GFP_KERNEL, "Total switches:        \t%lu\n"
                                                "Time last switch:      \t%lldns\n"
                                                "State:                 \tRunning\n"
                                                "Running worker:        \t%d\n"
                                                "Completion list:       \t[ ",
                                                scheduler->sched_data->num_switches,
                                                scheduler->sched_data->last_switch,
                                                worker->task->pid);
        }

        // Copying the first part of the buffer
        buf_len = strlcat(out_buf, tmp_buf, length);
        leftover -= buf_len;
        kfree(tmp_buf);

        // Writing the completion list
        hash_for_each(scheduler->worker_list, bkt, worker_node, node) {
                if (cnt_worker == 5) {
                        cnt_worker = 0;
                        tmp_buf = kasprintf(GFP_KERNEL, "%d,\n"
                                                        "                       \t  ",
                                                        worker_node->tid);
                } else {
                        tmp_buf = kasprintf(GFP_KERNEL, "%d, ", worker_node->tid);
                }
                buf_len = strlcat(out_buf, tmp_buf, length);
                leftover -= buf_len;
                kfree(tmp_buf);
                cnt_worker++;
                if (leftover < 0)
                        break;
        }
        // Closing bracket of the completion list
        buf_len = strlcat(out_buf, " ]\n", length);

        if (*offset >= buf_len || length == 0 ) {
                return 0;
        }
        PRINTDBG("Copying to user\n");
        leftover = copy_to_user(buffer, out_buf, buf_len);
        *offset = buf_len - leftover;
        PRINTDBG("Done copying to user\n");

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
        *offset += buf_len - leftover;
        kfree(out_buf);

        return buf_len - leftover;
}
