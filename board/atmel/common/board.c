// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2017 Microchip
 *		      Wenyou Yang <wenyou.yang@microchip.com>
 */

#include <dm.h>
#include <env.h>
#include <i2c_eeprom.h>
#include <w1.h>
#include <w1-eeprom.h>
#include <dm/device-internal.h>

#define AT91_PDA_EEPROM_ID_OFFSET		15
#define AT91_PDA_EEPROM_ID_LENGTH		5
#define AT91_PDA_EEPROM_DEFAULT_BUS		0

#ifdef CONFIG_I2C_EEPROM
#define AT91_EXT_BOARD_START_OFF		0x40
#define AT91_EXT_BOARD_MCHP_DIV_LEN		4
#define AT91_EXT_BOARD_MFG_COUNTRY_LEN		2
#define AT91_EXT_BOARD_CODE_LEN			12
#define AT91_EXT_BOARD_DRIVER_LEN		10

struct ext_eeprom_data {
	u8 num_bytes;
	char separator_1;
	char mchp_div[AT91_EXT_BOARD_MCHP_DIV_LEN];
	char separator_2;
	char mfg_country[AT91_EXT_BOARD_MFG_COUNTRY_LEN];
	char separator_3;
	char ordering_code[AT91_EXT_BOARD_CODE_LEN];
	char separator_4;
	char main_driver[AT91_EXT_BOARD_DRIVER_LEN];
	char separator_5;
	u8 year;
	u8 week;
	char separator_6;
	u8 hw_rev;
	char separator_7;
	u8 map_rev;
	char separator_8;
	u8 crc;
};

struct ext_eeprom_data ext_eeprom;
#endif

char *get_cpu_name(void);

void dummy(void)
{
}

#if defined CONFIG_W1
void at91_pda_detect(void)
{
	struct udevice *bus, *dev;
	u8 buf[AT91_PDA_EEPROM_ID_LENGTH + 1] = {0};
	int ret;
	int pda = 0;

	ret = w1_get_bus(AT91_PDA_EEPROM_DEFAULT_BUS, &bus);
	if (ret)
		goto pda_detect_err;

	for (device_find_first_child(bus, &dev);
	     dev;
	     device_find_next_child(&dev)) {
		ret = device_probe(dev);
		if (ret) {
			continue;
		} else {
			ret = w1_eeprom_read_buf(dev, AT91_PDA_EEPROM_ID_OFFSET,
						 (u8 *)buf, AT91_PDA_EEPROM_ID_LENGTH);
			if (ret)
				goto pda_detect_err;
			break;
		}
	}
	pda = dectoul((const char *)buf, NULL);

	switch (pda) {
	case 7000:
		if (buf[4] == 'B')
			printf("PDA TM7000B detected\n");
		else
			printf("PDA TM7000 detected\n");
		break;
	case 4300:
		printf("PDA TM4300 detected\n");
		break;
	case 5000:
		printf("PDA TM5000 detected\n");
		break;
	}

pda_detect_err:
	env_set("pda", (const char *)buf);
}
#else
void at91_pda_detect(void)
{
}
#endif

#ifdef CONFIG_I2C_EEPROM
const char *at91_ext_board_detect(const char *eeprom)
{
	struct udevice *dev;
	int ret;
	size_t len;

	memset(&ext_eeprom, 0, sizeof(ext_eeprom));
	ret = uclass_get_device_by_name(UCLASS_I2C_EEPROM, eeprom, &dev);
	if (ret)
		goto ext_detect_exit;

	ret = i2c_eeprom_read(dev, AT91_EXT_BOARD_START_OFF,
			      (void *)&ext_eeprom, sizeof(ext_eeprom));
	if (ret)
		goto ext_detect_exit;

	len = AT91_EXT_BOARD_DRIVER_LEN;
	while (len > 0 && ext_eeprom.main_driver[len - 1] == ' ') {
		ext_eeprom.main_driver[len - 1] = '\0';
		len--;
	}

ext_detect_exit:
	return (const char *)ext_eeprom.main_driver;
}

void at91_ext_board_display_detect(const char *eeprom)
{
	env_set("display", at91_ext_board_detect(eeprom));
}

void at91_ext_board_eth_detect(const char *eeprom)
{
	env_set("eth_phy", at91_ext_board_detect(eeprom));
}
#endif

void at91_prepare_cpu_var(void)
{
	env_set("cpu", get_cpu_name());
}
