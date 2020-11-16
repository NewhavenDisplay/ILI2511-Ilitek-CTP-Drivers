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
#include "ilitek_protocol.h"

char ilitek_driver_information[] = { DERVER_VERSION_MAJOR, DERVER_VERSION_MINOR, CUSTOMER_ID, MODULE_ID, PLATFORM_ID, PLATFORM_MODULE, ENGINEER_ID };
#if ILITEK_PLAT == ILITEK_PLAT_MTK
extern struct tpd_device *tpd;
#ifdef ILITEK_ENABLE_DMA
static uint8_t *I2CDMABuf_va;
static dma_addr_t I2CDMABuf_pa;
#endif
#endif
#ifdef ILITEK_GESTURE
struct wake_lock ilitek_wake_lock;
#endif
#ifdef ILITEK_TUNING_MESSAGE
static struct sock *ilitek_netlink_sock;
bool ilitek_debug_flag;
static void ilitek_udp_reply(int pid, int seq, void *payload, int size)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int len = NLMSG_SPACE(size);
	void *data;
	int ret;

	tp_log_debug("udp_reply\n");
	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb) {
		tp_log_info("alloc skb error\n");
		return;
	}
	//tp_log_info("ilitek udp_reply\n");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
	nlh = nlmsg_put(skb, pid, seq, 0, size, 0);
#else
	nlh = NLMSG_PUT(skb, pid, seq, 0, size);
#endif
	nlh->nlmsg_flags = 0;
	data = NLMSG_DATA(nlh);
	memcpy(data, payload, size);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
	NETLINK_CB(skb).portid = 0;	/* from kernel */
#else
	NETLINK_CB(skb).pid = 0;	/* from kernel */
#endif
	NETLINK_CB(skb).dst_group = 0;	/* unicast */
	ret = netlink_unicast(ilitek_netlink_sock, skb, pid, MSG_DONTWAIT);
	if (ret < 0) {
		tp_log_err("ilitek send failed\n");
		return;
	}
	return;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
nlmsg_failure:			/* Used by NLMSG_PUT */
	if (skb)
		kfree_skb(skb);
#endif
}

/* Receive messages from netlink socket. */
static u_int ilitek_pid = 100, ilitek_seq = 23; /*, sid */
static void udp_receive(struct sk_buff *skb)
{
	int count = 0, ret = 0, i = 0;
	uint8_t *data;
	struct nlmsghdr *nlh;

	nlh = (struct nlmsghdr *)skb->data;
	ilitek_pid = 100;	//NETLINK_CREDS(skb)->pid;
	//uid  = NETLINK_CREDS(skb)->uid;
	//sid  = NETLINK_CB(skb).sid;
	ilitek_seq = 23;	//nlh->nlmsg_seq;
	data = (uint8_t *) NLMSG_DATA(nlh);
	count = nlmsg_len(nlh);
	if (!strcmp(data, "Open!")) {
		tp_log_info("data is :%s\n", (char *)data);
		ilitek_data->operation_protection = true;
		ilitek_udp_reply(ilitek_pid, ilitek_seq, data, sizeof("Open!"));
	} else if (!strcmp(data, "Close!")) {
		tp_log_info("data is :%s\n", (char *)data);
		ilitek_data->operation_protection = false;
	}
	tp_log_debug("count = %d  data[count -3] = %d data[count -2] = %c\n", count, data[count - 3], data[count - 2]);
	for (i = 0; i < count; i++) {
		//tp_log_info("data[%d] = 0x%x\n", i, data[i]);
	}
	if (data[count - 2] == 'I' && (count == 20 || count == 52) && data[0] == 0x77 && data[1] == 0x77) {

		tp_log_debug("IOCTL_WRITE CMD = %d\n", data[2]);
		switch (data[2]) {
		case 13:
			//ilitek_irq_enable();
			tp_log_info("ilitek_irq_enable. do nothing\n");
			break;
		case 12:
			//ilitek_irq_disable();
			tp_log_info("ilitek_irq_disable. do nothing\n");
			break;
		case 19:
			ilitek_reset(ilitek_data->reset_time, false);
			break;
#ifdef ILITEK_TUNING_MESSAGE
		case 21:
			tp_log_info("ilitek The ilitek_debug_flag = %d.\n", data[3]);
			if (data[3] == 0) {
				ilitek_debug_flag = false;
			} else if (data[3] == 1) {
				ilitek_debug_flag = true;
			}
			break;
#endif
		case 15:
			if (data[3] == 0) {
				ilitek_irq_disable();
				tp_log_debug("ilitek_irq_disable.\n");
			} else {
				ilitek_irq_enable();
				tp_log_debug("ilitek_irq_enable.\n");
			}
			break;
		case 16:
			ilitek_data->operation_protection = data[3];
			tp_log_info("ilitek_data->operation_protection = %d\n", ilitek_data->operation_protection);
			break;
		case 8:
			tp_log_info("get driver version\n");
			ilitek_udp_reply(ilitek_pid, ilitek_seq, ilitek_driver_information, 7);
			break;
		case 18:
			tp_log_debug("firmware update write 33 bytes data\n");
			ret = ilitek_i2c_write(&data[3], 33);
			if (ret < 0)
				tp_log_err("i2c write error, ret %d, addr %x\n", ret, ilitek_data->client->addr);
			if (ret < 0) {
				data[0] = 1;
			} else {
				data[0] = 0;
			}
			ilitek_udp_reply(ilitek_pid, ilitek_seq, data, 1);
			return;
			break;
		default:
			return;
		}
	} else if (data[count - 2] == 'W') {
		ret = ilitek_i2c_write(data, count - 2);
		if (ret < 0)
			tp_log_err("i2c write error, ret %d, addr %x\n", ret, ilitek_data->client->addr);
		if (ret < 0) {
			data[0] = 1;
		} else {
			data[0] = 0;
		}
		ilitek_udp_reply(ilitek_pid, ilitek_seq, data, 1);
	} else if (data[count - 2] == 'R') {
		ret = ilitek_i2c_read(data, count - 2);
		if (ret < 0)
			tp_log_err("i2c read error, ret %d, addr %x\n", ret, ilitek_data->client->addr);
		if (ret < 0) {
			data[count - 2] = 1;
		} else {
			data[count - 2] = 0;
		}
		ilitek_udp_reply(ilitek_pid, ilitek_seq, data, count - 1);
	}
	return;
}
#endif

#ifdef ILITEK_ESD_PROTECTION
static void ilitek_esd_check(struct work_struct *work)
{
	int i = 0;
	uint8_t buf[4] = { 0 };

	tp_log_info("enter.......\n");
	if (ilitek_data->operation_protection) {
		tp_log_info("ilitek esd ilitek_data->operation_protection is true so not check\n");
		goto ilitek_esd_check_out;
	}
	mutex_lock(&ilitek_data->ilitek_mutex);
	buf[0] = ILITEK_TP_CMD_GET_PROTOCOL_VERSION;
	if (ilitek_data->esd_check) {
		for (i = 0; i < 3; i++) {
			if (ilitek_i2c_write_and_read(buf, 1, 0, buf, 2) < 0) {
				tp_log_err("ilitek esd  i2c communication error\n");
				if (i == 2) {
					tp_log_err("esd i2c communication failed three times reset now\n");
					break;
				}
			} else {
				if (buf[0] == 0x03) {
					tp_log_info("esd ilitek_ts_send_cmd successful, response ok\n");
					goto ilitek_esd_check_out;
				} else {
					tp_log_err("esd ilitek_ts_send_cmd successful, response failed\n");
					if (i == 2) {
						tp_log_err("esd ilitek_ts_send_cmd successful, response failed three times reset now\n");
						break;
					}
				}
			}
		}
	} else {
		tp_log_info("esd not need check ilitek_data->esd_check is false!!!\n");
		goto ilitek_esd_check_out;
	}

	ilitek_reset(ilitek_data->reset_time, false);
ilitek_esd_check_out:
	mutex_unlock(&ilitek_data->ilitek_mutex);
	ilitek_data->esd_check = true;
	queue_delayed_work(ilitek_data->esd_wq, &ilitek_data->esd_work, ilitek_data->esd_delay);
}
#endif

static DECLARE_WAIT_QUEUE_HEAD(waiter);

void ilitek_irq_enable(void)
{
	unsigned long irqflag = 0;

	spin_lock_irqsave(&ilitek_data->irq_lock, irqflag);
	if (!(ilitek_data->irq_status)) {
#ifdef MTK_UNDTS
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#else
		enable_irq(ilitek_data->client->irq);
#endif
		ilitek_data->irq_status = true;
		tp_log_debug("\n");
	}
	spin_unlock_irqrestore(&ilitek_data->irq_lock, irqflag);
}

void ilitek_irq_disable(void)
{
	unsigned long irqflag = 0;

	spin_lock_irqsave(&ilitek_data->irq_lock, irqflag);
	if ((ilitek_data->irq_status)) {
#ifdef MTK_UNDTS
		mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#else
		disable_irq(ilitek_data->client->irq);
#endif
		ilitek_data->irq_status = false;
		tp_log_info("\n");
	}
	spin_unlock_irqrestore(&ilitek_data->irq_lock, irqflag);
}

#ifdef ILITEK_ENABLE_DMA
static int ilitek_dma_i2c_read(struct i2c_client *client, uint8_t *buf, int len)
{
	int i = 0, err = 0;

	if (len < 8) {
		client->ext_flag = client->ext_flag & (~I2C_DMA_FLAG);
		//client->addr = client->addr & I2C_MASK_FLAG;
		return i2c_master_recv(client, buf, len);
	}
	client->ext_flag = client->ext_flag | I2C_DMA_FLAG;
	//client->addr = (client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG;
	err = i2c_master_recv(client, (uint8_t *)I2CDMABuf_pa, len);
	if (err < 0)
		return err;
	for (i = 0; i < len; i++)
		buf[i] = I2CDMABuf_va[i];
	return err;
}

static int ilitek_dma_i2c_write(struct i2c_client *client, uint8_t *pbt_buf, int dw_len)
{
	int i = 0;

	if (dw_len <= 8) {
		client->ext_flag = client->ext_flag & (~I2C_DMA_FLAG);
		//client->addr = client->addr & I2C_MASK_FLAG;
		return i2c_master_send(client, pbt_buf, dw_len);
	}
	for (i = 0; i < dw_len; i++)
		I2CDMABuf_va[i] = pbt_buf[i];
	client->ext_flag = client->ext_flag | I2C_DMA_FLAG;
	//client->addr = (client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG;
	return i2c_master_send(client, (uint8_t *)I2CDMABuf_pa, dw_len);
}
#endif

static int ilitek_i2c_transfer(struct i2c_msg *msgs, int cnt)
{
	int ret = 0;
	struct i2c_client *client = ilitek_data->client;
	int count = ILITEK_I2C_RETRY_COUNT;
#ifdef ILITEK_ENABLE_DMA
	int i = 0;

	for (i = 0; i < cnt; i++) {
		while (count >= 0) {
			count -= 1;
			msgs[i].ext_flag = 0;
			if (msgs[i].flags == I2C_M_RD)
				ret = ilitek_dma_i2c_read(client, msgs[i].buf, msgs[i].len);
			else if (msgs[i].flags == 0)
				ret = ilitek_dma_i2c_write(client, msgs[i].buf, msgs[i].len);

			if (ret < 0) {
				tp_log_err("ilitek i2c transfer err\n");
				mdelay(20);
				continue;
			}
			break;
		}
	}
#else
#if ILITEK_PLAT == ILITEK_PLAT_ROCKCHIP
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)
	int i = 0;

	for (i = 0; i < cnt; i++)
		msgs[i].scl_rate = 400000;
#endif
#endif
	while (count >= 0) {
		count -= 1;
		ret = i2c_transfer(client->adapter, msgs, cnt);
		if (ret < 0) {
			tp_log_err("ilitek_i2c_transfer err\n");
			mdelay(20);
			continue;
		}
		break;
	}
#endif
	return ret;
}

int ilitek_i2c_write(uint8_t *cmd, int length)
{
	int ret = 0;
	struct i2c_client *client = ilitek_data->client;
	struct i2c_msg msgs[] = {
		{.addr = client->addr, .flags = 0, .len = length, .buf = cmd,}
	};

	ret = ilitek_i2c_transfer(msgs, 1);
	if (ret < 0)
		tp_log_err("%s, i2c write error, ret %d\n", __func__, ret);
	return ret;
}

int ilitek_i2c_read(uint8_t *data, int length)
{
	int ret = 0;
	struct i2c_client *client = ilitek_data->client;
	struct i2c_msg msgs_ret[] = {
		{.addr = client->addr, .flags = I2C_M_RD, .len = length, .buf = data,}
	};

	ret = ilitek_i2c_transfer(msgs_ret, 1);
	if (ret < 0)
		tp_log_err("%s, i2c read error, ret %d\n", __func__, ret);

	return ret;
}

int ilitek_i2c_write_and_read(uint8_t *cmd, int write_len, int delay, uint8_t *data, int read_len)
{
	int ret = 0;
	struct i2c_client *client = ilitek_data->client;
	struct i2c_msg msgs_send[] = {
		{.addr = client->addr, .flags = 0, .len = write_len, .buf = cmd,},
		{.addr = client->addr, .flags = I2C_M_RD, .len = read_len, .buf = data,}
	};
	struct i2c_msg msgs_receive[] = {
		{.addr = client->addr, .flags = I2C_M_RD, .len = read_len, .buf = data,}
	};

	if (ilitek_data->ilitek_repeat_start) {
		if (read_len == 0) {
			if (write_len > 0) {
				ret = ilitek_i2c_transfer(msgs_send, 1);
				if (ret < 0)
					tp_log_err("%s, i2c write error, ret = %d\n", __func__, ret);
			}
			if (delay > 0)
				mdelay(delay);
		} else if (write_len == 0) {
			if (read_len > 0) {
				ret = ilitek_i2c_transfer(msgs_receive, 1);
				if (ret < 0)
					tp_log_err("%s, i2c read error, ret = %d\n", __func__, ret);
				if (delay > 0)
					mdelay(delay);
			}
		} else if (delay > 0) {
			if (write_len > 0) {
				ret = ilitek_i2c_transfer(msgs_send, 1);
				if (ret < 0)
					tp_log_err("%s, i2c write error, ret = %d\n", __func__, ret);
			}
			if (delay > 0)
				mdelay(delay);
			if (read_len > 0) {
				ret = ilitek_i2c_transfer(msgs_receive, 1);
				if (ret < 0)
					tp_log_err("%s, i2c read error, ret = %d\n", __func__, ret);
			}
		} else {
			ret = ilitek_i2c_transfer(msgs_send, 2);
			if (ret < 0)
				tp_log_err("%s, i2c repeat start error, ret = %d\n", __func__, ret);
		}
	} else {
		if (write_len > 0) {
			ret = ilitek_i2c_transfer(msgs_send, 1);
			if (ret < 0)
				tp_log_err("%s, i2c write error, ret = %d\n", __func__, ret);
		}
		if (delay > 0)
			mdelay(delay);
		if (read_len > 0) {
			ret = ilitek_i2c_transfer(msgs_receive, 1);
			if (ret < 0)
				tp_log_err("%s, i2c read error, ret = %d\n", __func__, ret);
		}
	}
	return ret;
}

int ilitek_poll_int(void)
{
#ifdef MTK_UNDTS
	return mt_get_gpio_in(ILITEK_IRQ_GPIO);
#else
	return gpio_get_value(ilitek_data->irq_gpio);
#endif
}

void ilitek_reset(int delay, bool boot)
{
	bool init_stats = ilitek_data->irq_status;

	tp_log_info("delay = %d\n", delay);
	if(boot){
		tp_log_info("IRQ has not been registered\n");
	}
	else
		ilitek_irq_disable();
#ifdef MTK_UNDTS
	mt_set_gpio_mode(ILITEK_RESET_GPIO, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(ILITEK_RESET_GPIO, GPIO_DIR_OUT);
	mt_set_gpio_out(ILITEK_RESET_GPIO, GPIO_OUT_ONE);
	mdelay(10);

	mt_set_gpio_mode(ILITEK_RESET_GPIO, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(ILITEK_RESET_GPIO, GPIO_DIR_OUT);
	mt_set_gpio_out(ILITEK_RESET_GPIO, GPIO_OUT_ZERO);
	mdelay(10);

	mt_set_gpio_mode(ILITEK_RESET_GPIO, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(ILITEK_RESET_GPIO, GPIO_DIR_OUT);
	mt_set_gpio_out(ILITEK_RESET_GPIO, GPIO_OUT_ONE);
	mdelay(delay);

#else

	if (ilitek_data->reset_gpio >= 0) {
#if ILITEK_PLAT != ILITEK_PLAT_MTK
		gpio_direction_output(ilitek_data->reset_gpio, 1);
		mdelay(10);
		gpio_direction_output(ilitek_data->reset_gpio, 0);
		mdelay(10);
		gpio_direction_output(ilitek_data->reset_gpio, 1);
		mdelay(delay);
#else
		tpd_gpio_output(ilitek_data->reset_gpio, 1);
		mdelay(10);
		tpd_gpio_output(ilitek_data->reset_gpio, 0);
		mdelay(10);
		tpd_gpio_output(ilitek_data->reset_gpio, 1);
		mdelay(delay);
#endif
	} else {
		tp_log_err("reset pin is invalid\n");
	}
	if(init_stats)
		ilitek_irq_enable();
#endif
}

#if ILITEK_PLAT != ILITEK_PLAT_ALLWIN
#ifdef ILITEK_ENABLE_REGULATOR_POWER_ON
static void ilitek_regulator_release(void)
{
	if (ilitek_data->vdd)
		regulator_put(ilitek_data->vdd);
	if (ilitek_data->vdd_i2c)
		regulator_put(ilitek_data->vdd_i2c);
}
#endif
#endif
static int ilitek_free_gpio(void)
{

#ifndef MTK_UNDTS
	if (gpio_is_valid(ilitek_data->reset_gpio)) {
		tp_log_info("reset_gpio is valid so free\n");
		gpio_free(ilitek_data->reset_gpio);
	}
	if (gpio_is_valid(ilitek_data->irq_gpio)) {
		tp_log_info("irq_gpio is valid so free\n");
		gpio_free(ilitek_data->irq_gpio);
	}
#endif

	return 0;
}

static int ilitek_set_input_param(void)
{
	int ret = 0;
	int i = 0;
	struct input_dev *input = ilitek_data->input_dev;

	tp_log_debug("ilitek_set_input_param\n");
#ifdef ILITEK_USE_MTK_INPUT_DEV
#ifndef MTK_UNDTS
	if (tpd_dts_data.use_tpd_button) {
		for (i = 0; i < tpd_dts_data.tpd_key_num; i++)
			input_set_capability(input, EV_KEY, tpd_dts_data.tpd_key_local[i]);
	}
#endif
#else
	__set_bit(INPUT_PROP_DIRECT, input->propbit);
	input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

#if !ILITEK_ROTATE_FLAG
#ifdef ILITEK_USE_LCM_RESOLUTION
	input_set_abs_params(input, ABS_MT_POSITION_X, 0, TOUCH_SCREEN_X_MAX, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, 0, TOUCH_SCREEN_Y_MAX, 0, 0);
#else
	input_set_abs_params(input, ABS_MT_POSITION_X, ilitek_data->screen_min_x, ilitek_data->screen_max_x, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, ilitek_data->screen_min_y, ilitek_data->screen_max_y, 0, 0);
#endif
#else
#ifdef ILITEK_USE_LCM_RESOLUTION
	input_set_abs_params(input, ABS_MT_POSITION_X, 0, TOUCH_SCREEN_Y_MAX, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, 0, TOUCH_SCREEN_X_MAX, 0, 0);
#else
	input_set_abs_params(input, ABS_MT_POSITION_X, ilitek_data->screen_min_y, ilitek_data->screen_max_y, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, ilitek_data->screen_min_x, ilitek_data->screen_max_x, 0, 0);
#endif
#endif
	input_set_abs_params(input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);

	input->name = ILITEK_TS_NAME;
	input->id.bustype = BUS_I2C;
	input->dev.parent = &(ilitek_data->client)->dev;
#endif

#ifdef ILITEK_TOUCH_PROTOCOL_B
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
	input_mt_init_slots(input, ilitek_data->max_tp, INPUT_MT_DIRECT);
#else
	input_mt_init_slots(input, ilitek_data->max_tp);
#endif
#else
	input_set_abs_params(input, ABS_MT_TRACKING_ID, 0, ilitek_data->max_tp, 0, 0);
#endif
#ifdef ILITEK_REPORT_PRESSURE
	input_set_abs_params(input, ABS_MT_PRESSURE, 0, 255, 0, 0);
#endif

	for (i = 0; i < ilitek_data->keycount; i++)
		set_bit(ilitek_data->keyinfo[i].id & KEY_MAX, input->keybit);
#ifdef ILITEK_GESTURE
	input_set_capability(input, EV_KEY, KEY_POWER);
#endif

#ifndef ILITEK_USE_MTK_INPUT_DEV
	ret = input_register_device(ilitek_data->input_dev);
	if (ret)
		tp_log_err("register input device, error\n");
#endif
	return ret;
}

static int ilitek_touch_down(int id, int x, int y, int p, int h, int w)
{
	struct input_dev *input = ilitek_data->input_dev;
	if (y != ILITEK_RESOLUTION_MAX) {

#if defined(ILITEK_USE_MTK_INPUT_DEV) || defined(ILITEK_USE_LCM_RESOLUTION)
		x = (x - ilitek_data->screen_min_x) * TOUCH_SCREEN_X_MAX / (ilitek_data->screen_max_x - ilitek_data->screen_min_x);
		y = (y - ilitek_data->screen_min_y) * TOUCH_SCREEN_Y_MAX / (ilitek_data->screen_max_y - ilitek_data->screen_min_y);
#endif
	}
	input_report_key(input, BTN_TOUCH, 1);
#ifdef ILITEK_TOUCH_PROTOCOL_B
	input_mt_slot(input, id);
	input_mt_report_slot_state(input, MT_TOOL_FINGER, true);
#endif
#if !ILITEK_ROTATE_FLAG
	input_event(input, EV_ABS, ABS_MT_POSITION_X, x);
	input_event(input, EV_ABS, ABS_MT_POSITION_Y, y);
#else
	input_event(input, EV_ABS, ABS_MT_POSITION_X, y);
	input_event(input, EV_ABS, ABS_MT_POSITION_Y, x);
#endif
	input_event(input, EV_ABS, ABS_MT_TOUCH_MAJOR, h);
	input_event(input, EV_ABS, ABS_MT_TOUCH_MAJOR, w);
#ifdef ILITEK_REPORT_PRESSURE
	input_event(input, EV_ABS, ABS_MT_PRESSURE, p);
#endif
#ifndef ILITEK_TOUCH_PROTOCOL_B
	input_event(input, EV_ABS, ABS_MT_TRACKING_ID, id);
	input_mt_sync(input);
#endif

#if ILITEK_PLAT == ILITEK_PLAT_MTK
#ifdef CONFIG_MTK_BOOT
#ifndef MTK_UNDTS
	if (tpd_dts_data.use_tpd_button) {
		if (FACTORY_BOOT == get_boot_mode() || RECOVERY_BOOT == get_boot_mode()) {
			tpd_button(x, y, 1);
			tp_log_debug("tpd_button(x, y, 1) = tpd_button(%d, %d, 1)\n", x, y);
		}
	}
#endif
#endif
#endif
	return 0;
}

static int ilitek_touch_release(int id)
{
	struct input_dev *input = ilitek_data->input_dev;
#ifdef ILITEK_TOUCH_PROTOCOL_B
	if (ilitek_data->touch_flag[id] == 1) {
		tp_log_debug("release point id = %d\n", id);
		input_mt_slot(input, id);
		input_mt_report_slot_state(input, MT_TOOL_FINGER, false);
	}
#else
	input_report_key(input, BTN_TOUCH, 0);
	input_mt_sync(input);
#endif
	ilitek_data->touch_flag[id] = 0;
#if ILITEK_PLAT == ILITEK_PLAT_MTK
#ifdef CONFIG_MTK_BOOT
#ifndef MTK_UNDTS
	if (tpd_dts_data.use_tpd_button) {
		if (FACTORY_BOOT == get_boot_mode() || RECOVERY_BOOT == get_boot_mode()) {
			tpd_button(0, 0, 0);
			tp_log_debug("tpd_button(x, y, 0) = tpd_button(%d, %d, 0)\n", 0, 0);
		}
	}
#endif
#endif
#endif

	return 0;
}

static int ilitek_touch_release_all_point(void)
{
	struct input_dev *input = ilitek_data->input_dev;
	int i = 0;

#ifdef ILITEK_TOUCH_PROTOCOL_B
	input_report_key(input, BTN_TOUCH, 0);
	for (i = 0; i < ilitek_data->max_tp; i++)
		ilitek_touch_release(i);
#else
	for (i = 0; i < ilitek_data->max_tp; i++)
		ilitek_data->touch_flag[i] = 0;
	ilitek_touch_release(0);
#endif
	ilitek_data->is_touched = false;
	input_sync(input);
	return 0;
}

static int ilitek_check_key_down(int x, int y)
{
	int j = 0;
	for (j = 0; j < ilitek_data->keycount; j++) {
		if ((x >= ilitek_data->keyinfo[j].x && x <= ilitek_data->keyinfo[j].x + ilitek_data->key_xlen) &&
		    (y >= ilitek_data->keyinfo[j].y && y <= ilitek_data->keyinfo[j].y + ilitek_data->key_ylen)) {
#if ILITEK_PLAT != ILITEK_PLAT_MTK
			input_report_key(ilitek_data->input_dev, ilitek_data->keyinfo[j].id, 1);
#else
#ifndef MTK_UNDTS
			if (tpd_dts_data.use_tpd_button) {
				x = tpd_dts_data.tpd_key_dim_local[j].key_x;
				y = tpd_dts_data.tpd_key_dim_local[j].key_y;
				tp_log_debug("key index=%x, tpd_dts_data.tpd_key_local[%d]=%d key down\n", j, j, tpd_dts_data.tpd_key_local[j]);
				ilitek_touch_down(0, x, y, 10, 128, 1);
			}
#else
			x = touch_key_point_maping_array[j].point_x;
			y = touch_key_point_maping_array[j].point_y;
			ilitek_touch_down(0, x, y, 10, 128, 1);
#endif
#endif
			ilitek_data->keyinfo[j].status = 1;
			ilitek_data->touch_key_hold_press = true;
			ilitek_data->is_touched = true;
			tp_log_debug("Key, Keydown ID=%d, X=%d, Y=%d, key_status=%d\n", ilitek_data->keyinfo[j].id, x, y, ilitek_data->keyinfo[j].status);
			break;
		}
	}
	return 0;
}

static int ilitek_check_key_release(int x, int y, int check_point)
{
	int j = 0;

	for (j = 0; j < ilitek_data->keycount; j++) {
		if (check_point) {
			if ((ilitek_data->keyinfo[j].status == 1) && (x < ilitek_data->keyinfo[j].x ||
								      x > ilitek_data->keyinfo[j].x + ilitek_data->key_xlen || y < ilitek_data->keyinfo[j].y ||
								      y > ilitek_data->keyinfo[j].y + ilitek_data->key_ylen)) {
#if ILITEK_PLAT != ILITEK_PLAT_MTK
				input_report_key(ilitek_data->input_dev, ilitek_data->keyinfo[j].id, 0);
#else
#ifndef MTK_UNDTS
				if (tpd_dts_data.use_tpd_button) {
					tp_log_debug("key index=%x, tpd_dts_data.tpd_key_local[%d]=%d key up\n", j, j, tpd_dts_data.tpd_key_local[j]);
					ilitek_touch_release(0);
				}
#else
				ilitek_touch_release(0);
#endif
#endif
				ilitek_data->keyinfo[j].status = 0;
				ilitek_data->touch_key_hold_press = false;
				tp_log_debug("Key, Keyout ID=%d, X=%d, Y=%d, key_status=%d\n",
					     ilitek_data->keyinfo[j].id, x, y, ilitek_data->keyinfo[j].status);
				break;
			}
		} else {
			if ((ilitek_data->keyinfo[j].status == 1)) {
#if ILITEK_PLAT != ILITEK_PLAT_MTK
				input_report_key(ilitek_data->input_dev, ilitek_data->keyinfo[j].id, 0);
#else
#ifndef MTK_UNDTS
				if (tpd_dts_data.use_tpd_button) {
					tp_log_debug("key index=%x, tpd_dts_data.tpd_key_local[%d]=%d key up\n", j, j, tpd_dts_data.tpd_key_local[j]);
					ilitek_touch_release(0);
				}
#else
				ilitek_touch_release(0);
#endif
#endif
				ilitek_data->keyinfo[j].status = 0;
				ilitek_data->touch_key_hold_press = false;
				tp_log_debug("Key, Keyout ID=%d, X=%d, Y=%d, key_status=%d\n",
					     ilitek_data->keyinfo[j].id, x, y, ilitek_data->keyinfo[j].status);
				break;
			}
		}
	}
	return 0;
}

#ifdef ILITEK_GESTURE
#if ILITEK_GESTURE == ILITEK_DOUBLE_CLICK_WAKEUP
#if ILITEK_GET_TIME_FUNC == ILITEK_GET_TIME_FUNC_WITH_TIME
static struct timeval start_event_time;
#else
unsigned long start_event_time_jiffies;
#endif
int event_spacing;
static uint8_t finger_state;	//0,1,2,3,4
static int start_x;
static int start_y;
static int current_x;
static int current_y;

static int ilitek_get_time_diff(void)
{
	int diff_milliseconds = 0;
#if ILITEK_GET_TIME_FUNC == ILITEK_GET_TIME_FUNC_WITH_TIME
	struct timeval time_now;

	do_gettimeofday(&time_now);
	diff_milliseconds += (time_now.tv_sec - start_event_time.tv_sec) * 1000;

	if (time_now.tv_usec < start_event_time.tv_usec) {
		diff_milliseconds -= 1000;
		diff_milliseconds += (1000 * 1000 + time_now.tv_usec - start_event_time.tv_usec) / 1000;
	} else
		diff_milliseconds += (time_now.tv_usec - start_event_time.tv_usec) / 1000;

	if (diff_milliseconds < (-10000))
		diff_milliseconds = 10000;
	tp_log_info("time_now.tv_sec = %d start_event_time.tv_sec = %d time_now.tv_usec = %d start_event_time.tv_usec = %d diff_milliseconds = %d\n",
		    (int)time_now.tv_sec, (int)start_event_time.tv_sec, (int)time_now.tv_usec, (int)start_event_time.tv_usec, diff_milliseconds);
#else
	diff_milliseconds = jiffies_to_msecs(jiffies) - jiffies_to_msecs(start_event_time_jiffies);
	tp_log_info("jiffies_to_msecs(jiffies) = %u jiffies_to_msecs(start_event_time_jiffies) = %u diff_milliseconds = %d\n", jiffies_to_msecs(jiffies),
		    jiffies_to_msecs(start_event_time_jiffies), diff_milliseconds);
#endif
	return diff_milliseconds;
}

static int ilitek_double_click_touch(int x, int y, char finger_state, int finger_id)
{
	tp_log_info("start finger_state = %d\n", finger_state);
	if (finger_id > 0) {
		finger_state = 0;
		goto out;
	}
	if (finger_state == 0 || finger_state == 5) {

		finger_state = 1;
		start_x = x;
		start_y = y;
		current_x = 0;
		current_y = 0;
		event_spacing = 0;
#if ILITEK_GET_TIME_FUNC == ILITEK_GET_TIME_FUNC_WITH_TIME
		do_gettimeofday(&start_event_time);
#else
		start_event_time_jiffies = jiffies;
#endif
	} else if (finger_state == 1) {
		event_spacing = ilitek_get_time_diff();
		if (event_spacing > DOUBLE_CLICK_ONE_CLICK_USED_TIME)
			finger_state = 4;
	} else if (finger_state == 2) {
		finger_state = 3;
		current_x = x;
		current_y = y;
		event_spacing = ilitek_get_time_diff();
		if (event_spacing > (DOUBLE_CLICK_ONE_CLICK_USED_TIME + DOUBLE_CLICK_NO_TOUCH_TIME))
			finger_state = 0;
	} else if (finger_state == 3) {
		current_x = x;
		current_y = y;
		event_spacing = ilitek_get_time_diff();
		if (event_spacing > DOUBLE_CLICK_TOTAL_USED_TIME) {
			start_x = current_x;
			start_y = current_y;
			finger_state = 4;
		}
	}
out:
	tp_log_info("finger_state = %d event_spacing = %d\n", finger_state, event_spacing);
	return finger_state;
}

static int ilitek_double_click_release(char finger_state)
{
	tp_log_info("start finger_state = %d\n", finger_state);
	if (finger_state == 1) {
		finger_state = 2;
		event_spacing = ilitek_get_time_diff();
		if (event_spacing > DOUBLE_CLICK_ONE_CLICK_USED_TIME)
			finger_state = 0;
	}
	if (finger_state == 3) {
		event_spacing = ilitek_get_time_diff();
		if ((event_spacing < DOUBLE_CLICK_TOTAL_USED_TIME && event_spacing > 50) && (ABSSUB(current_x, start_x) < DOUBLE_CLICK_DISTANCE)
		    && ((ABSSUB(current_y, start_y) < DOUBLE_CLICK_DISTANCE))) {
			finger_state = 5;
			goto out;
		} else
			finger_state = 0;
	} else if (finger_state == 4)
		finger_state = 0;
out:
	tp_log_info("finger_state = %d event_spacing = %d\n", finger_state, event_spacing);
	return finger_state;
}

#endif
#endif
int ilitek_read_data_and_report_3XX(void)
{
	int ret = 0;
	int packet = 0;
	int report_max_point = 6;
	int release_point = 0;
	int tp_status = 0;
	int i = 0;
	int x = 0;
	int y = 0;
	struct input_dev *input = ilitek_data->input_dev;
	uint8_t buf[64] = { 0 };
#ifndef ILITEK_CHECK_FUNCMODE
	buf[0] = ILITEK_TP_CMD_GET_TOUCH_INFORMATION;
	ret = ilitek_i2c_write_and_read(buf, 1, 0, buf, 31);
	if (ret < 0) {
		tp_log_err("get touch information err\n");
		if (ilitek_data->is_touched) {
			ilitek_touch_release_all_point();
			ilitek_check_key_release(x, y, 0);
		}
		return ret;
	}
#else
	uint8_t mode_status = 0;

	buf[0] = ILITEK_TP_CMD_GET_TOUCH_INFORMATION;
	ret = ilitek_i2c_write_and_read(buf, 1, 0, buf, 32);
	if (ret < 0) {
		tp_log_err("get touch information err\n");
		if (ilitek_data->is_touched) {
			ilitek_touch_release_all_point();
			ilitek_check_key_release(x, y, 0);
		}
		return ret;
	}
	mode_status = buf[31];
	tp_log_debug("mode_status = 0x%X\n", mode_status);
	if (mode_status & 0x80)
		tp_log_debug("Palm reject mode enable\n");
	if (mode_status & 0x40)
		tp_log_debug("Thumb mdoe enable\n");
	if (mode_status & 0x04)
		tp_log_debug("Water mode enable\n");
	if (mode_status & 0x02)
		tp_log_debug("Mist mode enable\n");
	if (mode_status & 0x01)
		tp_log_debug("Normal mode\n");
#endif
	packet = buf[0];
	if (packet == 2) {
		ret = ilitek_i2c_read(buf + 31, 20);
		if (ret < 0) {
			tp_log_err("get touch information packet 2 err\n");
			if (ilitek_data->is_touched) {
				ilitek_touch_release_all_point();
				ilitek_check_key_release(x, y, 0);
			}
			return ret;
		}
		report_max_point = 10;
	}
#ifdef ILITEK_TUNING_MESSAGE
	if (ilitek_debug_flag)
		ilitek_udp_reply(ilitek_pid, ilitek_seq, buf, sizeof(buf));
#endif
#ifdef ILITEK_TUNING_NODE
	if (ilitek_data->debug_node_open) {
		if (buf[0] == 0xDB) {

			mutex_lock(&ilitek_data->ilitek_debug_mutex);
			memcpy(ilitek_data->debug_buf[ilitek_data->debug_data_frame], buf, 64);
			ilitek_data->debug_data_frame++;
			if (ilitek_data->debug_data_frame > 1)
				tp_log_info("ilitek_data->debug_data_frame = %d\n", ilitek_data->debug_data_frame);
			if (ilitek_data->debug_data_frame > 1023) {
				tp_log_err("ilitek_data->debug_data_frame = %d > 1024\n", ilitek_data->debug_data_frame);
				ilitek_data->debug_data_frame = 1023;
			}
			//ilitek_data->debug_buf[ilitek_data->debug_buf[1]] = '\0';
			mutex_unlock(&ilitek_data->ilitek_debug_mutex);
			wake_up(&(ilitek_data->inq));
		}
	}
#endif
	if (buf[1] == 0x5F || buf[0] == 0xDB) {
		tp_log_debug("debug message return\n");
		return 0;
	}
	for (i = 0; i < report_max_point; i++) {
		tp_status = buf[i * 5 + 1] >> 7;
		tp_log_debug("ilitek tp_status = %d buf[i*5+1] = 0x%X\n", tp_status, buf[i * 5 + 1]);
		if (tp_status) {
			ilitek_data->touch_flag[i] = 1;
			x = ((buf[i * 5 + 1] & 0x3F) << 8) + buf[i * 5 + 2];
			y = (buf[i * 5 + 3] << 8) + buf[i * 5 + 4];
			tp_log_debug("ilitek x = %d y = %d\n", x, y);
			if (ilitek_data->system_suspend) {
				tp_log_info("system is suspend not report point\n");
#ifdef ILITEK_GESTURE
#if ILITEK_GESTURE == ILITEK_DOUBLE_CLICK_WAKEUP
				finger_state = ilitek_double_click_touch(x, y, finger_state, i);
#endif
#endif
			} else {
				if (!(ilitek_data->is_touched))
					ilitek_check_key_down(x, y);
				if (!(ilitek_data->touch_key_hold_press)) {
					if (x > ilitek_data->screen_max_x || y > ilitek_data->screen_max_y ||
					    x < ilitek_data->screen_min_x || y < ilitek_data->screen_min_y) {
						tp_log_info("Point (x > screen_max_x || y > screen_max_y) , ID=%02X, X=%d, Y=%d\n", i, x, y);
						tp_log_info("Packet is 0x%X. This ID read buf: 0x%X,0x%X,0x%X,0x%X,0x%X\n",
							    buf[0], buf[i * 5 + 1], buf[i * 5 + 2], buf[i * 5 + 3], buf[i * 5 + 4], buf[i * 5 + 5]);
					} else {
						ilitek_data->is_touched = true;
						if (ILITEK_REVERT_X)
							x = ilitek_data->screen_max_x - x + ilitek_data->screen_min_x;
						if (ILITEK_REVERT_Y)
							y = ilitek_data->screen_max_y - y + ilitek_data->screen_min_y;
						tp_log_debug("Point, ID=%02X, X=%04d, Y=%04d\n", i, x, y);
						ilitek_touch_down(i, x, y, 10, 128, 1);
					}
				}
				//if ((ilitek_data->touch_key_hold_press)){
				//      ilitek_check_key_release(x, y, 1);
				//}
			}
		} else {
			release_point++;
#ifdef ILITEK_TOUCH_PROTOCOL_B
			ilitek_touch_release(i);
#endif
		}
	}
	tp_log_debug("release point counter =  %d packet = %d\n", release_point, packet);
	if (packet == 0 || release_point == report_max_point) {
		if (ilitek_data->is_touched)
			ilitek_touch_release_all_point();
		ilitek_check_key_release(x, y, 0);
		ilitek_data->is_touched = false;
		if (ilitek_data->system_suspend) {
#ifdef ILITEK_GESTURE
#if ILITEK_GESTURE == ILITEK_CLICK_WAKEUP
			wake_lock_timeout(&ilitek_wake_lock, 5 * HZ);
			input_report_key(input, KEY_POWER, 1);
			input_sync(input);
			input_report_key(input, KEY_POWER, 0);
			input_sync(input);
			//ilitek_data->system_suspend = false;
#elif ILITEK_GESTURE == ILITEK_DOUBLE_CLICK_WAKEUP
			finger_state = ilitek_double_click_release(finger_state);
			if (finger_state == 5) {
				tp_log_info("double click wakeup\n");
				wake_lock_timeout(&ilitek_wake_lock, 5 * HZ);
				input_report_key(input, KEY_POWER, 1);
				input_sync(input);
				input_report_key(input, KEY_POWER, 0);
				input_sync(input);
				//ilitek_data->system_suspend = false;
			}
#endif
#endif
		}
	}
	input_sync(input);
	return 0;
}
int ilitek_read_data_and_report_6XX(void) {
	int ret = 0;
	int count = 0;
	int report_max_point = 6;
	int release_point = 0;
	int i = 0;
	struct input_dev *input = ilitek_data->input_dev;
	unsigned char buf[512]={0};
	int packet_len = 0;
	int packet_max_point = 0;

// 	if (ilitek_data->system_suspend) {
// 		tp_log_info("system is suspend not report point\n");
// #ifdef ILITEK_GESTURE
// #if ILITEK_GESTURE == ILITEK_DOUBLE_CLICK_WAKEUP
// 		finger_state = ilitek_double_click_touch(x, y, finger_state, i);
// #endif
// #endif
// 	}
	tp_log_debug("ilitek_read_data_and_report_6XX: format:%d\n", ilitek_data->format);
	//initial touch format
	switch (ilitek_data->format)
	{
	case 0:
		packet_len = REPORT_FORMAT0_PACKET_LENGTH;
		packet_max_point = REPORT_FORMAT0_PACKET_MAX_POINT;
		break;
	case 1:
		packet_len = REPORT_FORMAT1_PACKET_LENGTH;
		packet_max_point = REPORT_FORMAT1_PACKET_MAX_POINT;
		break;
	case 2:
		packet_len = REPORT_FORMAT2_PACKET_LENGTH;
		packet_max_point = REPORT_FORMAT2_PACKET_MAX_POINT;
		break;
	case 3:
		packet_len = REPORT_FORMAT3_PACKET_LENGTH;
		packet_max_point = REPORT_FORMAT3_PACKET_MAX_POINT;
		break;
	default:
		packet_len = REPORT_FORMAT0_PACKET_LENGTH;
		packet_max_point = REPORT_FORMAT0_PACKET_MAX_POINT;
		break;
	}
	for(i = 0; i < ilitek_data->max_tp; i++)
		ilitek_data->tp[i].status = false;

	ret = ilitek_i2c_read(buf, 64);
	if (ret < 0) {
		tp_log_err("get touch information err\n");
		if (ilitek_data->is_touched) {
			ilitek_touch_release_all_point();
			ilitek_check_key_release(0, 0, 0);
		}
		return ret;
	}

	report_max_point = buf[REPORT_ADDRESS_COUNT];
	if(report_max_point > ilitek_data->max_tp) {
		tp_log_err("FW report max point:%d > panel information max point:%d\n", report_max_point, ilitek_data->max_tp);
		return ILITEK_FAIL;
	}
	count = CEIL(report_max_point, packet_max_point); //(report_max_point / 10) + 1;
	for(i = 1; i < count; i++) {
		ret = ilitek_i2c_read(buf+i*64, 64);
		if (ret < 0) {
			tp_log_err("get touch information err, count=%d\n", count);
			if (ilitek_data->is_touched) {
				ilitek_touch_release_all_point();
				ilitek_check_key_release(0, 0, 0);
			}
			return ret;
		}
	}
	for (i = 0; i < report_max_point; i++) {
		ilitek_data->tp[i].status = buf[i*packet_len+1] >> 6;
		ilitek_data->tp[i].id = buf[i*packet_len+1] & 0x3F;
		tp_log_debug("ilitek tp_status = %d buf[i*5+1] = 0x%X\n", ilitek_data->tp[i].status, buf[i*packet_len+1]);
		if (ilitek_data->tp[i].status) {
			ilitek_data->touch_flag[ilitek_data->tp[i].id] = 1;
			ilitek_data->tp[i].x = ((buf[i*packet_len+3]) << 8) + buf[i*packet_len+2];
			ilitek_data->tp[i].y = (buf[i*packet_len+5] << 8) + buf[i*packet_len+4];
			if(ilitek_data->format == 0) {
				ilitek_data->tp[i].p = 10;
				ilitek_data->tp[i].w = 10;
				ilitek_data->tp[i].h = 10;
			}
			if(ilitek_data->format == 1 || ilitek_data->format == 3) {
				ilitek_data->tp[i].p = buf[i*packet_len+6];
				ilitek_data->tp[i].w = 10;
				ilitek_data->tp[i].h = 10;
			}
			if(ilitek_data->format == 2 || ilitek_data->format == 3) {
				ilitek_data->tp[i].p = 10;
				ilitek_data->tp[i].w = buf[i*packet_len+7];
				ilitek_data->tp[i].h = buf[i*packet_len+8];
			}

			tp_log_debug("ilitek id = %d, x = %d y = %d\n", ilitek_data->tp[i].id, ilitek_data->tp[i].x, ilitek_data->tp[i].y);
			if (ilitek_data->system_suspend) {
				tp_log_info("system is suspend not report point\n");
		#ifdef ILITEK_GESTURE
		#if ILITEK_GESTURE == ILITEK_DOUBLE_CLICK_WAKEUP
				finger_state = ilitek_double_click_touch(ilitek_data->tp[i].x, ilitek_data->tp[i].y, finger_state, i);
		#endif
		#endif
			}
			else {
				if(!(ilitek_data->is_touched)) {
					ilitek_check_key_down(ilitek_data->tp[i].x, ilitek_data->tp[i].y);
				}
				if (!(ilitek_data->touch_key_hold_press)) {
					if (ilitek_data->tp[i].x > ilitek_data->screen_max_x || ilitek_data->tp[i].y > ilitek_data->screen_max_y ||
						ilitek_data->tp[i].x < ilitek_data->screen_min_x || ilitek_data->tp[i].y < ilitek_data->screen_min_y) {
						tp_log_info("Point (x > screen_max_x || y > screen_max_y) , ID=%02X, X=%d, Y=%d\n",
						ilitek_data->tp[i].id, ilitek_data->tp[i].x, ilitek_data->tp[i].y);
					}
					else {
						ilitek_data->is_touched = true;
						if (ILITEK_REVERT_X) {
							ilitek_data->tp[i].x = ilitek_data->screen_max_x - ilitek_data->tp[i].x + ilitek_data->screen_min_x;
						}
						if (ILITEK_REVERT_Y) {
							ilitek_data->tp[i].y = ilitek_data->screen_max_y - ilitek_data->tp[i].y + ilitek_data->screen_min_y;
						}
						tp_log_debug("Point, ID=%02X, X=%04d, Y=%04d, P=%d, H=%d, W=%d\n",
						ilitek_data->tp[i].id, ilitek_data->tp[i].x,ilitek_data->tp[i].y,
						ilitek_data->tp[i].p, ilitek_data->tp[i].h, ilitek_data->tp[i].w);
						ilitek_touch_down(ilitek_data->tp[i].id, ilitek_data->tp[i].x, ilitek_data->tp[i].y,
						ilitek_data->tp[i].p, ilitek_data->tp[i].h, ilitek_data->tp[i].w);
					}
				}
				if ((ilitek_data->touch_key_hold_press)){
					ilitek_check_key_release(ilitek_data->tp[i].x, ilitek_data->tp[i].y, 1);
				}
			}
		}
		else {
			release_point++;
			#ifdef ILITEK_TOUCH_PROTOCOL_B
			ilitek_touch_release(ilitek_data->tp[i].id);
			#endif
		}
	}
	tp_log_debug("release point counter =  %d , report max point = %d\n", release_point, report_max_point);
	if (release_point == report_max_point) {
		if (ilitek_data->is_touched) {
			ilitek_touch_release_all_point();
		}
		ilitek_check_key_release(0, 0, 0);
		ilitek_data->is_touched = false;
		if (ilitek_data->system_suspend) {
		#ifdef ILITEK_GESTURE
			#if ILITEK_GESTURE == ILITEK_CLICK_WAKEUP
			wake_lock_timeout(&ilitek_wake_lock, 5 * HZ);
			input_report_key(input, KEY_POWER, 1);
			input_sync(input);
			input_report_key(input, KEY_POWER, 0);
			input_sync(input);
			//ilitek_data->system_suspend = false;
			#elif ILITEK_GESTURE == ILITEK_DOUBLE_CLICK_WAKEUP
			finger_state = ilitek_double_click_release(finger_state);
			if (finger_state == 5) {

				tp_log_info("double click wakeup\n");
				wake_lock_timeout(&ilitek_wake_lock, 5 * HZ);
				input_report_key(input, KEY_POWER, 1);
				input_sync(input);
				input_report_key(input, KEY_POWER, 0);
				input_sync(input);
				//ilitek_data->system_suspend = false;
			}
			#endif
		#endif
		}
	}
	input_sync(input);
	return 0;
}

static int ilitek_i2c_process_and_report(void)
{
	int ret = 0;
	mutex_lock(&ilitek_data->ilitek_mutex);
	if (!ilitek_data->unhandle_irq)
		ret = ilitek_data->process_and_report();
	mutex_unlock(&ilitek_data->ilitek_mutex);
	return ret;
}

#ifdef MTK_UNDTS
static void ilitek_i2c_isr(void)
#else
static irqreturn_t ilitek_i2c_isr(int irq, void *dev_id)
#endif
{
	unsigned long irqflag = 0;

	tp_log_debug("\n");
#ifdef ILITEK_ESD_PROTECTION
	ilitek_data->esd_check = false;
#endif
	if (ilitek_data->firmware_updating) {
		tp_log_debug("firmware_updating return\n");
#ifdef MTK_UNDTS
		return;
#else
		return IRQ_HANDLED;
#endif
	}
	spin_lock_irqsave(&ilitek_data->irq_lock, irqflag);
	if (ilitek_data->irq_status) {
#ifdef MTK_UNDTS
		mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#else
		disable_irq_nosync(ilitek_data->client->irq);
#endif
		ilitek_data->irq_status = false;
		ilitek_data->irq_trigger_count++;
	}
	spin_unlock_irqrestore(&ilitek_data->irq_lock, irqflag);
	//ilitek_data->irq_trigger = true;
	//wake_up_interruptible(&waiter);
	if (ilitek_i2c_process_and_report() < 0)
		tp_log_err("process error\n");
	ilitek_irq_enable();
#ifndef MTK_UNDTS
	return IRQ_HANDLED;
#endif
}

static int ilitek_request_irq(void)
{
	int ret = 0;
#if ILITEK_PLAT == ILITEK_PLAT_MTK
#ifndef MTK_UNDTS
	struct device_node *node;
#endif
#endif
	spin_lock_init(&ilitek_data->irq_lock);
	ilitek_data->irq_status = true;
#if ILITEK_PLAT != ILITEK_PLAT_MTK
	ilitek_data->client->irq = gpio_to_irq(ilitek_data->irq_gpio);
#else
#ifndef MTK_UNDTS
	node = of_find_matching_node(NULL, touch_of_match);
	if (node)
		ilitek_data->client->irq = irq_of_parse_and_map(node, 0);
#endif
#endif

#ifdef MTK_UNDTS
	mt_set_gpio_mode(ILITEK_IRQ_GPIO, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(ILITEK_IRQ_GPIO, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(ILITEK_IRQ_GPIO, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(ILITEK_IRQ_GPIO, GPIO_PULL_UP);

	mt_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
	mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_TYPE, ilitek_i2c_isr, 1);
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#else
	tp_log_info("ilitek_data->client->irq = %d\n", ilitek_data->client->irq);
	if (ilitek_data->client->irq > 0) {
		//ret = request_irq(ilitek_data->client->irq, ilitek_i2c_isr, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "ilitek_i2c_irq", ilitek_data);
		ret = request_threaded_irq(ilitek_data->client->irq, NULL,ilitek_i2c_isr, IRQF_TRIGGER_FALLING/* IRQF_TRIGGER_LOW*/ | IRQF_ONESHOT,
									"ilitek_touch_irq", ilitek_data);
		// ret = request_threaded_irq(ilitek_data->client->irq, NULL, ilitek_i2c_isr, /*IRQF_TRIGGER_FALLING */ IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
		// 							"ilitek_touch_irq", ilitek_data);
		if (ret)
			tp_log_err("ilitek_request_irq, error\n");
	} else
		ret = -EINVAL;
#endif
	return ret;
}

#ifdef ILITEK_UPDATE_FW
static int ilitek_update_thread(void *arg)
{

	int ret = 0;

	tp_log_info("\n");
	if (kthread_should_stop()) {
		tp_log_info("ilitek_update_thread, stop\n");
		return -1;
	}

	mdelay(100);
	ilitek_data->firmware_updating = true;
	ilitek_data->operation_protection = true;
	if(ilitek_data->ptl.ver_major == 0x6 || ilitek_data->ptl.ver == BL_V1_8)
		ret = ilitek_boot_upgrade_6XX();
	else
		ret = ilitek_boot_upgrade_3XX();
	ret = ilitek_read_tp_info(true);
	ilitek_data->operation_protection = false;
	ilitek_data->firmware_updating = false;
	ret = ilitek_set_input_param();
	if (ret)
		tp_log_err("register input device, error\n");
	ret = ilitek_request_irq();
	if (ret)
		tp_log_err("ilitek_request_irq, error\n");
	return ret;
}
#endif

#if 0
static int ilitek_irq_handle_thread(void *arg)
{
	int ret = 0;
	struct sched_param param = {.sched_priority = 4 };

	sched_setscheduler(current, SCHED_RR, &param);
	tp_log_info("%s, enter\n", __func__);

	// mainloop
	while (!kthread_should_stop() && !ilitek_data->ilitek_exit_report) {
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(waiter, ilitek_data->irq_trigger);
		ilitek_data->irq_trigger = false;
		ilitek_data->irq_handle_count++;
		set_current_state(TASK_RUNNING);
		if (ilitek_i2c_process_and_report() < 0)
			tp_log_err("process error\n");
		ilitek_irq_enable();
	}
	tp_log_err("%s, exit\n", __func__);
	tp_log_err("%s, exit\n", __func__);
	tp_log_err("%s, exit\n", __func__);
	return ret;
}
#endif

void ilitek_suspend(void)
{

	tp_log_info("\n");
#ifdef ILITEK_ESD_PROTECTION
	ilitek_data->esd_check = false;
	cancel_delayed_work_sync(&ilitek_data->esd_work);
#endif

	if (ilitek_data->operation_protection || ilitek_data->firmware_updating) {
		tp_log_info("operation_protection or firmware_updating return\n");
		return;
	}
	mutex_lock(&ilitek_data->ilitek_mutex);
#ifndef ILITEK_GESTURE
	#if ILITEK_LOW_POWER == ILITEK_SLEEP
		if(api_protocol_set_testmode(true) < ILITEK_SUCCESS)
			tp_log_err("Set suspend mode error\n");
		if (api_protocol_set_cmd(ILITEK_TP_CMD_SET_SLEEP, NULL, NULL) < ILITEK_SUCCESS)
			tp_log_err("0x30 set tp sleep err\n");
	#endif
#endif
#ifndef ILITEK_GESTURE
	ilitek_irq_disable();
#else
	enable_irq_wake(ilitek_data->client->irq);
#endif
	mutex_unlock(&ilitek_data->ilitek_mutex);
	ilitek_data->system_suspend = true;
}

void ilitek_resume(void)
{
	tp_log_info("\n");
	if (ilitek_data->operation_protection || ilitek_data->firmware_updating) {
		tp_log_info("operation_protection or firmware_updating return\n");
		return;
	}
	mutex_lock(&ilitek_data->ilitek_mutex);
#ifdef ILITEK_GESTURE
	ilitek_irq_disable();
#endif
#if ILITEK_LOW_POWER == ILITEK_SLEEP
	if (api_protocol_set_cmd(ILITEK_TP_CMD_SET_WAKEUP, NULL, NULL) < ILITEK_SUCCESS)
		tp_log_err("0x31 set wake up err\n");
	if(api_protocol_set_testmode(false) < ILITEK_SUCCESS)
		tp_log_err("Set Normal mode error\n");
#elif ILITEK_LOW_POWER == ILITEK_POWEROFF
	ilitek_reset(ilitek_data->reset_time, false);
#endif
	mutex_unlock(&ilitek_data->ilitek_mutex);
	ilitek_irq_enable();
	ilitek_data->system_suspend = false;
#ifdef ILITEK_GESTURE
#if ILITEK_GESTURE == ILITEK_DOUBLE_CLICK_WAKEUP
	finger_state = 0;
#endif
	disable_irq_wake(ilitek_data->client->irq);
#endif
#ifdef ILITEK_ESD_PROTECTION
	ilitek_data->esd_check = true;
	if (ilitek_data->esd_wq)
		queue_delayed_work(ilitek_data->esd_wq, &ilitek_data->esd_work, ilitek_data->esd_delay);
#endif
	ilitek_touch_release_all_point();
	ilitek_check_key_release(0, 0, 0);
}

#if ILITEK_PLAT == ILITEK_PLAT_ALLWIN
int ilitek_suspend_allwin(struct i2c_client *client, pm_message_t mesg)
{
	ilitek_suspend();
	return 0;
}

int ilitek_resume_allwin(struct i2c_client *client)
{
	ilitek_resume();
	return 0;
}
#endif

#if ILITEK_PLAT != ILITEK_PLAT_MTK
#if defined(CONFIG_FB) || defined(CONFIG_QCOM_DRM)
static int ilitek_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data) {
#ifdef CONFIG_QCOM_DRM
	struct msm_drm_notifier *ev_data = data;
#else
	struct fb_event *ev_data = data;
#endif
	int *blank;
	tp_log_info("FB EVENT event = %ld\n", event);
	/*if (ev_data && ev_data->data && event == FB_EARLY_EVENT_BLANK) {
		blank = ev_data->data;
		if (*blank == FB_BLANK_UNBLANK || *blank == FB_BLANK_NORMAL) {
			ilitek_resume();
		}
	}*/
#ifdef CONFIG_QCOM_DRM
	if (!ev_data || (ev_data->id != 0))
		return 0;
#endif
	if (ev_data && ev_data->data && event == ILITEK_EVENT_BLANK) {
		blank = ev_data->data;
		if (*blank == ILITEK_BLANK_POWERDOWN) {
			ilitek_suspend();
		}
		else if (*blank == ILITEK_BLANK_UNBLANK || *blank == ILITEK_BLANK_NORMAL) {
			ilitek_resume();
		}
	}

	return 0;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void ilitek_early_suspend(struct early_suspend *h)
{
	ilitek_suspend();
}

static void ilitek_late_resume(struct early_suspend *h)
{
	ilitek_resume();
}
#endif
#endif

static int ilitek_get_gpio_num(void)
{
	int ret = 0;
#ifdef ILITEK_GET_GPIO_NUM
#if ILITEK_PLAT == ILITEK_PLAT_ALLWIN
	tp_log_info("(config_info.wakeup_gpio.gpio) = %d (config_info.int_number) = %d\n", (config_info.wakeup_gpio.gpio), (config_info.int_number));
	ilitek_data->reset_gpio = (config_info.wakeup_gpio.gpio);
	ilitek_data->irq_gpio = (config_info.int_number);
#else
#ifdef CONFIG_OF
	struct device *dev = &(ilitek_data->client->dev);
	struct device_node *np = dev->of_node;

	ilitek_data->reset_gpio = of_get_named_gpio(np, "ilitek,reset-gpio", 0);
	if (ilitek_data->reset_gpio < 0)
		tp_log_err("reset_gpio = %d\n", ilitek_data->reset_gpio);
	ilitek_data->irq_gpio = of_get_named_gpio(np, "ilitek,irq-gpio", 0);
	if (ilitek_data->irq_gpio < 0)
		tp_log_err("irq_gpio = %d\n", ilitek_data->irq_gpio);
#endif
#endif
#else
	ilitek_data->reset_gpio = ILITEK_RESET_GPIO;
	ilitek_data->irq_gpio = ILITEK_IRQ_GPIO;
#endif
	tp_log_info("reset_gpio = %d irq_gpio = %d\n", ilitek_data->reset_gpio, ilitek_data->irq_gpio);
	return ret;
}

static int ilitek_request_gpio(void)
{
	int ret = 0;

	ilitek_get_gpio_num();
#if ILITEK_PLAT != ILITEK_PLAT_MTK
	if (ilitek_data->reset_gpio > 0) {
		ret = gpio_request(ilitek_data->reset_gpio, "ilitek-reset-gpio");
		if (ret) {
			tp_log_err("Failed to request reset_gpio so free retry\n");
			gpio_free(ilitek_data->reset_gpio);
			ret = gpio_request(ilitek_data->reset_gpio, "ilitek-reset-gpio");
			if (ret)
				tp_log_err("Failed to request reset_gpio\n");
		}
		if (ret) {
			tp_log_err("Failed to request reset_gpio\n");
		} else {
			ret = gpio_direction_output(ilitek_data->reset_gpio, 1);
			if (ret)
				tp_log_err("Failed to direction output rest gpio err\n");
		}
	}
	if (ilitek_data->irq_gpio > 0) {
		ret = gpio_request(ilitek_data->irq_gpio, "ilitek-irq-gpio");
		if (ret) {
			tp_log_err("Failed to request irq_gpio so free retry\n");
			gpio_free(ilitek_data->irq_gpio);
			ret = gpio_request(ilitek_data->irq_gpio, "ilitek-irq-gpio");
			if (ret)
				tp_log_err("Failed to request irq_gpio\n");
		}
		if (ret) {
			tp_log_err("Failed to request irq_gpio\n");
		} else {
			ret = gpio_direction_input(ilitek_data->irq_gpio);
			if (ret)
				tp_log_err("Failed to direction input irq gpio err\n");
		}
	}
#endif
	return ret;
}

static int ilitek_power_on(bool status)
{
	int ret = 0;

	tp_log_info("%s\n", status ? "POWER ON" : "POWER OFF");
#if ILITEK_PLAT != ILITEK_PLAT_ALLWIN
#ifdef ILITEK_ENABLE_REGULATOR_POWER_ON
	if (status) {
		if (ilitek_data->vdd) {
			ret = regulator_enable(ilitek_data->vdd);
			if (ret < 0) {
				tp_log_err("regulator_enable vdd fail\n");
				return -EINVAL;
			}
		}
		if (ilitek_data->vdd_i2c) {
			ret = regulator_enable(ilitek_data->vdd_i2c);
			if (ret < 0) {
				tp_log_err("regulator_enable vdd_i2c fail\n");
				return -EINVAL;
			}
		}
	} else {
		if (ilitek_data->vdd) {
			ret = regulator_disable(ilitek_data->vdd);
			if (ret < 0)
				tp_log_err("regulator_enable vdd fail\n");
				//return -EINVAL;
		}
		if (ilitek_data->vdd_i2c) {
			ret = regulator_disable(ilitek_data->vdd_i2c);
			if (ret < 0)
				tp_log_err("regulator_enable vdd_i2c fail\n");
				//return -EINVAL;
		}
	}
#endif

#ifdef MTK_UNDTS
//please change for your own platform
	if (status)
		hwPowerOn(PMIC_APP_CAP_TOUCH_VDD, VOL_2800, "TP");
#endif

#else
	input_set_power_enable(&(config_info.input_type), status);
#endif
	return ret;
}

static int ilitek_create_esdandcharge_workqueue(void)
{
#ifdef ILITEK_ESD_PROTECTION
	INIT_DELAYED_WORK(&ilitek_data->esd_work, ilitek_esd_check);
	ilitek_data->esd_wq = create_singlethread_workqueue("ilitek_esd_wq");
	if (!ilitek_data->esd_wq) {
		tp_log_err("create workqueue esd work err\n");
	} else {
		ilitek_data->esd_check = true;
		ilitek_data->esd_delay = 2 * HZ;
		queue_delayed_work(ilitek_data->esd_wq, &ilitek_data->esd_work, ilitek_data->esd_delay);
	}
#endif
	return 0;
}

static int ilitek_register_resume_suspend(void)
{
	int ret = 0;
#if ILITEK_PLAT != ILITEK_PLAT_MTK
#if defined(CONFIG_FB) || defined(CONFIG_QCOM_DRM)
	ilitek_data->fb_notif.notifier_call = ilitek_notifier_callback;
#ifdef CONFIG_QCOM_DRM
	ret = msm_drm_register_client(&ilitek_data->fb_notif);
#else
	ret = fb_register_client(&ilitek_data->fb_notif);
#endif
	if (ret)
		tp_log_err("Unable to register fb_notifier: %d\n", ret);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	ilitek_data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ilitek_data->early_suspend.suspend = ilitek_early_suspend;
	ilitek_data->early_suspend.resume = ilitek_late_resume;
	register_early_suspend(&ilitek_data->early_suspend);
#endif
#endif
#if ILITEK_PLAT == ILITEK_PLAT_ALLWIN
	device_enable_async_suspend(&ilitek_data->client->dev);
	pm_runtime_set_active(&ilitek_data->client->dev);
	pm_runtime_get(&ilitek_data->client->dev);
	pm_runtime_enable(&ilitek_data->client->dev);
#endif
	return ret;
}

static int ilitek_init_netlink(void)
{
#ifdef ILITEK_TUNING_MESSAGE
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
	struct netlink_kernel_cfg cfg = {
		.groups = 0,
		.input = udp_receive,
	};
#endif
#endif

#ifdef ILITEK_TUNING_MESSAGE
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
	ilitek_netlink_sock = netlink_kernel_create(&init_net, 21, &cfg);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3, 6, 0)
	ilitek_netlink_sock = netlink_kernel_create(&init_net, 21, 0, udp_receive, NULL, THIS_MODULE);
#else
	ilitek_netlink_sock = netlink_kernel_create(&init_net, 21, THIS_MODULE, &cfg);
#endif
#endif
	return 0;
}
// boot for initial
int ilitek_read_tp_info(bool boot)
{
	int ret = 0;
	uint8_t outbuf[64] = {0};
	tp_log_info("driver version %d.%d.%d.%d.%d.%d.%d\n", ilitek_driver_information[0], ilitek_driver_information[1],
		    ilitek_driver_information[2], ilitek_driver_information[3], ilitek_driver_information[4],
		    ilitek_driver_information[5], ilitek_driver_information[6]);
	if (api_protocol_set_cmd(ILITEK_TP_CMD_GET_PROTOCOL_VERSION, NULL, outbuf) < ILITEK_SUCCESS)
		goto transfer_err;
	if (api_protocol_set_cmd(ILITEK_TP_CMD_GET_KERNEL_VERSION, NULL, outbuf) < ILITEK_SUCCESS)
		goto transfer_err;
#ifdef ILITEK_GESTURE
		ilitek_data->enable_gesture = true;
#endif
	if (api_protocol_set_cmd(ILITEK_TP_CMD_GET_FIRMWARE_VERSION, NULL, outbuf) < ILITEK_SUCCESS)
		goto transfer_err;
	if(boot) {
		if (api_protocol_set_cmd(ILITEK_TP_CMD_GET_SCREEN_RESOLUTION ,NULL, outbuf) < ILITEK_SUCCESS)
			goto transfer_err;
	}
	if (api_protocol_set_cmd(ILITEK_TP_CMD_GET_TP_RESOLUTION, NULL, outbuf) < ILITEK_SUCCESS)
		goto transfer_err;
	if (api_protocol_set_cmd(ILITEK_TP_CMD_READ_MODE, NULL, outbuf) < ILITEK_SUCCESS)
		goto transfer_err;
	return ret;
transfer_err:
	return ILITEK_FAIL;
}

#ifdef ILITEK_ENABLE_DMA
static int ilitek_alloc_dma(void)
{
	tpd->dev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	I2CDMABuf_va = (u8 *) dma_alloc_coherent(&tpd->dev->dev, 4096, &I2CDMABuf_pa, GFP_KERNEL);
	if (!I2CDMABuf_va) {
		tp_log_err("ilitek [TPD] tpd->dev->dev dma_alloc_coherent error\n");
		I2CDMABuf_va = (u8 *) dma_alloc_coherent(NULL, 4096, &I2CDMABuf_pa, GFP_KERNEL);
		if (!I2CDMABuf_va) {
			tp_log_err("ilitek [TPD] NULL dma_alloc_coherent error\n");
			return -1;
		}
	}
	memset(I2CDMABuf_va, 0, 4096);

	//ilitek_data->client->ext_flag |= I2C_DMA_FLAG;

	return 0;
}
#endif
int ilitek_main_probe(struct ilitek_ts_data *ilitek_ts_data)
{
	int ret = 0;
//#if ILITEK_PLAT != ILITEK_PLAT_MTK
	tp_log_info("default client->addr = 0x%x client->irq = %d\n", ilitek_data->client->addr, ilitek_data->client->irq);
//#endif
	if (ilitek_data->client->addr != 0x41)
		ilitek_data->client->addr = 0x41;
	mutex_init(&ilitek_data->ilitek_mutex);
	ilitek_data->unhandle_irq = false;
#ifdef ILITEK_TUNING_NODE
	mutex_init(&ilitek_data->ilitek_debug_mutex);
#endif
#ifdef ILITEK_ENABLE_DMA
	ilitek_alloc_dma();
#endif
	ret = ilitek_power_on(true);
	ret = ilitek_request_gpio();
	ilitek_reset(1000 , true);
	ret = api_protocol_init_func();
	if (ret < 0) {
		tp_log_err("init read tp protocol error so exit\n");
		goto read_info_err;
	}
	ret = ilitek_read_tp_info(true);
#ifdef ILITEK_USE_MTK_INPUT_DEV
	ilitek_data->input_dev = tpd->dev;
#else
	ilitek_data->input_dev = input_allocate_device();
#endif
	if (ilitek_data->input_dev == NULL) {
		tp_log_err("allocate input device, error\n");
		goto read_info_err;
	}
#ifndef ILITEK_UPDATE_FW
	ret = ilitek_set_input_param();
	if (ret) {
		tp_log_err("register input device, error\n");
		goto input_register_err;
	}
	ret = ilitek_request_irq();
	if (ret) {
		tp_log_err("ilitek_request_irq, error\n");
		goto input_register_err;
	}
#endif
#if 0
	ilitek_data->irq_thread = kthread_run(ilitek_irq_handle_thread, NULL, "ilitek_irq_thread");
	if (ilitek_data->irq_thread == (struct task_struct *)ERR_PTR) {
		ilitek_data->irq_thread = NULL;
		tp_log_err("kthread create ilitek_irq_handle_thread, error\n");
		goto kthread_run_irq_thread_err;
	}
#endif
#ifdef ILITEK_UPDATE_FW
	ilitek_data->update_thread = kthread_run(ilitek_update_thread, NULL, "ilitek_update_thread");
	if (ilitek_data->update_thread == (struct task_struct *)ERR_PTR) {
		ilitek_data->update_thread = NULL;
		tp_log_err("kthread create ilitek_update_thread, error\n");
	}
#endif
	ilitek_register_resume_suspend();

	ilitek_create_sysfsnode();

#ifdef ILITEK_TOOL
	ilitek_create_tool_node();
#endif
	ilitek_init_netlink();

	ilitek_create_esdandcharge_workqueue();
#ifdef ILITEK_GESTURE
	device_init_wakeup(&ilitek_data->client->dev, 1);
	wake_lock_init(&ilitek_wake_lock, WAKE_LOCK_SUSPEND, "ilitek wakelock");
#endif
	return 0;
// kthread_run_irq_thread_err:
// #ifndef ILITEK_UPDATE_FW
// #ifndef MTK_UNDTS
//      free_irq(ilitek_data->client->irq, ilitek_data);
// #endif
// #endif
#ifndef ILITEK_UPDATE_FW
input_register_err:
	input_free_device(ilitek_data->input_dev);
#endif
read_info_err:
#if ILITEK_PLAT != ILITEK_PLAT_ALLWIN
#ifdef ILITEK_ENABLE_REGULATOR_POWER_ON
	ilitek_regulator_release();
#endif
#endif
	ilitek_free_gpio();
	kfree(ilitek_data);
	tp_log_err("return -ENODEV\n");
	return -ENODEV;
}

int ilitek_main_remove(struct ilitek_ts_data *ilitek_data)
{
	tp_log_info("\n");
	if (ilitek_data != NULL) {
#ifdef ILITEK_GESTURE
		device_init_wakeup(&ilitek_data->client->dev, 0);
		wake_lock_destroy(&ilitek_wake_lock);
#endif
#ifndef MTK_UNDTS
		free_irq(ilitek_data->client->irq, ilitek_data);
#endif
#ifdef ILITEK_TUNING_MESSAGE
		if (ilitek_netlink_sock != NULL) {
			netlink_kernel_release(ilitek_netlink_sock);
			ilitek_netlink_sock = NULL;
		}
#endif

#if defined(CONFIG_FB) || defined(CONFIG_QCOM_DRM)
#ifdef CONFIG_QCOM_DRM
		msm_drm_unregister_client(&ilitek_data->fb_notif);
#else
		fb_unregister_client(&ilitek_data->fb_notif);
#endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
		unregister_early_suspend(&ilitek_data->early_suspend);
#endif
		if (ilitek_data->irq_thread != NULL) {
			tp_log_info("irq_thread\n");
			ilitek_data->ilitek_exit_report = true;
			ilitek_data->irq_trigger = true;
			wake_up_interruptible(&waiter);
			kthread_stop(ilitek_data->irq_thread);
			ilitek_data->irq_thread = NULL;
		}
		if (ilitek_data->input_dev) {
			input_unregister_device(ilitek_data->input_dev);
			ilitek_data->input_dev = NULL;
		}
#ifdef ILITEK_TOOL
		ilitek_remove_tool_node();
#endif
		ilitek_remove_sys_node();
#ifdef ILITEK_ESD_PROTECTION
		if (ilitek_data->esd_wq) {
			destroy_workqueue(ilitek_data->esd_wq);
			ilitek_data->esd_wq = NULL;
		}
#endif

#if ILITEK_PLAT != ILITEK_PLAT_ALLWIN
#ifdef ILITEK_ENABLE_REGULATOR_POWER_ON
		ilitek_regulator_release();
#endif
#endif

		ilitek_free_gpio();
#ifdef ILITEK_ENABLE_DMA
		if (I2CDMABuf_va) {
			dma_free_coherent(&tpd->dev->dev, 250, I2CDMABuf_va, I2CDMABuf_pa);

			I2CDMABuf_va = NULL;
			I2CDMABuf_pa = 0;

		}
#endif
		kfree(ilitek_data);
		ilitek_data = NULL;
	}
	return 0;
}
