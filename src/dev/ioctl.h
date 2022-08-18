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
#include <linux/slab.h>
#include <linux/gfp.h>

/// Command to set a thread to state IDLE, this will avoid the kernel to schedule it.
#define INIT_WORKER _IO(0x1337, 'a')
/// Command to register a process to be in UMS mode.
#define REGISTER_PROC _IO(0x1337, 'b')
/// Command to unregister a process that is in UMS mode.
#define UNREGISTER_PROC _IO(0x1337, 'c')
/// Command to register a scheduler thread, takes as input the CPUID where to register the scheduler.
#define REGISTER_SCHED _IOW(0x1337, 'd', unsigned int)
/// Command to register a worker to a precise scheduler.
#define REGISTER_WORKER _IOW(0x1337, 'e', struct ums_usr_worker)
/// Command to execute a ums worker.
#define EXECUTE_THREAD _IOW(0x1337, 'f', pid_t)
/// Command to yield the calling thread.
#define THREAD_YIELD _IO(0x1337, 'g')


/** IOCTL implementation to handle interaction with the LKM.
 *
 *  @param file: the file associated with the device fd
 *  @param cmd: the command to be executed
 *  @param arg: a potential argument needed to execute the wanted command
 *  @return the result of the given command
 */
long ums_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

#endif // !DEV_IOCTL_H
