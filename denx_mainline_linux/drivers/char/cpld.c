/*
 *  linux/drivers/char/cpld.c
 *
 * Copyright 2008 DENX Software Engineering GmbH
 * Author: Heiko Schocher <hs@denx.de>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/capability.h>
#include <linux/ptrace.h>
#include <linux/device.h>
#include <linux/bootmem.h>
#include <linux/pfn.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/cpld.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/immap_cpm2.h>
#include <asm/cpm2.h>

#ifdef CONFIG_PPC_CPM_NEW_BINDING
#include <linux/of_platform.h>
#endif
typedef struct {
	atomic_t	inUse;
	wait_queue_head_t       wq_timer;
	int	irq;
	int	event;
} timer_dev;

timer_dev	*ptimer;

static int mmap_cpld(struct file * file, struct vm_area_struct * vma)
{
	size_t size = vma->vm_end - vma->vm_start;

	pgprot_val( vma->vm_page_prot ) |= ( _PAGE_USER );
	vma->vm_page_prot = phys_mem_access_prot(file, vma->vm_pgoff,
						 size,
						 vma->vm_page_prot);

	/* Remap-pfn-range will mark the range VM_IO and VM_RESERVED */
	if (remap_pfn_range(vma,
			    vma->vm_start,
			    vma->vm_pgoff,
			    size,
			    vma->vm_page_prot))
		return -EAGAIN;
	return 0;
}

static int open_cpld(struct inode * inode, struct file * filp)
{
	return capable(CAP_SYS_RAWIO) ? 0 : -EPERM;
}

static int release_cpld(struct inode * inode, struct file * filp)
{
	return 0;
}

static const struct file_operations cpld_fops = {
	.mmap		= mmap_cpld,
	.open		= open_cpld,
	.release	= release_cpld,
};

static void print_timer_reg (void)
{
	cpmtimer_cpm2_t	*cpmtimer;

	cpmtimer = &cpm2_immr->im_cpmtimer;

	printk ("------------------------------\n");
	printk ("TGCR1: %x\n", cpmtimer->cpmt_tgcr1);
	printk ("TMR1 : %x\n", cpmtimer->cpmt_tmr1);
	printk ("TRR1 : %x\n", cpmtimer->cpmt_trr1);
	printk ("TCR1 : %x\n", cpmtimer->cpmt_tcr1);
	printk ("TCN1 : %x\n", cpmtimer->cpmt_tcn1);
	printk ("TER1 : %x\n", cpmtimer->cpmt_ter1);
}

static int stopcpld_timer (void)
{
	cpmtimer_cpm2_t	*cpmtimer;

	cpmtimer = &cpm2_immr->im_cpmtimer;
	out_8 (&cpmtimer->cpmt_tgcr1, 0x03);
	return 0;
}

static int startcpld_timer (void)
{
	cpmtimer_cpm2_t	*cpmtimer;

	cpmtimer = &cpm2_immr->im_cpmtimer;
	out_8 (&cpmtimer->cpmt_tgcr1, 0x01);
	return 0;
}

static int setcpld_timeout (unsigned short timeout)
{
	cpmtimer_cpm2_t	*cpmtimer;

	cpmtimer = &cpm2_immr->im_cpmtimer;
	out_be16 (&cpmtimer->cpmt_trr1, timeout);
	return 0;
}

static int initcpld_timer (void)
{
	cpmtimer_cpm2_t	*cpmtimer;

	cpmtimer = &cpm2_immr->im_cpmtimer;

	stopcpld_timer ();
	out_be16 (&cpmtimer->cpmt_tmr1, 0x001c);
	out_be16 (&cpmtimer->cpmt_tcn1, 0x0000);
	setcpld_timeout (41250); /* 10 ms */
	return 0;
}

static irqreturn_t timer_isr( int irq, void *dev_id )
{
	cpmtimer_cpm2_t	*cpmtimer;

	cpmtimer = &cpm2_immr->im_cpmtimer;
	out_be16 (&cpmtimer->cpmt_ter1, 0x03);
	ptimer->event = 1;
	wake_up (&ptimer->wq_timer);
	return IRQ_HANDLED;
}

static int open_timer(struct inode * inode, struct file * filp)
{
	const u32 *data;
	struct device_node *np = NULL;
	int	len;

	if (atomic_read(&ptimer->inUse) != 0) {
		return -EBUSY;
	}
	atomic_inc(&ptimer->inUse);

	np =  of_find_compatible_node(NULL, NULL, "iai,cpld");
	if (!np) {
		printk ("TIMER no cpld node\n");
		return -ENODEV;
	}
	ptimer->irq = of_irq_to_resource(np, 0, NULL);
	if (ptimer->irq == NO_IRQ) {
		printk ("TIMER no interrupt in device node\n");
		return -EINVAL;
	}
	data = of_get_property(np, "timeout", &len);
	if (!data || len != 4) {
		printk(KERN_ERR "TIMER %s has no/invalid "
	        	        "timeout property.\n", np->name);
		return -EINVAL;
	}
	initcpld_timer ();
	setcpld_timeout ((unsigned short )*data);
	/* request IRQ */
	if (request_irq(ptimer->irq, timer_isr, 0, "cpld", NULL)) {
		printk ("CPLD: couldnt register IRQ: %d\n", ptimer->irq);
		atomic_dec(&ptimer->inUse);
		return -EINVAL;
	}
	return 0;
}

static int release_timer(struct inode * inode, struct file * filp)
{
	atomic_dec (&ptimer->inUse);
	if(atomic_read (&ptimer->inUse) == 0) {
		stopcpld_timer ();
		/* disable IRQ */
		free_irq (ptimer->irq, NULL);
		/* cleanups */
	}
	return 0;
}

static unsigned int poll_timer(struct file *filp, poll_table *wait)
{
	int	mask = 0;

	poll_wait(filp, &ptimer->wq_timer, wait);
	if ( ptimer->event) {
		mask = (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM);
		ptimer->event = 0;
	}
	return mask;
}

static ssize_t
read_timer(struct file *file, char __user *buf, size_t count, loff_t *ptr)
{
	int	len = 0;
	return len;
}

static int
ioctl_timer(struct inode * inode, struct file * file,
             unsigned int cmd, unsigned long arg)
{

        switch (cmd) {
        case TIMER_START:
		startcpld_timer ();
		break;
        case TIMER_STOP:
		stopcpld_timer ();
		break;
	case TIMER_PRINT:
		print_timer_reg ();
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static const struct file_operations timer_fops = {
	.open		= open_timer,
	.release	= release_timer,
	.poll		= poll_timer,
	.read		= read_timer,
	.ioctl		= ioctl_timer,
};

static int cpld_memory_open(struct inode * inode, struct file * filp)
{
	switch (iminor(inode)) {
		case 1:
			filp->f_op = &cpld_fops;
			filp->f_mapping->backing_dev_info =
				&directly_mappable_cdev_bdi;
			break;
		case 2:
			filp->f_op = &timer_fops;
			filp->f_mapping->backing_dev_info =
				&directly_mappable_cdev_bdi;
			break;
		default:
			return -ENXIO;
	}
	if (filp->f_op && filp->f_op->open)
		return filp->f_op->open(inode,filp);
	return 0;
}

static const struct file_operations cpld_memory_fops = {
	.open		= cpld_memory_open,	/* just a selector for the real open */
};

static const struct {
	unsigned int		minor;
	char			*name;
	umode_t			mode;
	const struct file_operations	*fops;
} devlist[] = { /* list of minor devices */
	{1, "cpld",     S_IRUSR | S_IWUSR | S_IRGRP, &cpld_fops},
	{2, "timer",     S_IRUSR | S_IWUSR | S_IRGRP, &timer_fops},
};

static struct class *cpld_class;

static int __init chr_dev_cpld_init(void)
{
	int i;

	if (register_chrdev(CPLD_MAJOR, "cpld", &cpld_memory_fops)) {
		printk("unable to get major %d for cpld devs\n", CPLD_MAJOR);
		return -ENXIO;
	}

	ptimer = kzalloc (sizeof (timer_dev), GFP_KERNEL);
	if (ptimer == NULL) {
		printk ("unable to alloc memory for CPLD driver.\n");
		return -ENOMEM;
	}
	/* initialize wakeupqueue */
	init_waitqueue_head (&ptimer->wq_timer);
	cpld_class = class_create(THIS_MODULE, "cpld");
	for (i = 0; i < ARRAY_SIZE(devlist); i++)
		device_create(cpld_class, NULL,
			      MKDEV(CPLD_MAJOR, devlist[i].minor),
			      NULL, devlist[i].name);

	return 0;
}

fs_initcall(chr_dev_cpld_init);
