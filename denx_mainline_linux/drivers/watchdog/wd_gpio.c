/***********************************************************************
 *
 * (C) Copyright 2009
 * Heiko Schocher, DENX Software Engineering, hs@denx.de.
 *
 * Hardware specific watchdog functions for using a GPIO pin
 * for triggering an external WDT
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 ***********************************************************************/
#include <linux/kernel.h>
#include <linux/reboot.h>
#include <linux/wd.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

#include <asm/io.h>
#include <asm/machdep.h>
#include <asm/mpc52xx.h>
#include <asm/gpio.h>

/* GPIO pin number for triggering WDT */
static int pin;

/* trigger period in jiffies, setup from DTS */
unsigned long	trigger_period;

static const struct of_device_id wd_gpio_match[] = {
        {
                .compatible = "generic,gpio-wdt",
        },
        {},
};

#if defined(CONFIG_ARCH_HAS_NMI_WATCHDOG)
static unsigned __iomem *base = NULL;
/* only trigger WDT until real driver is loaded */
static int	real_wdt_driver_is_up = 0;

#define GPT_OUT_0	0x00000027
#define GPT_OUT_1	0x00000037

void touch_nmi_watchdog(void)
{
	/*
	 * When the "real" watchdog is running, the early bootup
	 * kick function is not needed anymore, so just return
	 */
	if (real_wdt_driver_is_up)
		return;

	if (base == NULL) {
		struct device_node *np = NULL;

		np = of_find_compatible_node(NULL, NULL,
					wd_gpio_match[0].compatible);
		if (!np) {
			printk("%s: no %s node\n", __func__,
				wd_gpio_match[0].compatible);
			return ;
		}
		base = of_iomap(np, 0);
	}
	out_be32(base, GPT_OUT_1);
	out_be32(base, GPT_OUT_0);
}
#endif

int wd_gpio_init(unsigned long *tpp)
{
	/*
	 * Return number of seconds for re-triggering, calculations are
	 * done in jiffies
	 */
	*tpp = trigger_period;

#if defined(CONFIG_ARCH_HAS_NMI_WATCHDOG)
	/* real WDT driver is up and running */
	real_wdt_driver_is_up = 1;
	if (base)
		iounmap(base);
#endif

	return 0;
}

/*
 * This function resets watchdog counter.
 */
void wd_gpio_kick(void)
{
	gpio_set_value(pin, 1);
	gpio_set_value(pin, 0);
}

/*
 * This function stops watchdog timer
 */
void wd_gpio_delete(void)
{
	/* not possible */
}

/*
 * This function performs emergency reboot.
 */
void wd_gpio_machine_restart(void)
{
	machine_restart(NULL);
}

static int __devinit wd_gpio_probe(struct of_device *ofdev,
                                        const struct of_device_id *match)
{
	struct device_node *np = ofdev->node;
	const u32 *data;
	int len;
	int	ret;

	data = of_get_property(np, "period", &len);
	if (data)
		trigger_period = msecs_to_jiffies(*data);

	pin = of_get_gpio(np, 0);

	if (!gpio_is_valid(pin)) {
		printk("%s: no valid gpio pin\n", __func__);
		return -EFAULT;
	}

	ret = gpio_request(pin, "wdt trigger");
	if (ret) {
		printk("%s: could not request gpio pin\n", __func__);
		return -EFAULT;
	}
	gpio_direction_output(pin, 0);

	wd_hw_functions.wd_init = wd_gpio_init;
	wd_hw_functions.wd_kick = wd_gpio_kick;
	wd_hw_functions.wd_delete = wd_gpio_delete;
	wd_hw_functions.wd_machine_restart = wd_gpio_machine_restart;

	return 0;
}

static int wd_gpio_remove(struct of_device *ofdev)
{
	return 0;
}

static struct of_platform_driver wd_gpio_driver = {
        .driver = {
                .name = "gpio-wdt",
        },
        .match_table = wd_gpio_match,
        .probe = wd_gpio_probe,
        .remove = wd_gpio_remove,
};

static int __init wd_gpio_init_hwl(void)
{
	of_register_platform_driver(&wd_gpio_driver);
	return 0;
}

subsys_initcall(wd_gpio_init_hwl);
