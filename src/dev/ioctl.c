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
#include "ioctl.h"
#include "shared.h"

long ums_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
        switch (cmd) {
        case WORKER_IDLE:
                __set_current_state(TASK_IDLE);
                schedule();
                break;
        default:
                return -EINVAL;
        }
        return 0;
}
