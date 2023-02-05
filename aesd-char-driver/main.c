/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include <linux/mutex.h>
#include "aesdchar.h"
#include "aesd_ioctl.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("saadatfar");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

loff_t aesd_llseek(struct file *file, loff_t offset, int whence) {
    struct aesd_buffer_entry *entry;
    int i;
    loff_t size;
    size = 0;
    AESD_CIRCULAR_BUFFER_FOREACH(entry,&aesd_device.buffer,i) {
        size += entry->size;
    }
    return fixed_size_llseek(file, offset, whence, size);
}

long aesd_ioctl(struct file *file, unsigned int request, unsigned long arg) {
    int retval, i, pos;
    int res;
    struct aesd_seekto seekto;
    pos = 0;
    switch (request)
    {
    case AESDCHAR_IOCSEEKTO:
        res = __copy_from_user(&seekto, (const void __user *)arg, sizeof(struct aesd_seekto));
        for (i=0;i<seekto.write_cmd;i++) {
             pos += aesd_device.buffer.entry[aesd_device.buffer.out_offs + i].size;
        }
        pos += seekto.write_cmd_offset;
        file->f_pos = pos;
        retval = 0;
        break;
     
     default:
        retval = 0;
        break;
    }
    return retval;
}

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    int c, i;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    mutex_lock(&aesd_device.lock);
    c = 0;
    for (i=0; i<count; i++) {
        size_t epos;
        struct aesd_buffer_entry *entry = aesd_circular_buffer_find_entry_offset_for_fpos(&(aesd_device.buffer), (*f_pos) + i, &epos);
        if (entry == NULL) break;
        copy_to_user(buf + c, entry->buffptr + epos, 1);
        c++;
    }
    *f_pos = *f_pos + c;
    mutex_unlock(&aesd_device.lock);

    return c;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    char *ptr;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    mutex_lock(&aesd_device.lock);
    if (aesd_device.entry_buffer.size == 0) {
        ptr = kmalloc(count + 1, GFP_KERNEL);
    } else {
        ptr = krealloc(aesd_device.entry_buffer.buffptr, count + aesd_device.entry_buffer.size + 1, GFP_KERNEL);
    }
    copy_from_user(ptr + aesd_device.entry_buffer.size, buf, count);

    if (ptr[count + aesd_device.entry_buffer.size - 1] == '\n') {
        struct aesd_buffer_entry entry;
        if (aesd_device.buffer.full) {
            kfree(aesd_device.buffer.entry[aesd_device.buffer.in_offs].buffptr);
        }
        entry.size = count + aesd_device.entry_buffer.size;
        entry.buffptr = ptr;
        aesd_circular_buffer_add_entry(&(aesd_device.buffer), &entry);
        aesd_device.entry_buffer.size = 0;
    } else {
        aesd_device.entry_buffer.buffptr = ptr;
        aesd_device.entry_buffer.size += count;
    }
    mutex_unlock(&aesd_device.lock);
    return count;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .unlocked_ioctl = aesd_ioctl,
    .llseek = aesd_llseek,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    mutex_init(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    struct aesd_buffer_entry *entry;
    int i;

    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    AESD_CIRCULAR_BUFFER_FOREACH(entry,&aesd_device.buffer,i) {
        if (entry->buffptr != 0) kfree(entry->buffptr);
    }
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
