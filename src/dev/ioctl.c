/**
 *  @file ioctl.c
 *  @brief Ioctl commands to interact with the device.
 *
 *  This file contains all the functions corresponding to the commands used to
 *  interact with the device through the ioctl interface.
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#include "asm-generic/errno-base.h"
#include "asm/current.h"
#include "linux/types.h"
#include "linux/uaccess.h"
#include "shared.h"
#include "ioctl.h"
#include "utils.h"

long ums_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
        pid_t pid, tid;
        unsigned int cpuid;
        long retval;

        switch (cmd) {
        case INIT_WORKER:
                // Initialize worker for UMS process.
                retval = init_worker_node();
                if ( retval != SUCCESS) {
                        tid = current->pid;
                        pr_err(LOG_MSG "Couldn't Initialize worker with TID: %d\n", tid);
                        return retval;
                }
                // Change state of worker task so it is not scheduled by the kernel
                PRINTDBG("Going to sleep, from worker[TID]: %d\n", current->pid);
                __set_current_state(TASK_IDLE);
                schedule();
                PRINTDBG("I'm awake, from worker[TID]: %d\n", current->pid);
                break;
        case REGISTER_PROC:
                pid = current->tgid;
                retval = register_ums_process(pid);
                if ( retval != SUCCESS) {
                        pr_err(LOG_MSG "Couldn't register process PID: %d\n", pid);
                        return retval;
                }
                break;
        case UNREGISTER_PROC:
                pid = current->tgid;
                retval = unregister_ums_process(pid);
                if ( retval != SUCCESS) {
                        pr_err(LOG_MSG "Couldn't unregister process PID: %d\n", pid);
                        return retval;
                }
                break;
        case REGISTER_SCHED:
                if (copy_from_user(&cpuid, (unsigned int *)arg, sizeof(cpuid)) != 0) {
                        pr_err(LOG_MSG "Error in copy_from_user copying CPUID for scheduler in process: %d\n", current->tgid);
                        return -EFAULT;
                }
                retval = register_ums_scheduler(cpuid);
                break;
        default:
                return -EINVAL;
        }

        return SUCCESS;
}
