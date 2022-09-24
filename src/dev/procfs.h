/**
 *  @file procfs.h
 *  @brief Header file needed to manage the procfs instantiation.
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#ifndef DEV_PROCFS_H
#define DEV_PROCFS_H

#include "linux/fs.h"
#include "linux/types.h"
#include "shared.h"
#include "utils.h"

#define LINUX_KERNEL_VERSION 510 ///< Defines the version of the kernel in use (5.10.122).


// Prototypes
/** @brief Initializes the procfs data for UMS LKM.
 */
struct proc_dir_entry *init_procfs (void);

/// @brief This function is the hook for the read function of a scheduler's info file in proc_ops struct.
ssize_t sched_proc_read (struct file *filp, char __user *buffer, size_t length, loff_t *offset);

/// @brief This function is the hook for the read function of a workers's info file in proc_ops struct.
ssize_t worker_proc_read (struct file *filp, char __user *buffer, size_t length, loff_t *offset);

// Global variables
extern struct proc_dir_entry *procfs_base_dir;

#endif // !DEV_PROCFS_H
