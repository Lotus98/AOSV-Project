/** @file ums.c
 *  @brief Initialization of LKM.
 *
 *  This file Manages the initialization of the Linux Kernel module
 *  for the UMS driver.
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#include "ums.h"

static int ums_open(struct inode *inode, struct file *filp)
{
        PRINTDBG("The device has been open");
	return 0;
}

static int ums_close(struct inode *inode, struct file *filp)
{
        PRINTDBG("The device has been close");
        return 0;
}

static long ums_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
        return 1337;
}

static int __init init_umsmodule(void)
{
        int error;

        error = misc_register(&ums_misc_dev);
        if (error < 0) {
                pr_err(LOG_MSG "Registering misc device failed\n");
                return error;
        }
        pr_info(LOG_MSG "Misc device registered successfully!\n");

        return 0;
}

static void __exit exit_umsmodule(void)
{
        misc_deregister(&ums_misc_dev);
        pr_info(KERN_INFO LOG_MSG "Exit\n");
        return;
}

module_init(init_umsmodule);
module_exit(exit_umsmodule);
