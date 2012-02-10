/*
 * This driver gives access(read/write) to the bootcounter used by u-boot.
 * Access is supported via procFS and sysFS.
 *
 * Copyright 2008 DENX Software Engineering GmbH
 * Author: Heiko Schocher <hs@denx.de>
 * Based on work from: Steffen Rumler  (Steffen.Rumler@siemens.com)
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/init.h>
#include <linux/capability.h>
#include <linux/ptrace.h>
#include <linux/device.h>

#include <asm/uaccess.h>
#include <asm/io.h>

#include <linux/of_platform.h>

#ifndef CONFIG_PROC_FS
#error "PROC FS support must be switched-on"
#endif


#define	UBOOT_BOOTCOUNT_MAGIC_OFFSET	0x04	/* offset of magic number */
#define	UBOOT_BOOTCOUNT_MAGIC		0xB001C041	/* magic number value */

#define	UBOOT_BOOTCOUNT_PROC_ENTRY	"driver/bootcount"	/* PROC FS entry under '/proc' */

/*
 * This macro frees the machine specific function from bounds checking and
 * this like that... 
 */
#define PRINT_PROC(fmt,args...) \
	do { \
		*len += sprintf( buffer+*len, fmt, ##args ); \
		if (*begin + *len > offset + size) \
			return( 0 ); \
		if (*begin + *len < offset) { \
			*begin += *len; \
			*len = 0; \
		} \
	} while(0)

void __iomem *mem = NULL;
/*
 * read U-Boot bootcounter
 */
static int
read_bootcounter_info(char *buffer, int *len, off_t * begin, off_t offset,
		       int size)
{
	unsigned long magic;
	unsigned long counter;


	magic = *((unsigned long *) (mem + UBOOT_BOOTCOUNT_MAGIC_OFFSET));
	counter = *((unsigned long *) (mem));

	if (magic == UBOOT_BOOTCOUNT_MAGIC) {
		PRINT_PROC ("%lu\n", counter);
	} else {
		PRINT_PROC ("bad magic: 0x%lu != 0x%lu\n", magic,
			    (unsigned long)UBOOT_BOOTCOUNT_MAGIC);
	}

	return 1;
}

/*
 * read U-Boot bootcounter (wrapper)
 */
static int
read_bootcounter(char *buffer, char **start, off_t offset, int size,
		  int *eof, void *arg)
{
	int len = 0;
	off_t begin = 0;


	*eof = read_bootcounter_info(buffer, &len, &begin, offset, size);

	if (offset >= begin + len)
		return 0;

	*start = buffer + (offset - begin);
	return size < begin + len - offset ? size : begin + len - offset;
}

/*
 * write new value to U-Boot bootcounter
 */
static int
write_bootcounter(struct file *file, const char *buffer, unsigned long count,
		   void *data)
{
	unsigned long magic;
	unsigned long *counter_ptr;


	magic = *((unsigned long *) (mem + UBOOT_BOOTCOUNT_MAGIC_OFFSET));
	counter_ptr = (unsigned long *) (mem);

	if (magic == UBOOT_BOOTCOUNT_MAGIC)
		*counter_ptr = simple_strtol(buffer, NULL, 10);
	else
		return -EINVAL;

	return count;
}

/* helper for the sysFS */
static int show_str_bootcount(struct device *device,
				struct device_attribute *attr,
				char *buf)
{
	int ret = 0;
	off_t begin = 0;

	read_bootcounter_info(buf, &ret, &begin, 0, 20);
        return ret;
}
static int store_str_bootcount(struct device *dev,
			struct device_attribute *attr,
			const char *buf,
			size_t count)
{
	write_bootcounter(NULL, buf, count, NULL);
	return count;
}
static DEVICE_ATTR(bootcount, S_IWUSR | S_IRUGO, show_str_bootcount, store_str_bootcount);

static int __devinit bootcount_probe(struct of_device *ofdev,
                                        const struct of_device_id *match)
{
	struct device_node *np = NULL;
	struct proc_dir_entry *bootcount;

	printk("%s (%d) %s:  ", __FILE__, __LINE__, __FUNCTION__);
	np = of_find_node_by_type(np, "bootcount");
	if (!np) {
		printk("%s no node, trying compatible node\n", __FUNCTION__);
		np =  of_find_compatible_node(NULL, NULL, "uboot,bootcount");
		if (!np) {
			printk("%s no node\n", __FUNCTION__);
			return -ENODEV;
		}
	}
	mem = of_iomap(np, 0);
	if (mem == NULL) {
		printk("%s couldnt map register.\n", __FUNCTION__);
	}

	/* init ProcFS */
	if ((bootcount =
	     create_proc_entry(UBOOT_BOOTCOUNT_PROC_ENTRY, 0600,
				NULL)) == NULL) {

		printk(KERN_ERR "\n%s (%d): cannot create /proc/%s\n",
			__FILE__, __LINE__, UBOOT_BOOTCOUNT_PROC_ENTRY);
	} else {

		bootcount->read_proc = read_bootcounter;
		bootcount->write_proc = write_bootcounter;
		printk("created \"/proc/%s\"\n", UBOOT_BOOTCOUNT_PROC_ENTRY);
	}

	if (device_create_file(&ofdev->dev, &dev_attr_bootcount))
		printk("%s couldnt register sysFS entry.\n", __FUNCTION__);

        return 0;
}

static int bootcount_remove(struct of_device *ofdev)
{
        BUG();
        return 0;
}

static const struct of_device_id bootcount_match[] = {
        {
                .compatible = "uboot,bootcount",
        },
        {},
};

static struct of_platform_driver bootcount_driver = {
        .driver = {
                .name = "bootcount",
        },
        .match_table = bootcount_match,
        .probe = bootcount_probe,
        .remove = bootcount_remove,
};


static int __init uboot_bootcount_init(void)
{
	of_register_platform_driver(&bootcount_driver);
	return 0;
}

static void __exit uboot_bootcount_cleanup(void)
{
	if (mem != NULL)
		iounmap(mem);
	remove_proc_entry(UBOOT_BOOTCOUNT_PROC_ENTRY, NULL);
}

module_init(uboot_bootcount_init);
module_exit(uboot_bootcount_cleanup);

MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("Steffen Rumler <steffen.rumler@siemens.com>");
MODULE_DESCRIPTION ("Provide (read/write) access to the U-Boot bootcounter via PROC FS");
