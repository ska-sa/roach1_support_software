/*
 * Driver for UC101 GPIO.
 * (C) Copyright 2006 Heiko Schocher, DENX <hs@denx.de>
 *
 * (C) Copyright 2009 Heiko Schocher, DENX <hs@denx.de>
 * adapted to Linux 2.6.31
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
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <sysdev/simple_gpio.h>

#include <asm/mpc5xxx.h>

#define UC101_GPIO_VERSION "1.0"
#define UC101_GPIO_NAME "uc101_gpio"

#define UC101_GPIO_MINOR	250

/* proc interface for driver */
#define UC101_GPIO_PROC	"driver/uc101_gpio"

/* Driver's banner */
static char uc101_gpio_banner[] = KERN_INFO UC101_GPIO_NAME ": "
					UC101_GPIO_VERSION "\n";

static int pin_rs_clear;
static int pin_cf;
static int pin_hb;
static int pin_h4;
static int pin_h5;
static int pin_ibs;
static int pin_jumper;
static int pin_rsf;
static int pin_rs3;
static int pin_rs2;
static int pin_rs1;
static int pin_rs0;
static int pin_pof;
static int pin_flash;
static int pin_cf_over;
static int pin_m;
static int pin_q;
static int pin_h2;
static int pin_h3;

static int uc101_get_state (unsigned long *state)
{
	/* The WDT pin is not longer accesable, because
	 * the WDT driver requests this pin.
	 */
	*state = ( \
		(gpio_get_value(pin_rsf) << 15) | \
		(gpio_get_value(pin_pof) << 14) | \
		(gpio_get_value(pin_m) << 13) | \
		(gpio_get_value(pin_q) << 12) | \
		(gpio_get_value(pin_rs3) << 11) | \
		(gpio_get_value(pin_rs2) << 10) | \
		(gpio_get_value(pin_rs1) << 9) | \
		(gpio_get_value(pin_rs0) << 8) | \
		(gpio_get_value(pin_h3) << 7) | \
		(gpio_get_value(pin_h2) << 6) | \
		(gpio_get_value(pin_h5) << 5) | \
		(gpio_get_value(pin_h4) << 4) | \
		(gpio_get_value(pin_hb) << 3) | \
		(gpio_get_value(pin_ibs) << 2) | \
		(gpio_get_value(pin_rs_clear) << 1)
		);
	return 0;
}

static int uc101_write_state (unsigned char state)
{
	gpio_set_value(pin_hb, (state & 0x08));
	gpio_set_value(pin_h4, (state & 0x10));
	gpio_set_value(pin_h5, (state & 0x20));
	gpio_set_value(pin_h2, (state & 0x40));
	gpio_set_value(pin_h3, (state & 0x80));

	return 0;
}

/*
 * Device file operations
 */
static ssize_t uc101_gpio_write(struct file *file, const char *buf,
					size_t bufsz, loff_t *off)
{
	unsigned char state;

	if (copy_from_user((void *) &state, buf, 1))
		return -EFAULT;
	uc101_write_state(state);

	return sizeof(state);
}

static ssize_t uc101_gpio_read(struct file *file, char *buf,
				size_t bufsz, loff_t *off)
{
	unsigned long state = 0;

	uc101_get_state(&state);
	if (copy_to_user(buf, (void *) &state, sizeof(unsigned long)))
		return -EFAULT;

	return sizeof (state);
}

static unsigned int uc101_gpio_poll(struct file *file,
					struct poll_table_struct *ptbl)
{
	/* It is always possible to read pin states */
	return POLLIN | POLLRDNORM;
}

#ifdef MODULE
static int uc101_gpio_open(struct inode *inode, struct file *file)
{
	MOD_INC_USE_COUNT;
	return 0;
}

static int uc101_gpio_close(struct inode *inode, struct file *file)
{
	MOD_DEC_USE_COUNT;
	return 0;
}
#endif

static struct file_operations uc101_gpio_fops = {
	.owner = THIS_MODULE,
	.read	= uc101_gpio_read,
	.write	= uc101_gpio_write,
	.poll	= uc101_gpio_poll,
#ifdef MODULE
	.open	= uc101_gpio_open,
	.release = uc101_gpio_close
#endif
};

static struct miscdevice uc101_gpio_miscdev = {
	UC101_GPIO_MINOR,
	UC101_GPIO_NAME,
	&uc101_gpio_fops
};

/*
 * procfs operations
 */
#ifdef CONFIG_PROC_FS

static int uc101_gpio_proc_read(char *page, char **start, off_t off,
					int count, int *eof, void *data)
{
	unsigned long	state = 0;
	char *p = page;

	uc101_get_state(&state);
	p += sprintf (p, "%x", (unsigned int)state);
	return p - page;
}

static int uc101_gpio_proc_write(struct file *file, const char *buffer,
	unsigned long count, void *data)
{
	unsigned char	state;

	if (count == 5) {
		unsigned char buf[10];
		unsigned char *p = (unsigned char *)buf;
		state = 0;
		if (count > 10) count = 10;
		if ( copy_from_user(buf, buffer, count) )
			return -EFAULT;
		state = simple_strtoul(p, NULL, 16);
	} else {
		return -EFAULT;
	}
	uc101_write_state(state);

	return 5;
}
#endif

/*
 * Initialization/cleanup
 */
static int uc101_gpio_getpin(struct device_node *np, int *pin,
					int index, int dir, char *name)
{
	int	ret;

	*pin = of_get_gpio(np, index);
	if (!gpio_is_valid(*pin)) {
		printk("%s: %s no valid gpio pin\n", __func__, name);
		return -EFAULT;
	}
	ret = gpio_request(*pin, name);
	if (ret) {
		printk("%s: could not request gpio pin\n", __func__);
		return -EFAULT;
	}

	if (dir < 0) {
		gpio_direction_input(*pin);
	} else {
		gpio_direction_output(*pin, dir);
	}
	return 0;
}

static int __devinit uc101_gpio_probe(struct of_device *ofdev,
                                        const struct of_device_id *match)
{
	struct device_node *np = ofdev->node;
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry * proc;
#endif

	printk(uc101_gpio_banner);

	if (misc_register(&uc101_gpio_miscdev) < 0) {
		printk(KERN_ERR "UC101 GPIO: can't misc_register" \
			" on minor=%d\n", UC101_GPIO_MINOR);
		goto error;
	}

#ifdef CONFIG_PROC_FS
	proc = create_proc_entry(UC101_GPIO_PROC, S_IFREG|S_IRUGO, NULL);
	if (!proc) {
		printk(KERN_ERR "failed to create /proc/"\
				 UC101_GPIO_PROC "\n");
		goto error_unreg;
	}

	proc->read_proc	 = uc101_gpio_proc_read;
	proc->write_proc = uc101_gpio_proc_write;
#endif

	uc101_gpio_getpin(np, &pin_rs_clear, 0, -1, "reset clear");
	uc101_gpio_getpin(np, &pin_cf, 1, -1, "CF");
	uc101_gpio_getpin(np, &pin_hb, 2, 0, "LED HB");
	uc101_gpio_getpin(np, &pin_h4, 3, 0, "LED H4");
	uc101_gpio_getpin(np, &pin_h5, 4, 0, "LEDH5");
	uc101_gpio_getpin(np, &pin_ibs, 5, -1, "IBS poweron");
	uc101_gpio_getpin(np, &pin_jumper, 6, -1, "jumper");

	uc101_gpio_getpin(np, &pin_rs3, 7, -1, "RS 3");
	uc101_gpio_getpin(np, &pin_rs2, 8, -1, "RS 2");
	uc101_gpio_getpin(np, &pin_rs1, 9, -1, "RS 1");
	uc101_gpio_getpin(np, &pin_rs0, 10, -1, "RS 0");

	uc101_gpio_getpin(np, &pin_rsf, 11, -1, "RSF");
	uc101_gpio_getpin(np, &pin_pof, 12, -1, "POF");

	uc101_gpio_getpin(np, &pin_flash, 13, -1, "Flash");
	uc101_gpio_getpin(np, &pin_cf_over, 14, -1, "CF over");

	uc101_gpio_getpin(np, &pin_q, 15, -1, "Q");
	uc101_gpio_getpin(np, &pin_m, 16, -1, "M");

	uc101_gpio_getpin(np, &pin_h2, 17, 0, "LED H2");
	uc101_gpio_getpin(np, &pin_h3, 18, 0, "LED H3");

	return 0;

#ifdef CONFIG_PROC_FS
	remove_proc_entry(UC101_GPIO_PROC, NULL);
#endif
error_unreg:
	misc_deregister(&uc101_gpio_miscdev);
error:
	return -ENODEV;

}

static int uc101_gpio_remove(struct of_device *ofdev)
{
	return 0;
}

static const struct of_device_id uc101_gpio_match[] = {
        {
                .compatible = "manroland,uc101_gpio",
        },
        {},
};

static struct of_platform_driver uc101_gpio_driver = {
        .driver = {
                .name = "uc101_gpio",
        },
        .match_table = uc101_gpio_match,
        .probe = uc101_gpio_probe,
        .remove = uc101_gpio_remove,
};

static int __init uc101_gpio_init(void)
{
	of_register_platform_driver(&uc101_gpio_driver);
	return 0;
}

static void __devexit uc101_gpio_cleanup(void)
{
#ifdef CONFIG_PROC_FS
	remove_proc_entry(UC101_GPIO_PROC, NULL);
#endif

	misc_deregister(&uc101_gpio_miscdev);
	of_unregister_platform_driver(&uc101_gpio_driver);
	gpio_free(pin_rs_clear);
	gpio_free(pin_cf);
	gpio_free(pin_hb);
	gpio_free(pin_h4);
	gpio_free(pin_h5);
	gpio_free(pin_ibs);
	gpio_free(pin_jumper);
	gpio_free(pin_rs3);
	gpio_free(pin_rs2);
	gpio_free(pin_rs1);
	gpio_free(pin_rs0);
	gpio_free(pin_rsf);
	gpio_free(pin_pof);
	gpio_free(pin_flash);
	gpio_free(pin_cf_over);
	gpio_free(pin_m);
	gpio_free(pin_q);
	gpio_free(pin_h2);
	gpio_free(pin_h3);
}
module_init(uc101_gpio_init)
module_exit(uc101_gpio_cleanup)

MODULE_AUTHOR("Heiko Schocher <hs@denx.de>");
MODULE_DESCRIPTION("UC101 GPIO Driver");
MODULE_LICENSE("GPL");
