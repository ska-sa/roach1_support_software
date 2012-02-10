/*
 * Driver for MUCMC52 IO.
 * (C) Copyright 2008 Heiko Schocher, DENX <hs@denx.de>
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
#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <sysdev/simple_gpio.h>

#include <asm/io.h>
#include <asm/mpc52xx.h>
#include "mucmc52_io.h"

#define MUCMC52_IO_VERSION "1.0"
#define MUCMC52_IO_NAME "mucmc52_gpio"

#define MUCMC52_IO_MINOR	250

/* proc interface for driver */
#define MUCMC52_IO_PROC	"driver/mucmc52_io"

/* simple gpio driver returns values > 1, so cut it */
#define GPIO_GET_AUX(gpio) (gpio_get_value(gpio) ? 1 : 0)

/* Driver's banner */
static char mucmc52_gpio_banner[] = KERN_INFO MUCMC52_IO_NAME ": "
					MUCMC52_IO_VERSION "\n";

static struct semaphore io_sem;
static wait_queue_head_t	wq_interrupt;
static int	irq_counter = 0;
static int	irq_counter_last = 0;
static int timer2_irq;
static struct mpc52xx_gpt __iomem *timer2_reg = NULL;
static int timer3_irq;
static struct mpc52xx_gpt __iomem *timer3_reg = NULL;
static struct mpc52xx_gpio __iomem *std_reg = NULL;
static int wkup_irq;
static struct mpc52xx_gpio_wkup __iomem *wkup_reg = NULL;
static int irq_irq;
static struct mpc52xx_intr __iomem *irq_reg = NULL;

/* pin numbers for GPIO pins */
static int	pin_resetstate_flag;
static int	pin_resetstate_pwr_on;
static int	pin_resetstate_clear;
static int	pin_taster_m;
static int	pin_taster_q;
static int	pin_rotary_switch_3;
static int	pin_rotary_switch_2;
static int	pin_rotary_switch_1;
static int	pin_rotary_switch_0;
static int	pin_flash;
static int	pin_jumper_3;
static int	pin_jumper_2;
static int	pin_jumper_1;
static int	pin_jumper_0;
static int	pin_epld_vers_3;
static int	pin_epld_vers_2;
static int	pin_epld_vers_1;
static int	pin_epld_vers_0;
static int	pin_led_h6;
static int	pin_led_h5;
static int	pin_led_h4;
static int	pin_led_h3;
static int	pin_led_h2;
static int	pin_led_hb;
static int	pin_power1_battery_good;
static int	pin_power1_overload;
static int	pin_power1_clear;
static int	pin_power2_cf_over;
static int	pin_power2_rs422;
static int	pin_rs422;
static int	pin_cf;

/* read the state of an irq pin */
static inline int mpc5200_get_irq_pin(int pin)
{
	if (pin > 0) {
		return ((in_be32(&irq_reg->main_status) & \
			(0x00008000 >> (pin - 1))) >> (16 - pin));
	}
	if (pin == 0) {
		return (in_be32(&irq_reg->crit_status) & \
			(0x08000000 >> 27));
	}
	return 0;
}

/* board specific functions */
static int mucmc52_get_state (STATUSINFO *state)
{
	state->resetstate =  (GPIO_GET_AUX(pin_resetstate_flag) << 2) | \
				(GPIO_GET_AUX(pin_resetstate_pwr_on) << 1) | \
				gpio_get_value(pin_resetstate_clear);
	state->taster = (mpc5200_get_irq_pin(0) << 2) | \
			(GPIO_GET_AUX(pin_taster_m) << 1) | \
			GPIO_GET_AUX(pin_taster_q);
	state->drehschalter = (GPIO_GET_AUX(pin_rotary_switch_3) << 3) | \
				(GPIO_GET_AUX(pin_rotary_switch_2) << 2) | \
				(GPIO_GET_AUX(pin_rotary_switch_1) << 1) | \
				(GPIO_GET_AUX(pin_rotary_switch_0));
	state->flash = 	GPIO_GET_AUX(pin_flash);
	state->jumper = ((GPIO_GET_AUX(pin_jumper_3) << 3)) | \
			((GPIO_GET_AUX(pin_jumper_2) << 2)) | \
			((GPIO_GET_AUX(pin_jumper_1) << 1)) | \
			(GPIO_GET_AUX(pin_jumper_0));
	state->epld_vers = (GPIO_GET_AUX(pin_epld_vers_3) << 3) | \
			(GPIO_GET_AUX(pin_epld_vers_2) << 2) | \
			(GPIO_GET_AUX(pin_epld_vers_1) << 1) | \
			(GPIO_GET_AUX(pin_epld_vers_0));
	state->led_h8 = 0;
	state->led_h7 = 0;
	state->led_h6 =  GPIO_GET_AUX(pin_led_h6);
	state->led_h5 =  GPIO_GET_AUX(pin_led_h5);
	state->led_h4 =  GPIO_GET_AUX(pin_led_h4);
	state->led_h3 =  GPIO_GET_AUX(pin_led_h3);
	state->led_h2 =  GPIO_GET_AUX(pin_led_h2);
	state->led_hb =  GPIO_GET_AUX(pin_led_hb);
	state->power1 = ((GPIO_GET_AUX(pin_power1_battery_good) << 3) | \
			(gpio_get_value(pin_power1_overload) << 2) | \
			(mpc5200_get_irq_pin(1) << 1) | \
			gpio_get_value(pin_power1_clear));

	state->power2 = (gpio_get_value(pin_cf) << 2) | \
			(GPIO_GET_AUX(pin_power2_cf_over) << 1) | \
			GPIO_GET_AUX(pin_power2_rs422);
	state->rs422 = GPIO_GET_AUX(pin_rs422);
	state->cf = gpio_get_value(pin_cf);
	state->local_ibs = 0;
	return 0;
}

static int mucmc52_write_state(STATUSINFO *state)
{
	if (state->resetstate & 0x01)
		gpio_set_value(pin_resetstate_clear, 1);
	else
		gpio_set_value(pin_resetstate_clear, 0);

	gpio_set_value(pin_led_h6, state->led_h6);
	gpio_set_value(pin_led_h5, state->led_h5);
	gpio_set_value(pin_led_h4, state->led_h4);
	gpio_set_value(pin_led_h3, state->led_h3);
	gpio_set_value(pin_led_h2, state->led_h2);
	gpio_set_value(pin_led_hb, state->led_hb);
	gpio_set_value(pin_power1_clear, (state->power1 & 0x01));
	gpio_set_value(pin_rs422, state->rs422);

	return 0;
}

static void mucmc52_write_one(unsigned char val, int pos)
{
	switch (pos) {
		case 0:
			if (val & 0x01)
				gpio_set_value(pin_resetstate_clear, 1);
			else
				gpio_set_value(pin_resetstate_clear, 0);
			break;
		case 6:
			break;
		case 7:
			break;
		case 8:
			gpio_set_value(pin_led_h6, val);
			break;
		case 9:
			gpio_set_value(pin_led_h5, val);
			break;
		case 10:
			gpio_set_value(pin_led_h4, val);
			break;
		case 11:
			gpio_set_value(pin_led_h3, val);
			break;
		case 12:
			gpio_set_value(pin_led_h2, val);
			break;
		case 13:
			gpio_set_value(pin_led_hb, val);
			break;
		case 14:
			gpio_set_value(pin_power1_clear, val);
			break;
		case 16:
			gpio_set_value(pin_rs422, val);
			break;
	}
}

#define MPC52xx_GPT_STATUS_IRQMASK	(0x000f)
static irqreturn_t timer2_interrupt(int irq, void *dev_id)
{
	irq_counter++;
	out_be32((&timer2_reg->status), MPC52xx_GPT_STATUS_IRQMASK);
	wake_up (&wq_interrupt);

	return IRQ_HANDLED;
}

static irqreturn_t timer3_interrupt(int irq, void *dev_id)
{
	irq_counter++;
	out_be32((&timer3_reg->status), MPC52xx_GPT_STATUS_IRQMASK);
	wake_up (&wq_interrupt);

	return IRQ_HANDLED;
}

static irqreturn_t irq0_interrupt(int irq, void *dev_id)
{
	irq_counter++;
	wake_up (&wq_interrupt);

	return IRQ_HANDLED;
}

static irqreturn_t psc1_wake_interrupt(int irq, void *dev_id)
{
	irq_counter++;
	out_8((&wkup_reg->wkup_istat), 0x01);
	wake_up (&wq_interrupt);

	return IRQ_HANDLED;
}

static void wkup_enable_irq(void)
{
	out_8((&wkup_reg->wkup_maste), 0);
	out_8((&std_reg->gpio_control), 0);
	setbits8((&wkup_reg->wkup_gpioe), 1);
	setbits8((&wkup_reg->wkup_iinten), 1);
	setbits8((&wkup_reg->wkup_maste), 1);
	setbits8((&std_reg->gpio_control), 1);
}

static void free_board(void)
{
	free_irq (wkup_irq, NULL);
	free_irq (irq_irq, NULL);
	free_irq (timer3_irq, NULL);
	free_irq (timer2_irq, NULL);
	iounmap(timer2_reg);
	iounmap(timer3_reg);
	iounmap(std_reg);
	iounmap(wkup_reg);
	iounmap(irq_reg);
}

/*
 * Device file operations
 */
static loff_t mucmc52_lseek(struct file * file, loff_t offset, int orig)
{
	loff_t ret;

	switch (orig) {
		case 0:
			file->f_pos = offset;
			ret = file->f_pos;
			break;
		case 1:
			file->f_pos += offset;
			ret = file->f_pos;
			break;
		default:
			ret = -EINVAL;
	}
	return ret;
}

static ssize_t mucmc52_gpio_write(struct file *file, const char *buf, size_t bufsz,
					loff_t *off)
{
	STATUSINFO state;
	unsigned char	buffer[20];
	int		pos;
	int		i = 0;

	if (down_trylock(&io_sem))
		return -EBUSY;
	if ((file->f_pos == 0) && (sizeof(STATUSINFO) == bufsz)) {
		if (copy_from_user((void *) &state, buf, sizeof(STATUSINFO)))
			return -EFAULT;
		mucmc52_write_state(&state);
	} else {
		if ((file->f_pos + bufsz) >= sizeof(STATUSINFO))
			return -EINVAL;
		if (copy_from_user((void *) buffer, buf, bufsz))
			return -EFAULT;
		pos = file->f_pos;
		while (i < bufsz) {
			mucmc52_write_one(buffer[i], pos);
			pos ++;
			i++;
		}
	}
	up(&io_sem);

	return sizeof (state);
}

static ssize_t mucmc52_gpio_read(struct file *file, char *buf,
					size_t bufsz, loff_t *off)
{
	STATUSINFO state;

	if (down_trylock(&io_sem))
		return -EBUSY;

	mucmc52_get_state(&state);
	if (copy_to_user(buf, (void *) &state, sizeof(STATUSINFO)))
		return -EFAULT;

	up(&io_sem);
	return sizeof (state);
}

static unsigned int mucmc52_gpio_poll(struct file *file,
					struct poll_table_struct *ptbl)
{
	unsigned int mask = 0;
	poll_wait(file, &wq_interrupt, ptbl);
	if (irq_counter != irq_counter_last) {
		mask = POLLIN | POLLRDNORM;
		irq_counter_last = irq_counter;
	}
	return mask;
}

#ifdef MODULE
static int mucmc52_gpio_open(struct inode *inode, struct file *file)
{
	MOD_INC_USE_COUNT;
	return 0;
}

static int mucmc52_gpio_close(struct inode *inode, struct file *file)
{
	MOD_DEC_USE_COUNT;
	return 0;
}
#endif

static struct file_operations mucmc52_gpio_fops = {
	.owner = THIS_MODULE,
	.llseek = mucmc52_lseek,
	.read	= mucmc52_gpio_read,
	.write	= mucmc52_gpio_write,
	.poll	= mucmc52_gpio_poll,
#ifdef MODULE
	.open	= mucmc52_gpio_open,
	.release = mucmc52_gpio_close
#endif
};

static struct miscdevice mucmc52_gpio_miscdev = {
	MUCMC52_IO_MINOR,
	MUCMC52_IO_NAME,
	&mucmc52_gpio_fops
};

/*
 * procfs operations
 */
#ifdef CONFIG_PROC_FS

static int mucmc52_gpio_proc_read(char *page, char **start, off_t off,
					int count, int *eof, void *data)
{

	mucmc52_get_state((STATUSINFO *)page);
	return sizeof(STATUSINFO);
}
#endif

/*
 * Initialization/cleanup
 */
static int mucmc52_gpio_getpin(struct device_node *np, int *pin,
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

static __init int init_irq(void)
{
	struct device_node *np = NULL;
	int	error;

	simple_gpiochip_init("manroland,mucmc52-aux-gpio");

	np = of_find_node_by_path("/soc5200@f0000000/timer@620");
        if (!np) {
		printk ("%s: timer@620\n", __func__);
		goto error_timer620;
	}
	timer2_reg = of_iomap(np, 0);
	timer2_irq = irq_of_parse_and_map(np, 0);
	error = request_irq(timer2_irq, timer2_interrupt,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"Timer2",
				NULL);
	np = of_find_node_by_path("/soc5200@f0000000/timer@630");
        if (!np) {
		printk ("%s: timer@620\n", __func__);
		goto error_timer630;
	}
	timer3_reg = of_iomap(np, 0);
	timer3_irq = irq_of_parse_and_map(np, 0);
	error = request_irq(timer3_irq, timer3_interrupt,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"Timer3",
				NULL);

	np = of_find_node_by_path("/soc5200@f0000000/gpio@b00");
	if (!np) {
		printk ("%s: gpio@b00\n", __func__);
		goto error_gpiob00;
	}
	std_reg = of_iomap(np, 0);
	np = of_find_node_by_path("/soc5200@f0000000/gpio@c00");
        if (!np) {
		printk ("%s: gpio@c00\n", __func__);
		goto error_gpioc00;
	}
	wkup_reg = of_iomap(np, 0);
	wkup_irq = irq_of_parse_and_map(np, 0);
	error = request_irq(wkup_irq, psc1_wake_interrupt,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"gpio wkup psc1_4",
				NULL);
	np = of_find_node_by_path("/soc5200@f0000000/interrupt-controller@500");
	if (!np) {
		printk ("%s: interrupt-controller@500\n", __func__);
		goto error_irq;
	}
	irq_reg = of_iomap(np, 0);
	irq_irq = irq_of_parse_and_map(np, 0);
	error = request_irq(irq_irq, irq0_interrupt,
				IRQF_TRIGGER_LOW,
				"IRQ0",
				NULL);

	wkup_enable_irq();
	return 0;

error_irq:
	iounmap(wkup_reg);
	free_irq(wkup_irq, NULL);
error_gpioc00:
	iounmap(std_reg);
error_gpiob00:
	iounmap(timer3_reg);
	free_irq(timer3_irq, NULL);
error_timer630:
	iounmap(timer2_reg);
	free_irq(timer2_irq, NULL);
error_timer620:

	return -1;
}

static __init int init_gpio_pins(struct device_node *np)
{
	simple_gpiochip_init("manroland,mucmc52-aux-gpio");

	/* request GPIO pins */
	mucmc52_gpio_getpin(np, &pin_resetstate_clear, 0, -1, "reset clear");
	mucmc52_gpio_getpin(np, &pin_cf, 1, -1, "CF");
	mucmc52_gpio_getpin(np, &pin_power1_overload, 2, -1, "pwr overload");
	mucmc52_gpio_getpin(np, &pin_power1_clear, 3, -1, "pwr clear");
	mucmc52_gpio_getpin(np, &pin_jumper_0, 4, -1, "jumper 0");
	mucmc52_gpio_getpin(np, &pin_jumper_1, 5, -1, "jumper 1");
	mucmc52_gpio_getpin(np, &pin_jumper_2, 6, -1, "jumper 1");
	mucmc52_gpio_getpin(np, &pin_jumper_3, 7, -1, "jumper 1");
	mucmc52_gpio_getpin(np, &pin_rotary_switch_3, 8, -1, "rs 3");
	mucmc52_gpio_getpin(np, &pin_rotary_switch_2, 9, -1, "rs 2");
	mucmc52_gpio_getpin(np, &pin_rotary_switch_1, 10, -1, "rs 1");
	mucmc52_gpio_getpin(np, &pin_rotary_switch_0, 11, -1, "rs 0");
	mucmc52_gpio_getpin(np, &pin_resetstate_flag, 12, -1,
				"reset state flg");
	mucmc52_gpio_getpin(np, &pin_resetstate_pwr_on, 13, -1,
				"reset state pwr on");
	mucmc52_gpio_getpin(np, &pin_flash, 14, -1, "flash");
	mucmc52_gpio_getpin(np, &pin_power1_battery_good, 15, -1,
				"bat. good");
	mucmc52_gpio_getpin(np, &pin_power2_cf_over, 16, -1, "CF over");
	mucmc52_gpio_getpin(np, &pin_taster_q, 17, -1, "Q");
	mucmc52_gpio_getpin(np, &pin_taster_m, 18, -1, "M");
	mucmc52_gpio_getpin(np, &pin_power2_rs422, 19, -1, "rs422 over");
	mucmc52_gpio_getpin(np, &pin_led_hb, 20, -1, "LED HB");
	mucmc52_gpio_getpin(np, &pin_led_h4, 21, -1, "LED H4");
	mucmc52_gpio_getpin(np, &pin_led_h5, 22, -1, "LED H5");
	mucmc52_gpio_getpin(np, &pin_led_h2, 23, -1, "LED H2");
	mucmc52_gpio_getpin(np, &pin_led_h3, 24, -1, "LED H3");
	mucmc52_gpio_getpin(np, &pin_led_h6, 25, -1, "LED H6");
	mucmc52_gpio_getpin(np, &pin_rs422, 26, -1, "RS422");
	mucmc52_gpio_getpin(np, &pin_epld_vers_0, 27, -1, "epld 0");
	mucmc52_gpio_getpin(np, &pin_epld_vers_1, 28, -1, "epld 1");
	mucmc52_gpio_getpin(np, &pin_epld_vers_2, 29, -1, "epld 2");
	mucmc52_gpio_getpin(np, &pin_epld_vers_3, 30, -1, "epld 3");

	return 0;
}

static int __init mucmc52_gpio_probe(struct of_device *ofdev,
                                        const struct of_device_id *match)
{
	struct device_node *np = ofdev->node;
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry * proc;
#endif

	printk(mucmc52_gpio_banner);

	if (misc_register(&mucmc52_gpio_miscdev) < 0) {
		printk(KERN_ERR "%s: can't misc_register on minor=%d\n",
			MUCMC52_IO_NAME, MUCMC52_IO_MINOR);
		goto error;
	}

#ifdef CONFIG_PROC_FS
	proc = create_proc_entry(MUCMC52_IO_PROC, S_IFREG | S_IRUGO, NULL);
	if (!proc) {
		printk(KERN_ERR "failed to create /proc/" \
				 MUCMC52_IO_PROC "\n");
		goto error_unreg;
	}

	proc->read_proc	 = mucmc52_gpio_proc_read;
#endif

	init_waitqueue_head (&wq_interrupt);
	init_gpio_pins(np);
	init_irq();
	sema_init(&io_sem, 1);

	return 0;

#ifdef CONFIG_PROC_FS
	remove_proc_entry(MUCMC52_IO_PROC, NULL);
#endif
error_unreg:
	misc_deregister(&mucmc52_gpio_miscdev);
error:
	return -ENODEV;
}

static int mucmc52_gpio_remove(struct of_device *ofdev)
{
	return 0;
}

static const struct of_device_id mucmc52_gpio_match[] = {
        {
                .compatible = "manroland,mucmc52_gpio",
        },
        {},
};

static struct of_platform_driver mucmc52_gpio_driver = {
        .driver = {
                .name = "mucmc52_gpio",
        },
        .match_table = mucmc52_gpio_match,
        .probe = mucmc52_gpio_probe,
        .remove = mucmc52_gpio_remove,
};

static int __init mucmc52_gpio_init(void)
{
	of_register_platform_driver(&mucmc52_gpio_driver);
	return 0;
}

static void __devexit mucmc52_gpio_cleanup(void)
{
	free_board ();
#ifdef CONFIG_PROC_FS
	remove_proc_entry(MUCMC52_IO_PROC, NULL);
#endif

	misc_deregister(&mucmc52_gpio_miscdev);
	of_unregister_platform_driver(&mucmc52_gpio_driver);
	gpio_free(pin_resetstate_clear);
	gpio_free(pin_resetstate_flag);
	gpio_free(pin_resetstate_pwr_on);
	gpio_free(pin_resetstate_clear);
	gpio_free(pin_taster_m);
	gpio_free(pin_taster_q);
	gpio_free(pin_rotary_switch_3);
	gpio_free(pin_rotary_switch_2);
	gpio_free(pin_rotary_switch_1);
	gpio_free(pin_rotary_switch_0);
	gpio_free(pin_flash);
	gpio_free(pin_jumper_3);
	gpio_free(pin_jumper_2);
	gpio_free(pin_jumper_1);
	gpio_free(pin_jumper_0);
	gpio_free(pin_epld_vers_3);
	gpio_free(pin_epld_vers_2);
	gpio_free(pin_epld_vers_1);
	gpio_free(pin_epld_vers_0);
	gpio_free(pin_led_h6);
	gpio_free(pin_led_h5);
	gpio_free(pin_led_h4);
	gpio_free(pin_led_h3);
	gpio_free(pin_led_h2);
	gpio_free(pin_led_hb);
	gpio_free(pin_power1_battery_good);
	gpio_free(pin_power1_overload);
	gpio_free(pin_power1_clear);
	gpio_free(pin_power2_cf_over);
	gpio_free(pin_power2_rs422);
	gpio_free(pin_rs422);
	gpio_free(pin_cf);
}

module_init(mucmc52_gpio_init)
module_exit(mucmc52_gpio_cleanup)

MODULE_AUTHOR("Heiko Schocher <hs@denx.de>");
MODULE_DESCRIPTION("MUCMC52 IO Driver");
MODULE_LICENSE("GPL");
