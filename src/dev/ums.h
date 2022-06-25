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

#define DEVICE_NAME "umsdev"
#define LOG_MSG "ums: "

#undef PRINTDBG
#ifdef DEBUG
        #define PRINTDBG(fmt, args...) pr_debug(LOG_MSG fmt, ## args)
#else
        #define PRINTDBG(fmt, args...)
#endif // Enable debug prints if DEBUG passed as compilation variable
