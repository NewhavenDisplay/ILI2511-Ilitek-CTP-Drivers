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

#ifndef __ILITEK_TS_H__
#define __ILITEK_TS_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/rtc.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/pm_runtime.h>
#include <linux/ctype.h>
#include <linux/errno.h>
#include <linux/wakelock.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#if defined(CONFIG_FB)
	#include <linux/notifier.h>
	#include <linux/fb.h>
	#define 	ILITEK_BLANK_POWERDOWN 										FB_BLANK_POWERDOWN
	#define 	ILITEK_BLANK_UNBLANK 										FB_BLANK_UNBLANK
	#define 	ILITEK_EVENT_BLANK 											FB_EVENT_BLANK
	#define 	ILITEK_BLANK_NORMAL 										FB_BLANK_NORMAL
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	#include <linux/earlysuspend.h>
#endif

#define ILITEK_PLAT_QCOM											1
#define ILITEK_PLAT_MTK												2
#define ILITEK_PLAT_ROCKCHIP										3
#define ILITEK_PLAT_ALLWIN											4
#define ILITEK_PLAT_AMLOGIC											5
#define ILITEK_PLAT													ILITEK_PLAT_ROCKCHIP
//#define CONFIG_QCOM_DRM
#if ILITEK_PLAT == ILITEK_PLAT_QCOM
	#ifdef CONFIG_QCOM_DRM
		#include <linux/msm_drm_notify.h>
		#define 	ILITEK_BLANK_POWERDOWN 										MSM_DRM_BLANK_POWERDOWN
		#define 	ILITEK_BLANK_UNBLANK 										MSM_DRM_BLANK_UNBLANK
		#define 	ILITEK_EVENT_BLANK 											MSM_DRM_EVENT_BLANK
		#define 	ILITEK_BLANK_NORMAL 										MSM_DRM_BLANK_UNBLANK
	#endif
#endif

#define ILITEK_TOOL

#define POWER_SUPPLY_BATTERY_STATUS_PATCH  "/sys/class/power_supply/battery/status"

//#define ILITEK_ESD_PROTECTION
//#define ILITEK_CHECK_FUNCMODE
#define ILITEK_TOUCH_PROTOCOL_B
//#define ILITEK_REPORT_PRESSURE
//#define ILITEK_USE_LCM_RESOLUTION

#define ILITEK_ROTATE_FLAG											0
#define ILITEK_REVERT_X												0
#define ILITEK_REVERT_Y												0
#define TOUCH_SCREEN_X_MAX   										(1080)	//LCD_WIDTH
#define TOUCH_SCREEN_Y_MAX   										(1920)	//LCD_HEIGHT
#define ILITEK_RESOLUTION_MAX										16384
#define ILITEK_ENABLE_REGULATOR_POWER_ON
#define ILITEK_GET_GPIO_NUM

#define ILITEK_CLICK_WAKEUP											0
#define ILITEK_DOUBLE_CLICK_WAKEUP									1
#define ILITEK_GESTURE_WAKEUP										2
//#define ILITEK_GESTURE                                            ILITEK_CLICK_WAKEUP
#ifdef ILITEK_GESTURE
#define ILITEK_GET_TIME_FUNC_WITH_TIME								0
#define ILITEK_GET_TIME_FUNC_WITH_JIFFIES							1
#define ILITEK_GET_TIME_FUNC										ILITEK_GET_TIME_FUNC_WITH_JIFFIES
#endif

#define DOUBLE_CLICK_DISTANCE										1000
#define DOUBLE_CLICK_ONE_CLICK_USED_TIME							800
#define DOUBLE_CLICK_NO_TOUCH_TIME									1000
#define DOUBLE_CLICK_TOTAL_USED_TIME								(DOUBLE_CLICK_NO_TOUCH_TIME + (DOUBLE_CLICK_ONE_CLICK_USED_TIME * 2))

#define ILITEK_SLEEP												0
#define ILITEK_POWEROFF												1
#define ILITEK_LOW_POWER											ILITEK_SLEEP

//#define ILITEK_UPDATE_FW
#define RAW_DATA_TRANSGER_LENGTH                                    1024	//This is the setting of "Lego series" get raw data length.

#define UPGRADE_LENGTH_BLV1_8               						2048

#define ILI_UPDATE_BY_CHECK_INT
//#define CHROME_OS

#define ILITEK_TS_NAME												"ilitek_ts"
#define ILITEK_TUNING_MESSAGE										0xDB
#define ILITEK_TUNING_NODE											0xDB


#if ILITEK_PLAT == ILITEK_PLAT_MTK
//#define MTK_UNDTS //no use dts and for mtk old version
#define ILITEK_ENABLE_DMA
#define ILITEK_USE_MTK_INPUT_DEV
#if defined ILITEK_GET_GPIO_NUM
#undef ILITEK_GET_GPIO_NUM
#endif

#include <linux/sched.h>
#include <linux/kthread.h>
//#include <linux/rtpm_prio.h>
#include <linux/wait.h>
#include <linux/time.h>

#include <linux/namei.h>
#include <linux/vmalloc.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/module.h>

#ifdef MTK_UNDTS

#define TPD_KEY_COUNT   4
#define key_1           {60, 17000}	//auto define
#define key_2           {180, 17000}
#define key_3           {300, 17000}
#define key_4           {420, 17000}

#define TPD_KEYS        {KEY_MENU, KEY_HOMEPAGE, KEY_BACK, KEY_SEARCH}	//change for you panel key info
#define TPD_KEYS_DIM    {{key_1, 50, 30 }, {key_2, 50, 30 }, {key_3, 50, 30 }, {key_4, 50, 30 } }

#define ABSSUB(a, b) ((a > b) ? (a - b) : (b - a))

struct touch_vitual_key_map_t {
	int point_x;
	int point_y;
};

extern struct touch_vitual_key_map_t touch_key_point_maping_array[];

#include <mach/mt_pm_ldo.h>
#include <cust_eint.h>

#include "cust_gpio_usage.h"
#include <mach/mt_gpio.h>
//#include <mach/mt_reg_base.h>
#include <mach/mt_typedefs.h>
//#include <mach/eint.h>
#include <pmic_drv.h>
#include <mach/mt_boot.h>

#include <linux/dma-mapping.h>

#else
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>

#ifdef CONFIG_MTK_BOOT
#include "mt_boot_common.h"
#endif

#endif

#include "tpd.h"
extern struct tpd_device *tpd;
#endif

#if ILITEK_PLAT == ILITEK_PLAT_ALLWIN
#include <linux/irq.h>
#include <linux/init-input.h>
#include <linux/pm.h>
#include <linux/gpio.h>
extern struct ctp_config_info config_info;

#endif

#ifndef ILITEK_GET_GPIO_NUM
//must set
#if ILITEK_PLAT == ILITEK_PLAT_MTK
#ifdef MTK_UNDTS
#define ILITEK_IRQ_GPIO												GPIO_CTP_EINT_PIN
#define ILITEK_RESET_GPIO 											GPIO_CTP_RST_PIN
#else
#define ILITEK_IRQ_GPIO												GTP_INT_PORT
#define ILITEK_RESET_GPIO 											GTP_RST_PORT
#endif
#elif ILITEK_PLAT == ILITEK_PLAT_ALLWIN
#define ILITEK_IRQ_GPIO												(config_info.int_number)
#define ILITEK_RESET_GPIO											(config_info.wakeup_gpio.gpio)
#else
#define ILITEK_IRQ_GPIO												9
#define ILITEK_RESET_GPIO											10
#endif
#endif

#ifdef ILITEK_TUNING_MESSAGE
#include <linux/init.h>
#include <net/sock.h>
#include <net/netlink.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <linux/sched.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/icmp.h>
#include <linux/udp.h>
#endif

#define ILITEK_I2C_RETRY_COUNT											3
#if ILITEK_PLAT == ILITEK_PLAT_ALLWIN
extern int ilitek_suspend_allwin(struct i2c_client *client, pm_message_t mesg);
extern int ilitek_resume_allwin(struct i2c_client *client);
#endif

#define ERR_ALLOC_MEM(X)	((IS_ERR(X) || X == NULL) ? 1 : 0)

static inline void ilitek_kfree(void **mem)
{
	if (*mem != NULL) {
		kfree(*mem);
		*mem = NULL;
	}
}

static inline void ipio_vfree(void **mem)
{
	if (*mem != NULL) {
		vfree(*mem);
		*mem = NULL;
	}
}
#endif
