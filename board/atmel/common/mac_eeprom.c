// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2017 Microchip
 *		      Wenyou Yang <wenyou.yang@microchip.com>
 */

#include <dm.h>
#include <env.h>
#include <i2c_eeprom.h>
#include <net.h>
#include <netdev.h>
#include <errno.h>

#define ETH_ADDR_LEN 6

int at91_set_ethaddr(int offset)
{
	unsigned char ethaddr[ETH_ADDR_LEN];
	const char *ETHADDR_NAME = "ethaddr";
	struct udevice *dev;
	int ret;

	if (env_get(ETHADDR_NAME))
		return 0;

	ret = uclass_first_device_err(UCLASS_I2C_EEPROM, &dev);
	if (ret)
		return ret;

	ret = i2c_eeprom_read(dev, offset, ethaddr, 6);
	if (ret)
		return ret;

	if (is_valid_ethaddr(ethaddr))
		eth_env_set_enetaddr(ETHADDR_NAME, ethaddr);

	return 0;
}

/* this function will set eth1addr from a second eeprom, if available */
int at91_set_eth1addr(int offset)
{
	unsigned char ethaddr[ETH_ADDR_LEN];
	/* configure eth1addr for second interface */
	const char *ETHADDR_NAME = "eth1addr";
	struct udevice *dev;
	int ret;

	if (env_get(ETHADDR_NAME))
		return 0;

	/* first eeprom is retrieved, this is for the first interface */
	ret = uclass_first_device_err(UCLASS_I2C_EEPROM, &dev);
	if (ret)
		return ret;

	/* attempt to obtain a second eeprom device */
	ret = uclass_next_device_err(&dev);
	if (ret)
		return ret;

	ret = i2c_eeprom_read(dev, offset, ethaddr, 6);
	if (ret)
		return ret;

	if (is_valid_ethaddr(ethaddr))
		eth_env_set_enetaddr(ETHADDR_NAME, ethaddr);

	return 0;
}

#ifdef CONFIG_I2C_EEPROM
int at91_set_eeprom_ethaddr(const char *eeprom, const char *ethaddr_env,
			    int offset)
{
	unsigned char ethaddr[ETH_ADDR_LEN] = {0};
	struct udevice *dev;
	int ret = 0;

	ret = uclass_get_device_by_name(UCLASS_I2C_EEPROM, eeprom, &dev);
	if (ret)
		goto exit_read;

	ret = i2c_eeprom_read(dev, offset, ethaddr, ETH_ADDR_LEN);
	if (ret)
		goto exit_read;

	if (is_valid_ethaddr(ethaddr)) {
		eth_env_set_enetaddr(ethaddr_env, ethaddr);
	} else {
		ret = -EINVAL;
		goto exit_read;
	}

	return ret;

exit_read:
	env_set(ethaddr_env, ethaddr);
	return ret;
}
#endif
