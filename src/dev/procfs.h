/**
 *  @file procfs.h
 *  @brief Header file for the ProcFS interface of the UMS driver.
 *
 *  This file contains the prototypes of the functions needed to interact with the ProcFS.
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#ifndef DEV_PROCFS_H
#define DEV_PROCFS_H

#include <linux/proc_fs.h>

#define LINUX_KERNEL_VERSION 510 ///< Defines the version of the kernel in use (5.10.122).


// Prototypes
/** @brief Initializes the procfs data for UMS LKM.
 */
struct proc_dir_entry *init_procfs (void);

/** @brief Prints the data corresponding to the target scheduler.
 *
 *  This functions retrieves the data of the scheduler binded to the file of
 *  the type: "/proc/ums/<PID>/schedulers/<ID>/info" and copies it to userspace.
 */
ssize_t sched_proc_read (struct file *filp, char __user *buffer, size_t length, loff_t *offset);

/** @brief Prints the data corresponding to the target worker.
 *
 *  This functions retrieves the data of the worker binded to the file of the type:
 *  "/proc/ums/<PID>/schedulers/<ID>/workers/<TID>" and copies it to userspace.
 */
ssize_t worker_proc_read (struct file *filp, char __user *buffer, size_t length, loff_t *offset);

// Global variables
/// The proc_dir_entry corresponding to the UMS dir entry, which is: "/proc/ums/"
extern struct proc_dir_entry *procfs_base_dir;

#endif // !DEV_PROCFS_H
