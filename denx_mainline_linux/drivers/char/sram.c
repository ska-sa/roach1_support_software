/*
 * SRAM driver.
 * Copyright (C) 2004-2006 DENX Software Engineering, Wolfgang Denk, wd@denx.de
 * Copyright (C) 2009 DENX Software Engineering, Heiko Schocher, hs@denx.de
 * adapted for Linux 2.6.31
 *
 * DTS entry (example for 2 srams):
 *	sram@1,0 {
 *		compatible = "sram";
 *		reg = <1 0x100000 0x100000 1 0x200000 0x100000>;
 *	};
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/errno.h>
#include <linux/of_platform.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/pgtable.h>

#define SRAM_MAJOR	32

typedef struct sram_region {
	void __iomem *start_virt;
	ulong start_phys;
	ulong size;
}SRAM_REGION;

static struct device *dev = NULL;

/* Main region list, driver attempts to ioremap those regions and
 * each successfully mapped region is assigned a minor numer.
 * Thus first region on the list is represented as a character
 * device with (32,0) major and minor numbers, etc. */
static struct sram_region *region_table;
static int max_regions = 0;

static ssize_t sram_read(struct file *file, char *buf,
			size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;
	int reg_num = iminor(file->f_path.dentry->d_inode);

	dev_dbg(dev, "sram_read: rdev major = %d, rdev minor = %d\n",
			imajor(file->f_path.dentry->d_inode), reg_num);

	if (p >= region_table[reg_num].size)
		return 0;

	if (count > region_table[reg_num].size - p)
		count = region_table[reg_num].size - p;

	if (copy_to_user(buf, (region_table[reg_num].start_virt + p), count))
		return -EFAULT;

	*ppos += count;

	return count;
}

static ssize_t sram_write(struct file *file, const char *buf,
			 size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;
	int reg_num = iminor(file->f_path.dentry->d_inode);

	dev_dbg(dev, "sram_write: rdev major = %d, rdev minor = %d\n",
			imajor(file->f_path.dentry->d_inode), reg_num);

	if (p >= region_table[reg_num].size)
		return -ENOSPC;

	if (count > region_table[reg_num].size - p)
		count = region_table[reg_num].size - p;

	if (copy_from_user((region_table[reg_num].start_virt + p), buf, count))
		return -EFAULT;

	*ppos += count;

	return count;
}

static int sram_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long size = vma->vm_end - vma->vm_start;
	int reg_num = iminor(file->f_path.dentry->d_inode);

	dev_dbg(dev, "sram_mmap: rdev major = %d, rdev minor = %d\n",
			imajor(file->f_path.dentry->d_inode), reg_num);

	if (offset + size > region_table[reg_num].size)
		return -EINVAL;

	offset += region_table[reg_num].start_phys;

	/* here we have to set the "right" value */
	vma->vm_pgoff = offset >> PAGE_SHIFT;
	pgprot_val( vma->vm_page_prot ) |= ( _PAGE_USER );

	/* Remap-pfn-range will mark the range VM_IO and VM_RESERVED */
	if (remap_pfn_range(vma,
			    vma->vm_start,
			    vma->vm_pgoff,
			    size,
			    vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

static loff_t sram_lseek(struct file *file, loff_t offset, int orig)
{
	int reg_num = iminor(file->f_path.dentry->d_inode);

	dev_dbg(dev, "sram_lseek: rdev major = %d, rdev minor = %d\n",
			imajor(file->f_path.dentry->d_inode), reg_num);

	switch (orig) {
		case 0:
			file->f_pos = offset;
			return file->f_pos;
		case 1:
			file->f_pos += offset;
			return file->f_pos;
		case 2:
			file->f_pos =
				region_table[reg_num].size - offset;
			return file->f_pos;
		default:
			return -EINVAL;
	}
}

static int sram_open(struct inode *inode, struct file *file)
{
	int reg_num = iminor(inode);

	dev_dbg(dev, "sram_open: rdev major = %d, rdev minor = %d\n",
			imajor(inode), iminor(inode));

	if (reg_num >= max_regions)
		return -ENODEV;

	if (region_table[reg_num].start_virt == 0)
		return -ENODEV;

	return capable(CAP_SYS_RAWIO) ? 0 : -EPERM;
}

static struct file_operations sram_fops = {
	llseek:		sram_lseek,
	read:		sram_read,
	write:		sram_write,
	mmap:		sram_mmap,
	open:		sram_open,
};

static int sram_add_one_region(struct device_node *np, int index)
{
	struct resource res;

	if (!of_address_to_resource(np, index, &res)) {
		region_table[index].start_phys = res.start;
		region_table[index].size = 1 + res.end - res.start;
		dev_dbg(dev, "SRAM: region %d, phys = %08lx, size = %08lx\n",
			index, region_table[index].start_phys,
			region_table[index].size);

		region_table[index].start_virt = of_iomap(np, index);
		if (region_table[index].start_virt == 0) {
			dev_err(dev, "ioremap(0x%08lx, 0x%08lx) failed\n",
				region_table[index].start_phys,
				region_table[index].size);
			return -EFAULT;
		} else
			dev_dbg(dev, "region %d, virt = 0x%p\n",
				index, region_table[index].start_virt);
	}
	return 0;
}

static int __devinit sram_probe(struct of_device *ofdev,
				const struct of_device_id *match)
{
	struct device_node *np = NULL;
	struct resource res;
	int	i, reg_cnt;
	int	end = 1;

	dev = &ofdev->dev;

	np = of_find_node_by_type(np, "sram");
	if (!np) {
		dev_dbg(dev, "no node, trying compatible node\n");
		np =  of_find_compatible_node(NULL, NULL, "sram");
		if (!np) {
			dev_err(dev, "%s no node\n", __FUNCTION__);
			return -ENODEV;
		}
	}

	/* get count if regions */
	while(end) {
		if (of_address_to_resource(np, max_regions, &res))
			end = 0;
		else
			max_regions++;
	}
	if (max_regions == 0)
		return -EIO;

	dev_dbg(dev, "regions to initialize = %d\n", max_regions);
	region_table = kzalloc(sizeof(SRAM_REGION) * max_regions, GFP_KERNEL);

	reg_cnt = 0;
	while (reg_cnt < max_regions) {
		if (!sram_add_one_region(np, reg_cnt))
			reg_cnt++;
	}

	dev_dbg(dev, "regions properly initialized = %d\n", max_regions);

	if (register_chrdev(SRAM_MAJOR, "sram", &sram_fops)) {
		dev_dbg(dev, "register_chrdev failed\n");
		for (i = 0; i < max_regions; i++) {
			if (region_table[i].start_virt)
				iounmap((void *)region_table[i].start_virt);
		}
		return -EIO;
	}

	printk("SRAM: initialized\n");
	return 0;
}

static int sram_remove(struct of_device *ofdev)
{
        BUG();
        return 0;
}

static const struct of_device_id sram_match[] = {
        {
                .compatible = "sram",
        },
        {},
};

static struct of_platform_driver sram_driver = {
        .driver = {
                .name = "sram",
        },
        .match_table = sram_match,
        .probe = sram_probe,
        .remove = sram_remove,
};

static int __init sram_init(void)
{
	of_register_platform_driver(&sram_driver);
	return 0;
}
module_init(sram_init);

#ifdef MODULE
static void __exit sram_cleanup(void)
{
	int i;

	for (i = 0; i < max_regions; i++) {
		if (region_table[i].start_virt)
			iounmap((void *)region_table[i].start_virt);
	}

	if (unregister_chrdev(SRAM_MAJOR, "sram"))
		dev_err(dev, "failed to unregister character device\n");
}
module_exit(sram_cleanup);
#endif	/* MODULE */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wolfgang Denk <wd@denx.de>");
MODULE_AUTHOR("Heiko Schocher <hs@denx.de>");
MODULE_DESCRIPTION("SRAM driver");
