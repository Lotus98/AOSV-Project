/** @file ums.c
 *  @brief Initialization of LKM.
 *
 *  This file Manages the initialization of the Linux Kernel module for the UMS driver.
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include "linux/hashtable.h"
#include "shared.h"
#include "ioctl.h"

static const struct file_operations ums_fops = {
        .owner = THIS_MODULE,
        .unlocked_ioctl = ums_ioctl,
};

static struct miscdevice ums_misc_dev = {
        .minor = MISC_DYNAMIC_MINOR,
        .name = DEVICE_NAME,
        .mode = S_IRUGO | S_IWUGO,
        .fops = &ums_fops,
};

static int __init init_umsmodule(void)
{
        int error;

        error = misc_register(&ums_misc_dev);
        if (error < 0) {
                pr_err(LOG_MSG "Registering misc device failed\n");
                return error;
        }
        pr_info(LOG_MSG "Misc device registered successfully!\n");

        // Get number of online CPUs.
        ncpus = num_online_cpus();

        return 0;
}

static void __exit exit_umsmodule(void)
{
        misc_deregister(&ums_misc_dev);
        pr_info(KERN_INFO LOG_MSG "Exit\n");
        return;
}

/// @cond OMIT
module_init(init_umsmodule);
module_exit(exit_umsmodule);

MODULE_AUTHOR("Nalin Dhingra <lotus98@protonmail.com>");
MODULE_DESCRIPTION("User Mode thread Scheduler (UMS) LKM");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
/// @endcond
