/** @file ums.h
 *  @brief Informations, macros, prototypes, global variables and structures for LKM.
 *
 *  This file contains the informations regarding the UMS LKM and all the
 *  macros, prototypes, global variables and structures needed for its initialization.
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#define _GNU_SOURCE

#include "linux/kern_levels.h"
#include "linux/printk.h"
#include "linux/stat.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/ioctl.h>

MODULE_AUTHOR("Nalin Dhingra <lotus98@protonmail.com>");
MODULE_DESCRIPTION("User Mode thread Scheduler (UMS), which allows a user to manage the scheduling of threads in userspace");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

#define DEVICE_NAME "umsdev"
#define LOG_MSG "ums: "

/// @def PRINTDBG Macro used to enable debugging prints in the linux kernel
#undef PRINTDBG
#ifdef DEBUG
        #define PRINTDBG(fmt, args...) pr_debug(LOG_MSG fmt, ## args)
#else
        #define PRINTDBG(fmt, args...)
#endif

// Prototypes
static long ums_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int ums_open(struct inode *inode, struct file *filp);
static int ums_close(struct inode *inode, struct file *filp);

static const struct file_operations ums_fops = {
        .open = ums_open,
        .release = ums_close,
        .unlocked_ioctl = ums_ioctl,
};
static struct miscdevice ums_misc_dev = {
        .minor = MISC_DYNAMIC_MINOR,
        .name = DEVICE_NAME,
        .mode = S_IRUGO | S_IWUGO,
        .fops = &ums_fops,
};
