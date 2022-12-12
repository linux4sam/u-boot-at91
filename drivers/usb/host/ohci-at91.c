// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2006
 * DENX Software Engineering <mk@denx.de>
 */

#if !(CONFIG_IS_ENABLED(DM_USB))

#include <asm/arch/clk.h>

int usb_cpu_init(void)
{
#ifdef CONFIG_USB_ATMEL_CLK_SEL_PLLB
	if (at91_pllb_clk_enable(get_pllb_init()))
		return -1;

#ifdef CONFIG_AT91SAM9N12
	at91_usb_clk_init(AT91_PMC_USBS_USB_PLLB | AT91_PMC_USB_DIV_2);
#endif
#elif defined(CONFIG_USB_ATMEL_CLK_SEL_UPLL)
	if (at91_upll_clk_enable())
		return -1;

	at91_usb_clk_init(AT91_PMC_USBS_USB_UPLL | AT91_PMC_USBDIV_10);
#endif

	at91_periph_clk_enable(ATMEL_ID_UHP);

	at91_system_clk_enable(ATMEL_PMC_UHP);
#if defined(CONFIG_AT91SAM9261) || defined(CONFIG_AT91SAM9G10)
	at91_system_clk_enable(AT91_PMC_HCK0);
#endif

	return 0;
}

int usb_cpu_stop(void)
{
	at91_periph_clk_disable(ATMEL_ID_UHP);

	at91_system_clk_disable(ATMEL_PMC_UHP);
#if defined(CONFIG_AT91SAM9261) || defined(CONFIG_AT91SAM9G10)
	at91_system_clk_disable(AT91_PMC_HCK0);
#endif

#ifdef CONFIG_USB_ATMEL_CLK_SEL_PLLB
#ifdef CONFIG_AT91SAM9N12
	at91_usb_clk_init(0);
#endif

	if (at91_pllb_clk_disable())
		return -1;

#elif defined(CONFIG_USB_ATMEL_CLK_SEL_UPLL)
	if (at91_upll_clk_disable())
		return -1;
#endif

	return 0;
}

int usb_cpu_init_fail(void)
{
	return usb_cpu_stop();
}

#elif CONFIG_IS_ENABLED(DM_GPIO)

#include <clk.h>
#include <dm.h>
#include <asm/gpio.h>
#include <usb.h>
#include "ohci.h"

#if CONFIG_IS_ENABLED(PHY_MICROCHIP_SAMA7_USB)
#include <generic-phy.h>
#endif

#define AT91_MAX_USBH_PORTS        3

#define at91_for_each_port(index)	\
		for ((index) = 0; (index) < AT91_MAX_USBH_PORTS; (index)++)

struct at91_usbh_data {
	enum usb_init_type init_type;
	struct gpio_desc vbus_pin[AT91_MAX_USBH_PORTS];
	u8 ports;				/* number of ports on root hub */
};

struct ohci_at91_priv {
	struct clk *iclk;
	struct clk *fclk;
	struct clk *hclk;
	bool clocked;

#if CONFIG_IS_ENABLED(PHY_MICROCHIP_SAMA7_USB)
	struct phy phy[AT91_MAX_USBH_PORTS];
#endif
};

static void at91_start_clock(struct ohci_at91_priv *ohci_at91)
{
	if (ohci_at91->clocked)
		return;

#if CONFIG_IS_ENABLED(PHY_MICROCHIP_SAMA7_USB)
	int i;

	at91_for_each_port(i)
		generic_phy_power_on(&ohci_at91->phy[i]);
#endif

	clk_set_rate(ohci_at91->fclk, 48000000);
	clk_prepare_enable(ohci_at91->hclk);
	clk_prepare_enable(ohci_at91->iclk);
	clk_prepare_enable(ohci_at91->fclk);
	ohci_at91->clocked = true;
}

static void at91_stop_clock(struct ohci_at91_priv *ohci_at91)
{
	if (!ohci_at91->clocked)
		return;

#if CONFIG_IS_ENABLED(PHY_MICROCHIP_SAMA7_USB)
	int i;

	at91_for_each_port(i)
		generic_phy_power_off(&ohci_at91->phy[i]);
#endif

	clk_disable_unprepare(ohci_at91->fclk);
	clk_disable_unprepare(ohci_at91->iclk);
	clk_disable_unprepare(ohci_at91->hclk);
	ohci_at91->clocked = false;
}

static void ohci_at91_set_power(struct at91_usbh_data *pdata, int port,
				bool enable)
{
	if (!dm_gpio_is_valid(&pdata->vbus_pin[port]))
		return;

	if (enable)
		dm_gpio_set_dir_flags(&pdata->vbus_pin[port],
				      GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);
	else
		dm_gpio_set_dir_flags(&pdata->vbus_pin[port], 0);
}

static void at91_start_hc(struct udevice *dev)
{
	struct ohci_at91_priv *ohci_at91 = dev_get_priv(dev);

	at91_start_clock(ohci_at91);
}

static void at91_stop_hc(struct udevice *dev)
{
	struct ohci_at91_priv *ohci_at91 = dev_get_priv(dev);

	at91_stop_clock(ohci_at91);
}

static int ohci_atmel_deregister(struct udevice *dev)
{
	struct at91_usbh_data *pdata = dev_get_plat(dev);
	int i;

	at91_stop_hc(dev);

	at91_for_each_port(i) {
		if (i >= pdata->ports)
			break;

		ohci_at91_set_power(pdata, i, false);
	}

	return ohci_deregister(dev);
}

static int ohci_atmel_child_pre_probe(struct udevice *dev)
{
	struct udevice *ohci_controller = dev_get_parent(dev);
	struct at91_usbh_data *pdata = dev_get_plat(ohci_controller);
	int i;

	at91_for_each_port(i) {
		if (i >= pdata->ports)
			break;

		ohci_at91_set_power(pdata, i, true);
	}

	return 0;
}

static int ohci_atmel_probe(struct udevice *dev)
{
	struct at91_usbh_data *pdata = dev_get_plat(dev);
	struct ohci_at91_priv *ohci_at91 = dev_get_priv(dev);
	int		      i;
	int		      ret;
	u32		      ports;
	struct ohci_regs      *regs = (struct ohci_regs *)dev_read_addr(dev);

	if (!dev_read_u32(dev, "num-ports", &ports))
		pdata->ports = ports;

	at91_for_each_port(i) {
		if (i >= pdata->ports)
			break;

		gpio_request_by_name(dev, "atmel,vbus-gpio", i,
				     &pdata->vbus_pin[i], GPIOD_IS_OUT |
				     GPIOD_IS_OUT_ACTIVE);
	}

	ohci_at91->iclk = devm_clk_get(dev, "ohci_clk");
	if (IS_ERR(ohci_at91->iclk)) {
		ret = PTR_ERR(ohci_at91->iclk);
		goto fail;
	}

	ohci_at91->fclk = devm_clk_get(dev, "uhpck");
	if (IS_ERR(ohci_at91->fclk)) {
		ret = PTR_ERR(ohci_at91->fclk);
		goto fail;
	}

	ohci_at91->hclk = devm_clk_get(dev, "hclk");
	if (IS_ERR(ohci_at91->hclk)) {
		ret = PTR_ERR(ohci_at91->hclk);
		goto fail;
	}

#if CONFIG_IS_ENABLED(PHY_MICROCHIP_SAMA7_USB)
	at91_for_each_port(i) {
		generic_phy_get_by_index(dev, i, &ohci_at91->phy[i]);
		generic_phy_init(&ohci_at91->phy[i]);
		generic_phy_configure(&ohci_at91->phy[i], NULL);
	}
#endif

	at91_start_hc(dev);

	return ohci_register(dev, regs);

fail:
	at91_for_each_port(i)
		if (&pdata->vbus_pin[i])
			gpio_free(pdata->vbus_pin[i].offset);

	return ret;
}

static const struct udevice_id ohci_usb_ids[] = {
	{ .compatible = "atmel,at91rm9200-ohci", },
	{ .compatible = "microchip,sama7g5-ohci", },
	{ }
};

U_BOOT_DRIVER(ohci_atmel) = {
	.name		 = "ohci_atmel",
	.id		 = UCLASS_USB,
	.of_match	 = ohci_usb_ids,
	.probe		 = ohci_atmel_probe,
	.remove		 = ohci_atmel_deregister,
	.child_pre_probe = ohci_atmel_child_pre_probe,
	.ops		 = &ohci_usb_ops,
	.plat_auto	 = sizeof(struct at91_usbh_data),
	.priv_auto	 = sizeof(struct ohci_at91_priv),
	.flags		 = DM_FLAG_ALLOC_PRIV_DMA,
};

#endif /* CONFIG_IS_ENABLED(DM_USB) && CONFIG_IS_ENABLED(DM_GPIO) */
