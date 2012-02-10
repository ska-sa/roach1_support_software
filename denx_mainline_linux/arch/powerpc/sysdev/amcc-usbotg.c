/*
 * AMCC PPC4xx USB-OTG wrapper
 *
 * Copyright 2008-2009 DENX Software Engineering, Stefan Roese <sr@denx.de>
 *
 * Extract the resources (MEM & IRQ) from the dts file and put them
 * into the platform-device struct for usage in the platform-device
 * USB-OTG driver.
 *
 */

#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/dma-mapping.h>

/*
 * Resource template will be filled dynamically with the values
 * extracted from the dts file
 */
static struct resource usb_otg_resources[] = {
	[0] = {
		/* 4xx USB-OTG registers */
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		/* 4xx OTG IRQ */
		.flags  = IORESOURCE_IRQ,
	},
	[2] = {
		/* High-Power workaround IRQ */
		.flags  = IORESOURCE_IRQ,
	},
	[3] = {
		/* 4xx DMA IRQ */
		.flags  = IORESOURCE_IRQ,
	},
};

static u64 dma_mask = DMA_BIT_MASK(32);

static struct platform_device usb_otg_device = {
        .name = "dwc_otg",
        .id = 0,
        .num_resources = ARRAY_SIZE(usb_otg_resources),
        .resource = usb_otg_resources,
	.dev = {
		 .dma_mask = &dma_mask,
		 .coherent_dma_mask = DMA_BIT_MASK(32),
	},
};

static int __devinit ppc4xx_usb_otg_probe(struct of_device *ofdev,
					    const struct of_device_id *match)
{
	struct device_node *np = ofdev->node;
	struct resource res;

	/*
	 * Extract register address reange from device tree and put it into
	 * the platform device structure
	 */
	if (of_address_to_resource(np, 0, &res)) {
		printk(KERN_ERR "%s: Can't get USB-OTG register address\n", __func__);
		return -ENOMEM;
	}
	usb_otg_resources[0].start = res.start;
	usb_otg_resources[0].end = res.end;

	/*
	 * Extract IRQ number(s) from device tree and put them into
	 * the platform device structure
	 */
	usb_otg_resources[1].start = usb_otg_resources[1].end =
		irq_of_parse_and_map(np, 0);
	usb_otg_resources[2].start = usb_otg_resources[2].end =
		irq_of_parse_and_map(np, 1);
	usb_otg_resources[3].start = usb_otg_resources[3].end =
		irq_of_parse_and_map(np, 2);

	return platform_device_register(&usb_otg_device);
}

static int __devexit ppc4xx_usb_otg_remove(struct of_device *ofdev)
{
	/* Nothing to do here */
	return 0;
}

static const struct of_device_id ppc4xx_usb_otg_match[] = {
	{ .compatible = "amcc,usb-otg", },
	{}
};

static struct of_platform_driver ppc4xx_usb_otg_driver = {
	.name = "ppc4xx-usb-otg",
	.match_table = ppc4xx_usb_otg_match,
	.probe = ppc4xx_usb_otg_probe,
	.remove = ppc4xx_usb_otg_remove,
};

static int __init ppc4xx_usb_otg_init(void)
{
	return of_register_platform_driver(&ppc4xx_usb_otg_driver);
}
device_initcall(ppc4xx_usb_otg_init);
