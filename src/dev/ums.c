#include "ums.h"

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

MODULE_AUTHOR("Nalin Dhingra <lotus98@protonmail.com>");
MODULE_DESCRIPTION("User Mode thread Scheduler (UMS), which allows a user to manage the scheduling of threads in userspace");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
