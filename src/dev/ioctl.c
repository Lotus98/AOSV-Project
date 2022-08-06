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
#include "shared.h"
#include "ioctl.h"
#include "utils.h"

long ums_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
        pid_t pid;

        switch (cmd) {
        case SET_WORKER_IDLE:
                __set_current_state(TASK_IDLE);
                schedule();
                break;
        case REGISTER_PROC:
                pid = current->tgid;
                if (register_ums_process(pid) != SUCCESS) {
                        pr_err(LOG_MSG "Couldn't register process PID: %d\n", pid);
                        return FAILURE;
                }
                break;
        default:
                return -EINVAL;
        }

        return SUCCESS;
}
