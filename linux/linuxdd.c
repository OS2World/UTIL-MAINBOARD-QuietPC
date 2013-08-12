/* $Id: linuxdd.c,v 1.5 2007-01-30 11:48:33 andrew_belov Exp $ */

/* Linux physical driver framework */

#include <linux/version.h>
#define _LOOSE_KERNEL_NAMES
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/file.h>
#include <asm/uaccess.h>

#include <linux/mm.h>
#include <linux/kernel_stat.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#include <quietpc.h> 
#include <quietpcp.h>
#include <hwmon.h>
#define RC_BUG                     1

#define TIMER_GRANULARITY    (HZ/10)

MODULE_DESCRIPTION("Smart fan control");
MODULE_AUTHOR("Andrew Belov");
MODULE_LICENSE("GPL and additional rights"); /* This used to be public-domain anyway so why bother? */

static void linux_tick_handler(unsigned long dummy);

/* Event interface */

struct timer_list tl=TIMER_INITIALIZER(linux_tick_handler, 0, 0);

/* The Linux Tick Handler */

static void linux_tick_handler(unsigned long dummy)
{
 tick_handler();
 del_timer(&tl);
 tl.expires=jiffies+TIMER_GRANULARITY;
 add_timer(&tl);
}

/* Installs the handler */

int install_tick_handler()
{
 tl.expires=jiffies+TIMER_GRANULARITY;
 add_timer(&tl);
 return(0);
}

/*
 * Device communication functions
 */

/* open() */

static int hwmon_open(struct inode *inode, struct file *file)
{
 return(0);
}

/* close() */

static int hwmon_release(struct inode *inode, struct file *file)
{
 return(0);
}

/* ioctl() */

static int hwmon_ioctl_router(struct inode *inode, struct file *file,
                              unsigned int func, unsigned long param)
{
 char xferbuf[1024];
 int rc;
 struct ioctl_encaps prm;
 void *uptr, *pptr, *dptr;

 uptr=(void *)param;
 if(copy_from_user(&prm, uptr, sizeof(prm)))
  return(RC_BUG);
 if(prm.parm_len+prm.data_len>sizeof(xferbuf))
  return(RC_BUG);
 pptr=xferbuf;
 dptr=pptr+prm.parm_len;
 if(prm.parm_len>0)
 {
  if(copy_from_user(pptr, prm.pparm, prm.parm_len))
   return(RC_BUG);
 }
 if(prm.data_len>0)
 {
  if(copy_from_user(dptr, prm.pdata, prm.data_len))
   return(RC_BUG);
 }
 rc=hwmon_ioctl(func, pptr, prm.parm_len, dptr, prm.data_len);
 if(prm.data_len>0)
 {
  if(copy_to_user(prm.pdata, dptr, prm.data_len))
   return(RC_BUG);
 }
 return(rc);
}

/* File operations */

struct file_operations quietpc_fops=
{
 .owner = THIS_MODULE,
 .ioctl = hwmon_ioctl_router,
 .open = hwmon_open,
 .release = hwmon_release,
};

static struct class *quietpc_class;     /* For registration with udev */

/* Startup */

int __init qinit_module()
{
 int rc;

 if((rc=configure_hwmon()))
 {
  printk("QuietPC: failed to configure hardware, rc=%d\n", rc);
  return(rc);
 }
 if((rc=register_chrdev(MAJOR_NUM, DEVICE_NAME, &quietpc_fops))<0)
 {
  printk("QuietPC: character device creation failed, rc=%d\n", rc);
  shutdown_hwmon();
  return(rc);
 }
 quietpc_class=class_create(THIS_MODULE, DEVICE_NAME);
 class_device_create(quietpc_class, NULL, MKDEV(MAJOR_NUM, 0), NULL, DEVICE_NAME);
 return(0);
}

/* Shutdown */

void cleanup_module()
{
 class_device_destroy(quietpc_class, MKDEV(MAJOR_NUM, 0));
 class_destroy(quietpc_class);
 unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
 shutdown_hwmon();
 del_timer(&tl);
}

fs_initcall(qinit_module);
