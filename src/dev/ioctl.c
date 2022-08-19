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
        struct ums_usr_worker usr_worker;
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
                if ( retval != SUCCESS) {
                        pr_err(LOG_MSG "Couldn't register scheduler on CPU: %d, for process PID: %d\n", cpuid, current->tgid);
                        return retval;
                }
                break;
        case REGISTER_WORKER:
                if (copy_from_user(&usr_worker, (struct ums_usr_worker *)arg, sizeof(usr_worker)) != 0) {
                        pr_err(LOG_MSG "Error in copy_from_user copying usr_worker in process: %d\n", current->tgid);
                        return -EFAULT;
                }
                retval = register_usr_worker(&usr_worker);
                if ( retval != SUCCESS) {
                        pr_err(LOG_MSG "Couldn't register worker[TID]: %d on CPU: %d, for process PID: %d\n",
                                usr_worker.tid, usr_worker.cpuid, current->tgid);
                        return retval;
                }
                break;
        case EXECUTE_THREAD:
                if (copy_from_user(&tid, (pid_t *)arg, sizeof(tid)) != 0) {
                        pr_err(LOG_MSG "Error in copy_from_user copying worker tid\n");
                        return -EFAULT;
                }
                retval = execute_thread(tid);
                if ( retval != SUCCESS) {
                        pr_err(LOG_MSG "Error executing thread[TID]: %d\n", tid);
                        return retval;
                }
                break;
        case THREAD_YIELD:
                thread_yield();
                break;
        default:
                return -EINVAL;
        }

        return SUCCESS;
}
