/*
 * Driver for the PDSP 1880 Display
 * (C) Copyright 2006 Heiko Schocher, DENX <hs@denx.de>
 * Copyright (C) 2009 DENX Software Engineering, Heiko Schocher, hs@denx.de
 * adapted for Linux 2.6.31
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/pdsp1880_display.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/of_platform.h>

#include <asm/io.h>

#define DISPLAY_VERSION "1.0"
#define DISPLAY_NAME "display"

#define DISPLAY_MINOR	251

/* proc interface for driver */
#define DISPLAY_PROC	"driver/display"

#define DISPLAY_LEN	8

#define CTRL_BRIGHTNESS_MASK	0x07
#define CTRL_BLINK		0x10
#define CTRL_SELF_OK		0x20
#define CTRL_SELF_START		0x40
#define CTRL_CLEAR		0x80

volatile char *display_buf = NULL;
volatile char *display_ctrl;

typedef struct {
	char	flash_ram[32];
	char	udc_addr[8];
	char	udc_ram[8];
	char	control[8];
	char	ch_ram[8];
} DISPLAY;

static DISPLAY		*disp_base = NULL;

/* Froward declarations */
/* Driver's banner */
static char display_banner[] = KERN_INFO DISPLAY_NAME ": "
					DISPLAY_VERSION "\n";

static void display_clear (void)
{
	/* Clear display */
	out_8(display_ctrl, CTRL_CLEAR);
	mdelay(110 * 3);
}

static int display_set_brightness (char value)
{
	char tmp = *display_ctrl;

	tmp &= ~CTRL_BRIGHTNESS_MASK;
	tmp |= (value & CTRL_BRIGHTNESS_MASK);
	out_8(display_ctrl, tmp);

	return 0;
}

/*
 * Device file operations
 */
static ssize_t display_write(struct file *file, const char *buffer,
				size_t count, loff_t *ppos)
{
	int	p = (int )*ppos;
	unsigned char	*buf = (unsigned char *)display_buf;

	if (p >= DISPLAY_LEN) return 0;
	if (count > DISPLAY_LEN - p) count = DISPLAY_LEN - p;

	buf = (unsigned char *)&display_buf[p];
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	*ppos += count;

	return count;
}

static ssize_t display_read(struct file *file, char *buffer,
				size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;
	unsigned char	*buf = (unsigned char *)display_buf;

	/* on the UC101 board we can only write to the display :-( */
	if (p >= DISPLAY_LEN) return 0;
	if (count > DISPLAY_LEN - p) count = DISPLAY_LEN - p;
	buf = (unsigned char *)&display_buf[p];
	if (copy_to_user(buffer, buf, count))
		return -EFAULT;

	*ppos += count;

	return count;

}

static unsigned int display_poll(struct file *file,
				struct poll_table_struct *ptbl)
{
	/* It is always possible to read from the display */
	return POLLIN | POLLRDNORM;
}

static loff_t display_lseek(struct file *file, loff_t offset, int orig)
{

	switch (orig) {
		case 0:
			file->f_pos = offset;
			return file->f_pos;
		case 1:
			file->f_pos += offset;
			return file->f_pos;
		case 2:
			file->f_pos =
				DISPLAY_LEN - offset - 1;
			return file->f_pos;
		default:
			return -EINVAL;
	}
}

static int display_ioctl (struct inode * inode, struct file * file,
			 uint cmd, unsigned long arg)
{
	int err;
	unsigned int param = 0;

	if (_IOC_TYPE(cmd) != DISPLAY_MAGIC) return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if ((err = get_user(param, (unsigned int *)arg)) != 0)
			return err;
	}
	switch (cmd) {
		case DISPLAY_SET_BRIGHTNESS:
			display_set_brightness (param);
			break;
		case DISPLAY_SET_CLEAR:
			display_clear ();
			break;
		default:
			return -ENOTTY;
	}
	return 0;
}

#ifdef MODULE
static int display_open(struct inode *inode, struct file *file)
{
	MOD_INC_USE_COUNT;
	return 0;
}

static int display_close(struct inode *inode, struct file *file)
{
	MOD_DEC_USE_COUNT;
	return 0;
}
#endif

static struct file_operations display_fops = {
	.owner = THIS_MODULE,
	.read	= display_read,
	.write	= display_write,
	.poll	= display_poll,
	.llseek = display_lseek,
	.ioctl	= display_ioctl,
#ifdef MODULE
	.open	= display_open,
	.release = display_close
#endif
};

static struct miscdevice display_miscdev = {
	DISPLAY_MINOR,
	DISPLAY_NAME,
	&display_fops
};

/*
 * procfs operations
 */
#ifdef CONFIG_PROC_FS

static int display_proc_read(char *page, char **start, off_t off,
					int count, int *eof, void *data)
{
	char *p = page;
	char	buf[DISPLAY_LEN + 1];
	int	i;

	/* on the UC101 board we can only write to the display :-( */
	memcpy (buf, (char *)display_buf, DISPLAY_LEN);
	for (i = 0; i < DISPLAY_LEN; i++) {
		p += sprintf (p, "%x ", buf[i]);
	}
	p += sprintf (p, "\n");
	return p - page;
}

static int display_proc_write(struct file *file, const char *buffer,
	unsigned long count, void *data)
{
	unsigned char	*buf = (unsigned char *)display_buf;

	if ( count > DISPLAY_LEN) count = DISPLAY_LEN;
	if ( copy_from_user((void *)buf, buffer, count) ) {
		return -EFAULT;
	}
	return count;
}
#endif

/*
 * Initialization/cleanup
 */
static int __devinit pdsp_display_probe(struct of_device *ofdev,
                                        const struct of_device_id *match)
{
	struct device_node *np = NULL;
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry * proc;
#endif
	printk(display_banner);

	if (misc_register(&display_miscdev) < 0) {
		printk(KERN_ERR "DISPLAY: can't misc_register on minor=%d\n",
			DISPLAY_MINOR);
		goto error;
	}

#ifdef CONFIG_PROC_FS
	proc = create_proc_entry(DISPLAY_PROC, S_IFREG|S_IRUGO, NULL);
	if (!proc) {
		printk(KERN_ERR "failed to create /proc/" DISPLAY_PROC "\n");
		goto error_unreg;
	}

	proc->read_proc	 = display_proc_read;
	proc->write_proc = display_proc_write;
#endif

	/* ioremap the address */
	np =  of_find_compatible_node(NULL, NULL,
			"infineon,pdsp1880-display");
	if (!np) {
		printk("%s no node\n", __FUNCTION__);
		return -ENODEV;
	}
	disp_base = of_iomap(np, 0);
	if (disp_base == NULL) goto error_ioremap;

	display_ctrl	= (char *)disp_base + 0x30;
	display_buf	= (char *)disp_base + 0x38;

	return 0;

error_ioremap:
#ifdef CONFIG_PROC_FS
	remove_proc_entry(DISPLAY_PROC, NULL);
#endif
error_unreg:
	misc_deregister(&display_miscdev);
error:
	return -ENODEV;
}

static const struct of_device_id pdsp_display_match[] = {
        {
                .compatible = "infineon,pdsp1880-display",
        },
        {},
};

static int pdsp_display_remove(struct of_device *ofdev)
{
        BUG();
        return 0;
}

static struct of_platform_driver pdsp_display_driver = {
        .driver = {
                .name = "pdsp1880-display",
        },
        .match_table = pdsp_display_match,
        .probe = pdsp_display_probe,
        .remove = pdsp_display_remove,
};

static int __init pdsp_display_init(void)
{
	of_register_platform_driver(&pdsp_display_driver);
	return 0;
}

static void __exit pdsp_display_cleanup(void)
{
	if (disp_base != NULL)
		iounmap(disp_base);
	display_buf = NULL;
	display_ctrl = NULL;
	remove_proc_entry(DISPLAY_PROC, NULL);
	misc_deregister(&display_miscdev);
}

module_init(pdsp_display_init);
module_exit(pdsp_display_cleanup);

MODULE_AUTHOR("Heiko Schocher <hs@denx.de>");
MODULE_DESCRIPTION("PDSP 1880 Display driver");
MODULE_LICENSE("GPL");
