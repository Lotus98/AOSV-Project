/**
 *  @file ioctl.h
 *  @brief Header file for the ioctl interface.
 *
 *  This file contains the informations, macros, prototypes, global variables
 *  and structures needed to define the ioctl interface.
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#ifndef DEV_IOCTL_H
#define DEV_IOCTL_H

#include <linux/file.h>
#include <linux/ioctl.h>
#include <linux/sched.h>

/** Command to set a thread to state IDLE, this will avoid the kernel to schedule it.
 */
#define SET_WORKER_IDLE _IO(0x1337, 'a')

/** IOCTL implementation to handle interaction with the LKM.
 *
 *  @param file: the file associated with the device fd
 *  @param cmd: the command to be executed
 *  @param arg: a potential argument needed to execute the wanted command
 *  @return the result of the given command
 */
long ums_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

#endif // !DEV_IOCTL_H
