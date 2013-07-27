/*
 * Solid State Disk Example Driver
 *
 */


#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h> 
#include <linux/errno.h>
#include <linux/types.h> 
#include <linux/mm.h>
#include <linux/kdev_t.h>
#include <asm/page.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>

#include <linux/device.h>

#include "ssd.h"

#define MAX_SSD_DEV 7
#define MAX_LBAS_PER_SSD 1024
#define LBA_SIZE 512

static int ssd_major = 0;
module_param(ssd_major, int, 0);
MODULE_AUTHOR("Sam Siewert");
MODULE_LICENSE("GPL");

// Minor # is used to index each SSD device
static struct cdev ssdCdevs[MAX_SSD_DEV];
static struct ssd_dev ssdDevs[MAX_SSD_DEV];
static unsigned char ssdData[MAX_SSD_DEV][MAX_LBAS_PER_SSD*LBA_SIZE];

static int ssd_open (struct inode *inode, struct file *filp)
{
        int ssdIdx;

        ssdIdx=iminor(filp->f_dentry->d_inode);

        ssdDevs[ssdIdx].size = MAX_LBAS_PER_SSD*LBA_SIZE;

        printk(KERN_INFO "SSD: OPENED\n");
	return 0;
}


static int ssd_release(struct inode *inode, struct file *filp)
{
        printk(KERN_INFO "SSD: RELEASED\n");
	return 0;
}


loff_t ssd_llseek(struct file *filp, loff_t off, int whence)
{
        struct ssd_dev *dev = filp->private_data;
        loff_t newpos;

        switch(whence) {
          case 0: /* SEEK_SET */
                newpos = off;
                break;

          case 1: /* SEEK_CUR */
                newpos = filp->f_pos + off;
                break;

          case 2: /* SEEK_END */
                newpos = dev->size + off;
                break;

          default: /* can't happen */
                return -EINVAL;
        }
        if (newpos < 0) return -EINVAL;
        filp->f_pos = newpos;
        return newpos;
}



ssize_t ssd_read(struct file *filp,
                 char __user *buf,
                 size_t count,
                 loff_t *f_pos)
{
        ssize_t retval = 0;
        int ssdIdx;

        ssdIdx=iminor(filp->f_dentry->d_inode);
        printk(KERN_INFO "SSD: READ started for %d\n", ssdIdx);

        if(buf == (char *)0)
            return EFAULT;

        if (down_interruptible(&ssdDevs[ssdIdx].sem))
            return -ERESTARTSYS;

        if (*f_pos >= ssdDevs[ssdIdx].size)
        {
            //up(&ssdDevs[ssdIdx].sem);
            return -EIO;
        }

        // Read up to end of SSD buffer
        //
        if (*f_pos + count > ssdDevs[ssdIdx].size)
                count = ssdDevs[ssdIdx].size - *f_pos;


        if(copy_to_user(buf, &ssdData[ssdIdx][*f_pos], count))
        {
            up(&ssdDevs[ssdIdx].sem);
            return -EFAULT;
        }
        else
        {
            *f_pos += count;
            retval = count;
        }

        up(&ssdDevs[ssdIdx].sem);

        printk(KERN_INFO "SSD: READ finished\n");
        return retval;
}


ssize_t ssd_write(struct file *filp,
                  const char __user *buf,
                  size_t count,
                  loff_t *f_pos)
{
        ssize_t retval = -ENOMEM;
        int ssdIdx;

        ssdIdx=iminor(filp->f_dentry->d_inode);

        printk(KERN_INFO "SSD: WRITE started for %d\n", ssdIdx);

        if (down_interruptible(&ssdDevs[ssdIdx].sem))
                return -ERESTARTSYS;

        // Copy data from user buffer to SSD
        //
        // Take into account byte offset to LBA mapping and
        // apply RAID mapping including any read-modify-write operations
        // required and computation of new parity LBA.
        //
        // Starting example does simple copy only.
        //
        if(copy_from_user(&ssdData[ssdIdx][*f_pos], buf, count))
        {
            up(&ssdDevs[ssdIdx].sem);
            return -EFAULT;
        }
        else
        {
            // Update file position
            *f_pos += count;
            retval = count;
        }


        /* update the size */
        if (ssdDevs[ssdIdx].size < *f_pos)
                ssdDevs[ssdIdx].size = *f_pos;

        up(&ssdDevs[ssdIdx].sem);

        printk(KERN_INFO "SSD: WRITE finished\n");
        return retval;
}


long ssd_ioctl(/*struct inode *inode,*/ struct file *filp,
                 unsigned int cmd, unsigned long arg)
{
        int retval = 0, j;
        int ssdIdx;
        printk("\n ENTERING THE IOCTL");

        ssdIdx=iminor(filp->f_dentry->d_inode);
        
	printk("SSD::ssdidx=%d",ssdIdx);
        if(cmd == 0) return -EFAULT;
         
        switch(cmd)
        {
             /*lock_kernel();*/
          case SSD_IOCTL_ERASE:
            printk("\n ENTERING THE SWITCH CASE"); 
            // Erase each SSD to all F's
            for(j=0;j<(LBA_SIZE/4)*MAX_LBAS_PER_SSD;j++)
            {
                *((unsigned int *)(&ssdData[ssdIdx][j]))=0xFFFFFFFF;
                if(j>0 && j<=512) 
                printk("%d",j);
            }
            /*unlock_kernel();*/
            break;

          default:  /* redundant, as cmd was checked against MAXNR */
          /*unlock_kernel();*/      
          return -ENOTTY;
        }

        return retval;

}

static void ssd_setup_cdev(struct cdev *dev, int minor, struct file_operations *fops)
{
	int err, devno = MKDEV(ssd_major, minor);
    
	cdev_init(dev, fops);
	dev->owner = THIS_MODULE;
	dev->ops = fops;
	err = cdev_add (dev, devno, 1);

	/* Fail gracefully if need be */
	if (err)
	    printk (KERN_NOTICE "SSD: Error %d adding ssd%d\n", err, minor);
}


static struct file_operations ssd_ops =
{
	.owner   = THIS_MODULE,
        .llseek =   ssd_llseek,
        .read =     ssd_read,
        .write =    ssd_write,
        .unlocked_ioctl = ssd_ioctl,
	.open    = ssd_open,
	.release = ssd_release,
};


static int ssd_init(void)
{
	int result, i, j;
	dev_t dev = MKDEV(ssd_major, 0);

	/* Figure out our device number. */
	if (ssd_major)
		result = register_chrdev_region(dev, MAX_SSD_DEV, "ssd");
	else
        {
		result = alloc_chrdev_region(&dev, 0, MAX_SSD_DEV, "ssd");
		ssd_major = MAJOR(dev);
	}
	if (result < 0)
        {
		printk(KERN_WARNING "SSD: unable to get major %d\n", ssd_major);
		return result;
	}
	if (ssd_major == 0)
		ssd_major = result;

	/* Now set up SSD cdevs. */
        for(i=0;i<MAX_SSD_DEV;i++)
        {
	    ssd_setup_cdev(ssdCdevs+i, i, &ssd_ops);

            sema_init(&ssdDevs[i].sem,1);

            // Erase each SSD to all F's
            for(j=0;j<(LBA_SIZE/4)*MAX_LBAS_PER_SSD;j++)
            {
                *((unsigned int *)(&ssdData[i][j]))=0xFFFFFFFF;
            }
        }

        printk(KERN_INFO "SSD: INITIALIZED %d SSD ram devices\n", MAX_SSD_DEV);

	return 0;
}


static void ssd_cleanup(void)
{
    int i;

    for(i=0;i<MAX_SSD_DEV;i++)
    {
        cdev_del(ssdCdevs+i);
    }
    unregister_chrdev_region(MKDEV(ssd_major, 0), MAX_SSD_DEV);
}


module_init(ssd_init);
module_exit(ssd_cleanup);
