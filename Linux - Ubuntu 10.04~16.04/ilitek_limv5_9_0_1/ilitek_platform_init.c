/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Luca Hsu <luca_hsu@ilitek.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 *
 */

#include "ilitek_ts.h"
#include "ilitek_common.h"
struct ilitek_ts_data *ilitek_data;
/*
Data struct init. When the driver will be setup, the driver initial the data struct.
If the memory alloc fail the function will return -ENOMEM.
The log level be set ILITEK_DEFAULT_LOG_LEVEL.
*/
int ilitek_data_init(void) {
	ilitek_data = kzalloc(sizeof(*ilitek_data), GFP_KERNEL);
	if (ilitek_data == NULL) {
		tp_log_err("Alloc GFP_KERNEL memory failed.");
		return -ENOMEM;

	}

	memset(ilitek_data, 0, sizeof(struct ilitek_ts_data));
	//initial
	ilitek_data->ilitek_log_level_value = ILITEK_DEFAULT_LOG_LEVEL;
	tp_log_info("ilitek_log_level_value = %d.\n", ilitek_data->ilitek_log_level_value);
	ilitek_data->ilitek_repeat_start = true;
	ilitek_data->ilitek_exit_report = false;
	ilitek_debug_flag = false;
	return ILITEK_SUCCESS;
}
#if ILITEK_PLAT == ILITEK_PLAT_MTK

#define TPD_OK (0)

#ifdef MTK_UNDTS
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
struct touch_vitual_key_map_t touch_key_point_maping_array[] = { {key_1}, {key_2}, {key_3}, {key_4} };

static struct i2c_board_info __initdata ilitek_i2c_tpd = {
	I2C_BOARD_INFO(ILITEK_TS_NAME, 0x41)
};

#endif

/* probe function is used for matching and initializing input device */
static int /*__devinit*/ tpd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{

	int ret = 0;

#ifdef ILITEK_ENABLE_REGULATOR_POWER_ON
	const char *vdd_name = "vtouch";
	//    const char *vcc_i2c_name = "vcc_i2c";
#endif
	if (client == NULL) {
		tp_log_err("i2c client is NULL\n");
		return -1;
	}

	ilitek_data->client = client;
	tp_log_info("TPD probe\n");
#ifdef ILITEK_ENABLE_REGULATOR_POWER_ON
	ilitek_data->vdd = regulator_get(tpd->tpd_dev, vdd_name);
	tpd->reg = ilitek_data->vdd;
	if (IS_ERR(ilitek_data->vdd)) {
		tp_log_err("regulator_get vdd fail\n");
		ilitek_data->vdd = NULL;
		//return -EINVAL;
	} else {
		ret = regulator_set_voltage(ilitek_data->vdd, 2800000, 3300000);
		if (ret)
			tp_log_err("Could not set to 2800mv.\n");
	}

#endif
	ret = ilitek_main_probe(ilitek_data);
	if (ret == 0) {		// If probe is success, then enable the below flag.
		tpd_load_status = 1;

	}
	tp_log_info("TPD probe done\n");
	return TPD_OK;

}

static int tpd_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strcpy(info->type, TPD_DEVICE);
	return TPD_OK;

}

static int /*__devexit*/ tpd_remove(struct i2c_client *client)
{
	tp_log_info("TPD removed\n");
	return ilitek_main_remove(ilitek_data);
}

/* The I2C device list is used for matching I2C device and I2C device driver. */
static const struct i2c_device_id tpd_device_id[] = {
	{ILITEK_TS_NAME, 0},
	{},			/* should not omitted */
};

MODULE_DEVICE_TABLE(i2c, tpd_device_id);

const struct of_device_id touch_dt_match_table[] = {
	{ .compatible = "mediatek,cap_touch",},
	{},
};

MODULE_DEVICE_TABLE(of, touch_dt_match_table);

static struct i2c_driver tpd_i2c_driver = {

	.driver = {
		.name = ILITEK_TS_NAME,
		.of_match_table = of_match_ptr(touch_dt_match_table),
	},
	.probe = tpd_probe,
	.remove = tpd_remove,
	.id_table = tpd_device_id,
	.detect = tpd_detect,
};

static int tpd_local_init(void)
{
	tp_log_info("TPD init device driver\n");

	if (i2c_add_driver(&tpd_i2c_driver) != 0) {
		tp_log_err("Unable to add i2c driver.\n");
		return -1;
	}
	if (tpd_load_status == 0) {
		tp_log_err("Add error touch panel driver.\n");
		i2c_del_driver(&tpd_i2c_driver);
		return -1;
	}
#ifndef MTK_UNDTS
	if (tpd_dts_data.use_tpd_button)
		tpd_button_setting(tpd_dts_data.tpd_key_num, tpd_dts_data.tpd_key_local, tpd_dts_data.tpd_key_dim_local);

#else
	tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);	// initialize tpd button data
#endif
	tpd_type_cap = 1;
	tp_log_info("TPD init done\n");
	return TPD_OK;
}

#ifdef MTK_UNDTS
static void tpd_resume(struct early_suspend *h)
#else
static void tpd_resume(struct device *h)
#endif
{
	tp_log_info("TPD wake up\n");
	ilitek_resume();
	tp_log_info("TPD wake up done\n");
}

#ifdef MTK_UNDTS
static void tpd_suspend(struct early_suspend *h)
#else
static void tpd_suspend(struct device *h)
#endif
{
	tp_log_info("TPD enter sleep\n");
	ilitek_suspend();
	tp_log_info("TPD enter sleep done\n");
}

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = ILITEK_TS_NAME,
	.tpd_local_init = tpd_local_init,
	.suspend = tpd_suspend,
	.resume = tpd_resume,
	.tpd_have_button = 1,
	//.tpd_have_button = 0,
};

static int __init ilitek_touch_driver_init(void)
{
	printk("touch panel driver init\n");
	if(ilitek_data_init() < 0) {
		return ILITEK_FAIL;
	}
#ifdef MTK_UNDTS
	i2c_register_board_info(2, &ilitek_i2c_tpd, 1);
#else
	tpd_get_dts_info();
#endif
	if (tpd_driver_add(&tpd_device_driver) < 0)
		tp_log_err("TPD add TP driver failed\n");

	return 0;

}

static void __exit ilitek_touch_driver_exit(void)
{
	tp_log_info("touch panel driver exit\n");
	tpd_driver_remove(&tpd_device_driver);
}
#else
/* probe function is used for matching and initializing input device */
static int /*__devinit*/ ilitek_touch_driver_probe(struct i2c_client *client,
						   const struct i2c_device_id *id)
{
#if ILITEK_PLAT != ILITEK_PLAT_ALLWIN
#ifdef ILITEK_ENABLE_REGULATOR_POWER_ON
	int ret = 0;
	const char *vdd_name = "vdd";
	const char *vcc_i2c_name = "vcc_i2c";
#endif
#endif
	if (client == NULL) {
		tp_log_err("i2c client is NULL\n");
		return -1;
	}

	ilitek_data->client = client;
#if ILITEK_PLAT != ILITEK_PLAT_ALLWIN
#ifdef ILITEK_ENABLE_REGULATOR_POWER_ON
	ilitek_data->vdd = regulator_get(&client->dev, vdd_name);
	if (IS_ERR(ilitek_data->vdd)) {
		tp_log_err("regulator_get vdd fail\n");
		ilitek_data->vdd = NULL;
		//return -EINVAL;
	} else {
		ret = regulator_set_voltage(ilitek_data->vdd, 2800000, 3300000);
		if (ret)
			tp_log_err("Could not set to 2800mv.\n");

	}

	ilitek_data->vdd_i2c = regulator_get(&client->dev, vcc_i2c_name);
	if (IS_ERR(ilitek_data->vdd_i2c)) {
		tp_log_err("regulator_get vdd_i2c fail\n");
		ilitek_data->vdd_i2c = NULL;
		//return -EINVAL;
	} else {
		ret = regulator_set_voltage(ilitek_data->vdd_i2c, 1800000, 1800000);
		if (ret) {
			tp_log_err("Could not set to 1800mv.\n");
		}
	}
#endif
#endif
	return ilitek_main_probe(ilitek_data);
}

/* remove function is triggered when the input device is removed from input sub-system */
static int ilitek_touch_driver_remove(struct i2c_client *client)
{
	tp_log_info("*** %s ***\n", __func__);
	return ilitek_main_remove(ilitek_data);
}

#ifdef CONFIG_OF
static struct of_device_id ilitek_touch_match_table[] = {
	{.compatible = "tchip,ilitek",},
	{},
};

#endif
static const struct i2c_device_id ilitek_touch_device_id[] = {
	{ILITEK_TS_NAME, 0},
	{},			/* should not omitted */
};
MODULE_DEVICE_TABLE(i2c, ilitek_touch_device_id);
#ifdef CONFIG_ACPI
static const struct acpi_device_id ilitekts_acpi_id[] = {
	{"ILTK0001", 0},
	{}
};
MODULE_DEVICE_TABLE(acpi, ilitekts_acpi_id);
#endif
static struct i2c_driver ilitek_touch_device_driver = {
	.driver = {
		.name = ILITEK_TS_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = ilitek_touch_match_table,
#endif
#ifdef CONFIG_ACPI
		.acpi_match_table = ACPI_PTR(ilitekts_acpi_id),
#endif
	},
	.probe = ilitek_touch_driver_probe,
	.remove = ilitek_touch_driver_remove,
	.id_table = ilitek_touch_device_id,
};

#if ILITEK_PLAT == ILITEK_PLAT_ALLWIN
static const unsigned short normal_i2c[2] = { 0x41, I2C_CLIENT_END };
struct ctp_config_info config_info = {
	.input_type = CTP_TYPE,
	.name = NULL,
	.int_number = 0,
};

static int twi_id;
static int screen_max_x;
static int screen_max_y;
static int revert_x_flag;
static int revert_y_flag;
static int exchange_x_y_flag;
static int ctp_get_system_config(void)
{
	twi_id = config_info.twi_id;
	screen_max_x = config_info.screen_max_x;
	screen_max_y = config_info.screen_max_y;
	tp_log_info("Ilitek: screen_max_x = %d\n", screen_max_x);
	revert_x_flag = config_info.revert_x_flag;
	revert_y_flag = config_info.revert_y_flag;
	exchange_x_y_flag = config_info.exchange_x_y_flag;
	if ((screen_max_x == 0) || (screen_max_y == 0)) {
		tp_log_err("%s:read config error!\n", __func__);
		return -1;
	}
	return 0;
}

int ctp_ts_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;

	if (twi_id == adapter->nr) {
		strlcpy(info->type, ILITEK_TS_NAME, I2C_NAME_SIZE);
		return 0;
	} else {
		return -ENODEV;
	}
}

static struct i2c_board_info i2c_info_dev = {
	I2C_BOARD_INFO(ILITEK_TS_NAME, 0x41),
	.platform_data = NULL,
};

static int add_ctp_device(void)
{
	struct i2c_adapter *adap;
	//script_parser_fetch("ctp_para", "ctp_twi_id", &twi_id, 1);
	adap = i2c_get_adapter(twi_id);
	i2c_new_device(adap, &i2c_info_dev);
	return 0;
}

static int ilitek_init_allwin(void)
{

	int ret = 0;

	if (input_fetch_sysconfig_para(&(config_info.input_type))) {
		tp_log_err("Ilitek:  ctp_fetch_sysconfig_para err.\n");
		return -1;
	} else {
		ret = input_init_platform_resource(&(config_info.input_type));
		if (0 != ret)
			tp_log_err("Ilitek: ctp_ops.init_platform_resource err.\n");
	}

	if (config_info.ctp_used == 0) {
		tp_log_err("Ilitek: *** if use ctp,please put the sys_config.fex ctp_used set to 1.\n");
		return -1;
	}

	if (ctp_get_system_config() < 0)
		tp_log_err("Ilitek: %s:read config fail!\n", __func__);

	add_ctp_device();
	ilitek_touch_device_driver.address_list = normal_i2c;
	ilitek_touch_device_driver.detect = ctp_ts_detect;
	return 0;

}

#endif
static int __init ilitek_touch_driver_init(void)
{
	int ret;
	if(ilitek_data_init() < 0) {
		return ILITEK_FAIL;
	}
#if ILITEK_PLAT == ILITEK_PLAT_ALLWIN
	ret = ilitek_init_allwin();
	if (ret < 0) {
		tp_log_err("ilitek_init_allwin failed.\n");
		return -ENODEV;
	}
#endif
	/* register driver */
	ret = i2c_add_driver(&ilitek_touch_device_driver);
	if (ret != 0) {
		tp_log_err("add touch device driver i2c driver failed.so remove\n");
		i2c_del_driver(&ilitek_touch_device_driver);
		return -ENODEV;
	}
	tp_log_info("add touch device driver i2c driver.\n");
	return ret;
}

static void __exit ilitek_touch_driver_exit(void)
{
	tp_log_info("remove touch device driver i2c driver.\n");
	i2c_del_driver(&ilitek_touch_device_driver);
}
#endif
module_init(ilitek_touch_driver_init);
module_exit(ilitek_touch_driver_exit);
MODULE_AUTHOR("ILITEK");
MODULE_LICENSE("GPL");
