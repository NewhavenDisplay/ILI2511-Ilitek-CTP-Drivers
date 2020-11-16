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
#include "ilitek_common.h"
#include <linux/slab.h>
#include <linux/delay.h>
#include "ilitek_protocol.h"

PROTOCOL_MAP *ptl_cb_func;
//----------------------protocol function----------------------//
int api_protocol_control_funcmode(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
	inbuf[0] = ILITEK_TP_CMD_SET_FUNC_MODE;
	ret = ilitek_i2c_write_and_read(inbuf, 4, 10, outbuf, 0);
	return ret;
}
int api_protocol_system_busy(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
	inbuf[0] = ILITEK_TP_CMD_GET_SYSTEM_BUSY;
	ret = ilitek_i2c_write_and_read(inbuf, 1, 10, outbuf, 1);
	return ret;
}

int api_protocol_control_tsmode(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
	inbuf[0] = ILITEK_TP_CMD_CONTROL_TESTMODE;
	ret = ilitek_i2c_write_and_read(inbuf, 2, 0, outbuf, 0);
	return ret;
}

int api_protocol_write_enable(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
    uint8_t buf[64] = {0};
	buf[0] = ILITEK_TP_CMD_WRITE_ENABLE;
	buf[1] = 0x5A;
	buf[2] = 0xA5;
	ret = ilitek_i2c_write_and_read(buf, 3, 0, outbuf, 0);
	return ret;
}

int api_protocol_set_apmode(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
    uint8_t buf[64] = {0};
	buf[0] = ILITEK_TP_CMD_SET_APMODE;
	ret = ilitek_i2c_write_and_read(buf, 1, 0, outbuf, 0);
	return ret;
}

int api_protocol_set_blmode(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
    uint8_t buf[64] = {0};
	buf[0] = ILITEK_TP_CMD_SET_BLMODE;
	buf[1] = 0x5A;
	buf[2] = 0xA5;
	ret = ilitek_i2c_write_and_read(buf, 1, 0, outbuf, 0);
	return ret;
}

int api_protocol_get_mcuver(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS, i = 0;
    uint8_t buf[64] = {0};
	uint32_t ic = 0;

	buf[0] = ILITEK_TP_CMD_GET_KERNEL_VERSION;
	if(ilitek_i2c_write_and_read(buf, 1, 5, outbuf, 5) < 0){
		return ILITEK_FAIL;
	}
	for (i = 0; i < 5; i++)
		ilitek_data->mcu_ver[i] = outbuf[i];
	ic = ilitek_data->mcu_ver[0]+(ilitek_data->mcu_ver[1] << 8);
	tp_log_info("MCU KERNEL version:0x%x\n", ic);
    return ret;
}

int api_protocol_check_mode(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
    uint8_t buf[64] = {0};
	buf[0] = ILITEK_TP_CMD_READ_MODE;
	ret = ilitek_i2c_write_and_read(buf, 1, 5, outbuf, 2);
	tp_log_info("ilitek ic. mode =%d , it's %s\n", outbuf[0], ((outbuf[0] == 0x5A) ? "AP MODE" : "BL MODE"));
#ifdef ILITEK_UPDATE_FW
	if (outbuf[0] == 0x55)
		ilitek_data->force_update = true;
#endif
    return ret;
}

int api_protocol_get_fwver(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS, i = 0;
    uint8_t buf[64] = {0};
	buf[0] = ILITEK_TP_CMD_GET_FIRMWARE_VERSION;
	ret = ilitek_i2c_write_and_read(buf, 1, 5, outbuf, 8);
	for (i = 0; i < 8; i++)
		ilitek_data->firmware_ver[i] = outbuf[i];
	tp_log_info("firmware version:0x%x.0x%x.0x%x.0x%x.0x%x.0x%x.0x%x.0x%x\n",
	outbuf[0], outbuf[1], outbuf[2], outbuf[3], outbuf[4], outbuf[5], outbuf[6], outbuf[7]);
    return ret;
}

int api_protocol_get_ptl(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
    uint8_t buf[64] = {0};
	buf[0] = ILITEK_TP_CMD_GET_PROTOCOL_VERSION;
	if(ilitek_i2c_write_and_read(buf, 1, 5, outbuf, 3) < ILITEK_SUCCESS) {
		return ILITEK_FAIL;
	}
	ilitek_data->ptl.ver = (((int)outbuf[0]) << 16) + (((int)outbuf[1]) << 8);
	ilitek_data->ptl.ver_major = outbuf[0];
	ilitek_data->ptl.ver_mid = outbuf[1];
	ilitek_data->ptl.ver_minor = outbuf[2];
	tp_log_info("protocol version: %d.%d.%d  ilitek_data->ptl.ver = 0x%x\n",
	ilitek_data->ptl.ver_major, ilitek_data->ptl.ver_mid, ilitek_data->ptl.ver_minor, ilitek_data->ptl.ver);
    return ret;
}

int api_protocol_get_sc_resolution(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
    uint8_t buf[64] = {0};
    buf[0] = ILITEK_TP_CMD_GET_SCREEN_RESOLUTION;
	ret = ilitek_i2c_write_and_read(buf, 1, 5, outbuf, 8);
	ilitek_data->screen_min_x = outbuf[0];
	ilitek_data->screen_min_x += ((int)outbuf[1]) * 256;
	ilitek_data->screen_min_y = outbuf[2];
	ilitek_data->screen_min_y += ((int)outbuf[3]) * 256;
	ilitek_data->screen_max_x = outbuf[4];
	ilitek_data->screen_max_x += ((int)outbuf[5]) * 256;
	ilitek_data->screen_max_y = outbuf[6];
	ilitek_data->screen_max_y += ((int)outbuf[7]) * 256;
	tp_log_info("screen_min_x: %d, screen_min_y: %d screen_max_x: %d, screen_max_y: %d\n",
			ilitek_data->screen_min_x, ilitek_data->screen_min_y, ilitek_data->screen_max_x, ilitek_data->screen_max_y);
    return ret;
}

int api_protocol_set_sleep(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
    uint8_t buf[64] = {0};
	buf[0] = ILITEK_TP_CMD_SET_SLEEP;
	ret = ilitek_i2c_write_and_read(buf, 1, 0, outbuf, 0);
	return ret;
}

int api_protocol_set_wakeup(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
    uint8_t buf[64] = {0};
	buf[0] = ILITEK_TP_CMD_SET_WAKEUP;
	ret = ilitek_i2c_write_and_read(buf, 1, 0, outbuf, 0);
	return ret;
}

int api_protocol_get_v3_key_info(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS, i = 0;
    uint8_t *buf;
	int key_num = ilitek_data->keycount;
    buf = kmalloc(key_num * ILITEK_KEYINFO_FORMAT_LENGTH + ILITEK_KEYINFO_V3_HEADER, GFP_ATOMIC);
    buf[0] = ILITEK_TP_CMD_GET_KEY_INFORMATION;
    ret = ilitek_i2c_write_and_read(buf, 1, 10, outbuf, ILITEK_KEYINFO_FIRST_PACKET);
    if (ret < 0)
        return ret;
    if (key_num > 5) {
        for (i = 0; i < CEIL(key_num, ILITEK_KEYINFO_FORMAT_LENGTH) - 1; i++) {
            tp_log_info("read keyinfo times i = %d\n", i);
            ret = ilitek_i2c_write_and_read(buf, 0, 10,
             outbuf + ILITEK_KEYINFO_FIRST_PACKET + ILITEK_KEYINFO_OTHER_PACKET * i, ILITEK_KEYINFO_OTHER_PACKET);
            if (ret < 0)
                return ret;
        }
    }

    ilitek_data->key_xlen = (outbuf[0] << 8) + outbuf[1];
    ilitek_data->key_ylen = (outbuf[2] << 8) + outbuf[3];
    tp_log_info("key_xlen: %d, key_ylen: %d\n", ilitek_data->key_xlen, ilitek_data->key_ylen);

    //print key information
    for (i = 0; i < key_num; i++) {
        ilitek_data->keyinfo[i].id = outbuf[i * ILITEK_KEYINFO_FORMAT_LENGTH + ILITEK_KEYINFO_V3_HEADER + ILITEK_KEYINFO_FORMAT_ID];
        ilitek_data->keyinfo[i].x = (outbuf[i * ILITEK_KEYINFO_FORMAT_LENGTH + ILITEK_KEYINFO_V3_HEADER + ILITEK_KEYINFO_FORMAT_X_MSB] << 8)
         + outbuf[i * ILITEK_KEYINFO_FORMAT_LENGTH + ILITEK_KEYINFO_V3_HEADER + ILITEK_KEYINFO_FORMAT_X_LSB];
        ilitek_data->keyinfo[i].y = (outbuf[i * ILITEK_KEYINFO_FORMAT_LENGTH + ILITEK_KEYINFO_V3_HEADER + ILITEK_KEYINFO_FORMAT_Y_MSB] << 8)
         + outbuf[i * ILITEK_KEYINFO_FORMAT_LENGTH + ILITEK_KEYINFO_V3_HEADER + ILITEK_KEYINFO_FORMAT_Y_LSB];
        ilitek_data->keyinfo[i].status = 0;
        tp_log_info("key_id: %d, key_x: %d, key_y: %d, key_status: %d\n",
                ilitek_data->keyinfo[i].id, ilitek_data->keyinfo[i].x, ilitek_data->keyinfo[i].y, ilitek_data->keyinfo[i].status);
    }
    return ret;
}

int api_protocol_get_tp_resolution(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
    uint8_t buf[64] = {0};

	buf[0] = ILITEK_TP_CMD_GET_TP_RESOLUTION;
	ret = ilitek_i2c_write_and_read(buf, 1, 5, outbuf, 10);
	if (ret < 0)
		return ret;
	ilitek_data->max_tp = outbuf[6];
	ilitek_data->max_btn = outbuf[7];
	ilitek_data->keycount = outbuf[8];
	if (ilitek_data->keycount > 20) {
		tp_log_err("exception keycount > 20 is %d set keycount = 0\n", ilitek_data->keycount);
		ilitek_data->keycount = 0;
	}
	ilitek_data->tp_max_x = outbuf[0];
	ilitek_data->tp_max_x += ((int)outbuf[1]) << 8;
	ilitek_data->tp_max_y = outbuf[2];
	ilitek_data->tp_max_y += ((int)outbuf[3]) << 8;
	ilitek_data->x_ch = outbuf[4];
	ilitek_data->y_ch = outbuf[5];
	if (ilitek_data->keycount > 0) {
		//get key infotmation
        ret = api_protocol_get_v3_key_info(NULL, outbuf);
        if (ret < 0)
		    return ret;
	}
	if (ilitek_data->max_tp > 40) {
		tp_log_err("exception max_tp > 40 is %d set max_tp = 10\n", ilitek_data->max_tp);
		ilitek_data->max_tp = 10;
	}
	tp_log_info("tp_min_x: %d, tp_max_x: %d, tp_min_y: %d, tp_max_y: %d, ch_x: %d, ch_y: %d, max_tp: %d, key_count: %d\n",
		    ilitek_data->tp_min_x, ilitek_data->tp_max_x, ilitek_data->tp_min_y, ilitek_data->tp_max_y, ilitek_data->x_ch,
		    ilitek_data->y_ch, ilitek_data->max_tp, ilitek_data->keycount);
    return ret;
}

int api_protocol_get_module_name(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
    uint8_t buf[64] = {0};

	buf[0] = ILITEK_TP_CMD_GET_KERNEL_VERSION;
	if(ilitek_i2c_write_and_read(buf, 1, 5, buf, 32) < 0){
		return ILITEK_FAIL;
	}
	memcpy(outbuf, buf+6, 26);
	tp_log_info("module name:%s\n", outbuf);
    return ret;
}

//--------------------------protocol v6-------------------------//
int api_protocol_get_v6_key_info(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS, i = 0;
    uint8_t *buf;
	int key_num = ilitek_data->keycount;
	int w_len = key_num * ILITEK_KEYINFO_FORMAT_LENGTH + ILITEK_KEYINFO_V6_HEADER;
    buf = kmalloc(w_len, GFP_ATOMIC);
    buf[0] = ILITEK_TP_CMD_GET_KEY_INFORMATION;
    ret = ilitek_i2c_write_and_read(buf, 1, 10, outbuf, ILITEK_KEYINFO_FIRST_PACKET);
    if (ret < 0)
        return ret;

    ilitek_data->key_xlen = (outbuf[2] << 8) + outbuf[1];
    ilitek_data->key_ylen = (outbuf[4] << 8) + outbuf[3];
    tp_log_info("v6 key_xlen: %d, key_ylen: %d\n", ilitek_data->key_xlen, ilitek_data->key_ylen);

    //print key information
    for (i = 0; i < key_num; i++) {
        ilitek_data->keyinfo[i].id = outbuf[i * ILITEK_KEYINFO_FORMAT_LENGTH + ILITEK_KEYINFO_V6_HEADER + ILITEK_KEYINFO_FORMAT_ID];
        ilitek_data->keyinfo[i].x = (outbuf[i * ILITEK_KEYINFO_FORMAT_LENGTH + ILITEK_KEYINFO_V6_HEADER + ILITEK_KEYINFO_FORMAT_X_LSB] << 8)
         + outbuf[i * ILITEK_KEYINFO_FORMAT_LENGTH + ILITEK_KEYINFO_V6_HEADER + ILITEK_KEYINFO_FORMAT_X_MSB];
        ilitek_data->keyinfo[i].y = (outbuf[i * ILITEK_KEYINFO_FORMAT_LENGTH + ILITEK_KEYINFO_V6_HEADER + ILITEK_KEYINFO_FORMAT_Y_LSB] << 8)
         + outbuf[i * ILITEK_KEYINFO_FORMAT_LENGTH + ILITEK_KEYINFO_V6_HEADER + ILITEK_KEYINFO_FORMAT_Y_MSB];
        ilitek_data->keyinfo[i].status = 0;
        tp_log_info("key_id: %d, key_x: %d, key_y: %d, key_status: %d\n",
                ilitek_data->keyinfo[i].id, ilitek_data->keyinfo[i].x, ilitek_data->keyinfo[i].y, ilitek_data->keyinfo[i].status);
    }
    return ret;
}

int api_protocol_get_v6_tp_resolution(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
    uint8_t buf[64] = {0};

	buf[0] = ILITEK_TP_CMD_GET_TP_RESOLUTION;
	ret = ilitek_i2c_write_and_read(buf, 1, 5, outbuf, 15);
	if (ret < 0)
		return ret;
	ilitek_data->tp_max_x = outbuf[0];
	ilitek_data->tp_max_x += ((int)outbuf[1]) << 8;
	ilitek_data->tp_max_y = outbuf[2];
	ilitek_data->tp_max_y += ((int)outbuf[3]) << 8;
	ilitek_data->x_ch = outbuf[4] + ((int)outbuf[5] << 8);
	ilitek_data->y_ch = outbuf[6] + ((int)outbuf[7] << 8);
	ilitek_data->max_tp = outbuf[8];
	ilitek_data->keycount = outbuf[9];
	ilitek_data->slave_count = outbuf[10];
	ilitek_data->format = outbuf[12];
	if (ilitek_data->keycount > 20) {
		tp_log_err("exception keycount > 20 is %d set keycount = 0\n", ilitek_data->keycount);
		ilitek_data->keycount = 0;
	}
	if (ilitek_data->keycount > 0) {
		//get key infotmation
        ret = api_protocol_get_v6_key_info(NULL, outbuf);
        if (ret < 0)
		    return ret;
	}
	if (ilitek_data->max_tp > 40) {
		tp_log_err("exception max_tp > 40 is %d set max_tp = 10\n", ilitek_data->max_tp);
		ilitek_data->max_tp = 40;
	}
	tp_log_info("tp_min_x: %d, tp_max_x: %d, tp_min_y: %d, tp_max_y: %d, ch_x: %d, ch_y: %d, max_tp: %d, key_count: %d, slave_count: %d report format: %d\n",
		    ilitek_data->tp_min_x, ilitek_data->tp_max_x, ilitek_data->tp_min_y, ilitek_data->tp_max_y, ilitek_data->x_ch,
		    ilitek_data->y_ch, ilitek_data->max_tp, ilitek_data->keycount, ilitek_data->slave_count, ilitek_data->format);
    return ret;
}

int api_protocol_write_flash_enable(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
    uint8_t buf[64] = {0};
	buf[0] = ILITEK_TP_CMD_WRITE_FLASH_ENABLE;
	buf[1] = 0x5A;
	buf[2] = 0xA5;
	ret = ilitek_i2c_write_and_read(buf, 3, 0, outbuf, 0);
	return ret;
}

int api_set_data_length(uint32_t data_len)
{
    int ret = ILITEK_SUCCESS;
    uint8_t buf[64] = {0};

	buf[0] = ILITEK_TP_CMD_SET_DATA_LENGTH;
	buf[1] = (uint8_t)(data_len & 0xFF);
	buf[2] = (uint8_t)(data_len >> 8);
	ret = ilitek_i2c_write_and_read(buf, 3, 1, NULL, 0);
    return ret;
}

int api_write_flash_enable_BL1_8(uint32_t start,uint32_t end)
{
	uint8_t buf[64] = {0};

    buf[0] = (uint8_t)ILITEK_TP_CMD_WRITE_FLASH_ENABLE;//0xCC
    buf[1] = 0x5A;
    buf[2] = 0xA5;
    buf[3] = start & 0xFF;
    buf[4] = (start >> 8) & 0xFF;
    buf[5] = start >> 16;
    buf[6] = end & 0xFF;
    buf[7] = (end >> 8) & 0xFF;
    buf[8] = end >> 16;

	return ilitek_i2c_write_and_read(buf, 9, 1, NULL, 0);
}

int api_protocol_get_block_crc(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
    uint16_t crc=0;

    inbuf[0] = ILITEK_TP_CMD_GET_BLOCK_CRC;
    if(inbuf[1] == 0) {
    	ret = ilitek_i2c_write_and_read(inbuf, 8, 10, outbuf, 0);
		ret = ilitek_check_busy(50, 50, ILITEK_TP_SYSTEM_BUSY);
    }
    inbuf[1] = 1;
    ret = ilitek_i2c_write_and_read(inbuf, 2, 1, outbuf, 2);
    crc = outbuf[0]+(outbuf[1] << 8);
    return crc;
}

int api_protocol_set_mode(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
	inbuf[0] = ILITEK_TP_CMD_SET_MODE_CONTORL;
	inbuf[2] = 0;
	ret = ilitek_i2c_write_and_read(inbuf, 3, 100, outbuf, 0);
	return ret;
}

int api_protocol_set_short_Info(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
	ret = ilitek_i2c_write_and_read(inbuf, 6, 10, outbuf, 0);
	return ret;
}

int api_set_short_Info(int32_t Dump1, int32_t Dump2,uint8_t verf, uint8_t posidleL, uint8_t posidleH)
{
    uint8_t inbuf[6] = {0};
    inbuf[0] = ILITEK_TP_CMD_SET_SHORT_INFO;
    inbuf[1] = Dump1;
    inbuf[2] = Dump2;
    inbuf[3] = verf;
    inbuf[4] = posidleL;
    inbuf[5] = posidleH;

    if (api_protocol_set_cmd(ILITEK_TP_CMD_SET_SHORT_INFO, inbuf, NULL) < 0) {
        return ILITEK_FAIL;
    }
    return ILITEK_SUCCESS;
}

int api_protocol_set_open_Info(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
	ret = ilitek_i2c_write_and_read(inbuf, 4, 10, outbuf, 0);
	return ret;
}

int api_set_open_Info(uint8_t freqL, uint8_t freqH,uint8_t gain)
{
    uint8_t inbuf[6] = {0};
    inbuf[0] = ILITEK_TP_CMD_SET_OPEN_INFO;
    inbuf[1] = freqL;
    inbuf[2] = freqH;
    inbuf[3] = gain;

    if (api_protocol_set_cmd(ILITEK_TP_CMD_SET_OPEN_INFO, inbuf, NULL) < 0) {
        return ILITEK_FAIL;
    }
    return ILITEK_SUCCESS;
}

uint16_t api_get_block_crc(uint32_t start, uint32_t end, uint32_t type) {
	uint8_t inbuf[8] = {0}, outbuf[2] = {0};

	inbuf[1] = type;
	inbuf[2] = start;
	inbuf[3] = (start >> 8) & 0xFF;
	inbuf[4] = (start >> 16) & 0xFF;
	inbuf[5] = end & 0xFF;
	inbuf[6] = (end >> 8) & 0xFF;
	inbuf[7] = (end >> 16) & 0xFF;
	if (api_protocol_set_cmd(ILITEK_TP_CMD_GET_BLOCK_CRC, inbuf, outbuf) < 0) {
		return ILITEK_FAIL;
	}
	return 	outbuf[0]+(outbuf[1] << 8);
}

int api_get_slave_mode(int number)
{
    int i = 0;
    uint8_t inbuf[64] = {0}, outbuf[64] = {0};

    inbuf[0] = (uint8_t)ILITEK_TP_CMD_READ_MODE;
	if(ilitek_i2c_write_and_read(inbuf, 1, 5, outbuf, number*2) < 0)
		return ILITEK_FAIL;
    for(i = 0; i < number; i++){
        ilitek_data->ic[i].mode = outbuf[i*2];
        tp_log_info("IC[%d] mode: 0x%x\n", i, ilitek_data->ic[i].mode);
    }
	return ILITEK_SUCCESS;
}

uint32_t api_get_ap_crc(int number)
{
    int i = 0;
    uint8_t inbuf[64] = {0}, outbuf[64] = {0};

    inbuf[0] = ILITEK_TP_GET_AP_CRC;
	if(ilitek_i2c_write_and_read(inbuf, 1, 1, outbuf, number*2) < 0)
		return ILITEK_FAIL;
    for(i = 0; i < number; i++){
        ilitek_data->ic[i].crc = outbuf[i*2] + (outbuf[i*2+1] << 8);
        tp_log_info("IC[%d] CRC: 0x%x\n", i, ilitek_data->ic[i].crc);
    }
    return ILITEK_SUCCESS;
}

int api_set_access_slave(int number)
{
    int ret=0;
    uint8_t inbuf[64] = {0};

    inbuf[0] = (uint8_t)ILITEK_TP_CMD_ACCESS_SLAVE;
    inbuf[1] = 0x3;
    inbuf[2] = 0xC3;
	ret = ilitek_i2c_write_and_read(inbuf, 3, 1, NULL, 0);
	if (ilitek_check_busy(10, 100, ILITEK_TP_SYSTEM_BUSY) < 0)
	{
		tp_log_info("%s, Last: CheckBusy Failed\n", __func__);
		return ILITEK_FAIL;
	}
    return ILITEK_SUCCESS;
}

int api_protocol_set_cdc_initial(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;

	inbuf[0] = ILITEK_TP_CMD_SET_CDC_INITOAL_V6;
	ret = ilitek_i2c_write_and_read(inbuf, 2, 10, outbuf, 0);
	return ret;
}
/* Private define ------------------------------------------------------------*/
PROTOCOL_MAP ptl_func_map_v3[] =
{
    {ILITEK_TP_CMD_GET_TP_RESOLUTION, api_protocol_get_tp_resolution,"ILITEK_TP_CMD_GET_TP_RESOLUTION"},
    {ILITEK_TP_CMD_GET_SCREEN_RESOLUTION, api_protocol_get_sc_resolution, "ILITEK_TP_CMD_GET_SCREEN_RESOLUTION"},
    {ILITEK_TP_CMD_GET_KEY_INFORMATION, api_protocol_get_v3_key_info, "ILITEK_TP_CMD_GET_KEY_INFORMATION"},
    {ILITEK_TP_CMD_GET_FIRMWARE_VERSION, api_protocol_get_fwver, "ILITEK_TP_CMD_GET_FIRMWARE_VERSION"},
    {ILITEK_TP_CMD_READ_MODE, api_protocol_check_mode, "ILITEK_TP_CMD_READ_MODE"},
    {ILITEK_TP_CMD_GET_PROTOCOL_VERSION, api_protocol_get_ptl, "ILITEK_TP_CMD_GET_PROTOCOL_VERSION"},
    {ILITEK_TP_CMD_GET_KERNEL_VERSION, api_protocol_get_mcuver, "ILITEK_TP_CMD_GET_KERNEL_VERSION"},
	{ILITEK_TP_CMD_SET_FUNC_MODE, api_protocol_control_funcmode, "ILITEK_TP_CMD_SET_FUNC_MODE"},
	{ILITEK_TP_CMD_GET_SYSTEM_BUSY, api_protocol_system_busy, "ILITEK_TP_CMD_GET_SYSTEM_BUSY"},
	{ILITEK_TP_CMD_SET_APMODE, api_protocol_set_apmode, "ILITEK_TP_CMD_SET_APMODE"},
	{ILITEK_TP_CMD_SET_BLMODE, api_protocol_set_blmode, "ILITEK_TP_CMD_SET_BLMODE"},
	{ILITEK_TP_CMD_WRITE_ENABLE, api_protocol_write_enable, "ILITEK_TP_CMD_WRITE_ENABLE"},
	{ILITEK_TP_CMD_CONTROL_TESTMODE, api_protocol_control_tsmode, "ILITEK_TP_CMD_CONTROL_TESTMODE"},
	{ILITEK_TP_CMD_GET_MODULE_NAME, api_protocol_get_module_name, "ILITEK_TP_CMD_GET_MODULE_NAME"},
	{ILITEK_TP_CMD_SET_SLEEP, api_protocol_set_sleep, "ILITEK_TP_CMD_SET_SLEEP"},
	{ILITEK_TP_CMD_SET_WAKEUP, api_protocol_set_wakeup, "ILITEK_TP_CMD_SET_WAKEUP"},
};

PROTOCOL_MAP ptl_func_map_v6[] =
{
    {ILITEK_TP_CMD_GET_TP_RESOLUTION, api_protocol_get_v6_tp_resolution,"ILITEK_TP_CMD_GET_TP_RESOLUTION"},
    {ILITEK_TP_CMD_GET_SCREEN_RESOLUTION, api_protocol_get_sc_resolution, "ILITEK_TP_CMD_GET_SCREEN_RESOLUTION"},
    {ILITEK_TP_CMD_GET_KEY_INFORMATION, api_protocol_get_v6_key_info, "ILITEK_TP_CMD_GET_KEY_INFORMATION"},
    {ILITEK_TP_CMD_GET_FIRMWARE_VERSION, api_protocol_get_fwver, "ILITEK_TP_CMD_GET_FIRMWARE_VERSION"},
    {ILITEK_TP_CMD_READ_MODE, api_protocol_check_mode, "ILITEK_TP_CMD_READ_MODE"},
    {ILITEK_TP_CMD_GET_PROTOCOL_VERSION, api_protocol_get_ptl, "ILITEK_TP_CMD_GET_PROTOCOL_VERSION"},
    {ILITEK_TP_CMD_GET_KERNEL_VERSION, api_protocol_get_mcuver, "ILITEK_TP_CMD_GET_KERNEL_VERSION"},
	{ILITEK_TP_CMD_SET_FUNC_MODE, api_protocol_control_funcmode, "ILITEK_TP_CMD_SET_FUNC_MODE"},
	{ILITEK_TP_CMD_GET_SYSTEM_BUSY, api_protocol_system_busy, "ILITEK_TP_CMD_GET_SYSTEM_BUSY"},
	{ILITEK_TP_CMD_SET_APMODE, api_protocol_set_apmode, "ILITEK_TP_CMD_SET_APMODE"},
	{ILITEK_TP_CMD_SET_BLMODE, api_protocol_set_blmode, "ILITEK_TP_CMD_SET_BLMODE"},
	{ILITEK_TP_CMD_SET_MODE_CONTORL, api_protocol_set_mode, "ILITEK_TP_CMD_SET_MODE_CONTORL"},
	{ILITEK_TP_CMD_WRITE_FLASH_ENABLE, api_protocol_write_flash_enable, "ILITEK_TP_CMD_WRITE_FLASH_ENABLE"},
	{ILITEK_TP_CMD_GET_BLOCK_CRC, api_protocol_get_block_crc, "ILITEK_TP_CMD_GET_BLOCK_CRC"},
	{ILITEK_TP_CMD_SET_CDC_INITOAL_V6, api_protocol_set_cdc_initial, "ILITEK_TP_CMD_SET_CDC_INITOAL_V6"},
	{ILITEK_TP_CMD_SET_SHORT_INFO, api_protocol_set_short_Info, "ILITEK_TP_CMD_SET_SHORT_INFO"},
	{ILITEK_TP_CMD_SET_OPEN_INFO, api_protocol_set_open_Info, "ILITEK_TP_CMD_SET_OPEN_INFO"},	
	{ILITEK_TP_CMD_GET_MODULE_NAME, api_protocol_get_module_name, "ILITEK_TP_CMD_GET_MODULE_NAME"},
	{ILITEK_TP_CMD_SET_SLEEP, api_protocol_set_sleep, "ILITEK_TP_CMD_SET_SLEEP"},
	{ILITEK_TP_CMD_SET_WAKEUP, api_protocol_set_wakeup, "ILITEK_TP_CMD_SET_WAKEUP"},
};
//-----------------------extern api function------------------------// 
int api_protocol_set_cmd(uint16_t cmd, uint8_t *inbuf, uint8_t *outbuf)
{
    uint16_t cmd_id = 0;

    for(cmd_id = 0; cmd_id < ilitek_data->ptl.cmd_count; cmd_id++)
    {
        if(ptl_cb_func[cmd_id].cmd == cmd)
        {
			ptl_cb_func[cmd_id].func(inbuf, outbuf);
			tp_log_info("set command:%s\n", ptl_cb_func[cmd_id].name);
            return cmd_id;
        }
    }
	tp_log_info("command list not find, cmd:0x%x\n", cmd);
    return ILITEK_FAIL;
}

int api_protocol_set_testmode(bool testmode)
{
	uint8_t inbuf[3];
	if(ilitek_data->ptl.ver_major == 0x3) {
		if (testmode) {
			inbuf[1] = 0x01;	//inable test mode
		} else {
			inbuf[1] = 0x00;	//disable test mode
		}
		if (api_protocol_set_cmd(ILITEK_TP_CMD_CONTROL_TESTMODE, inbuf, NULL) < 0) {
			return ILITEK_FAIL;
		}
		mdelay(10);
	} else if(ilitek_data->ptl.ver_major == 0x6) {
		if (testmode) {
			inbuf[1] = ENTER_SUSPEND_MODE;	//inable test mode
		} else {
			inbuf[1] = ENTER_NORMAL_MODE;	//disable test mode
		}
		inbuf[2] = 0;
		if (api_protocol_set_cmd(ILITEK_TP_CMD_SET_MODE_CONTORL, inbuf, NULL) < 0) {
			return ILITEK_FAIL;
		}
		mdelay(100);
	}
	return ILITEK_SUCCESS;
}

int api_protocol_init_func(void){
	int ret = ILITEK_SUCCESS;
	uint8_t outbuf[64] = {0};
	if(api_protocol_get_ptl(NULL, outbuf) < ILITEK_SUCCESS)
		return ILITEK_FAIL;
	if(ilitek_data->ptl.ver_major == 0x3 || ilitek_data->ptl.ver == BL_V1_6 || ilitek_data->ptl.ver == BL_V1_7) {
		ptl_cb_func = ptl_func_map_v3;
		ilitek_data->ptl.cmd_count = PROTOCOL_CMD_NUM_V3;
		tp_log_info("command protocol: PROTOCOL_CMD_NUM_V3\n");
		ilitek_data->process_and_report = ilitek_read_data_and_report_3XX;
		ilitek_data->reset_time = 200;
		tp_log_info("report function: ilitek_read_data_and_report_3XX\n");
	} else if(ilitek_data->ptl.ver_major == 0x6 || ilitek_data->ptl.ver == BL_V1_8) {
		ptl_cb_func = ptl_func_map_v6;
		ilitek_data->ptl.cmd_count = PROTOCOL_CMD_NUM_V6;
		tp_log_info("command protocol: PROTOCOL_CMD_NUM_V6\n");
		ilitek_data->process_and_report = ilitek_read_data_and_report_6XX;
		ilitek_data->reset_time = 600;
		tp_log_info("report function: ilitek_read_data_and_report_6XX\n");
	}
	else {
		tp_log_info("Ilitek_Ver = %d\n", ilitek_data->ptl.ver);
		ptl_cb_func = ptl_func_map_v3;
		ilitek_data->ptl.cmd_count = PROTOCOL_CMD_NUM_V3;
		tp_log_info("command protocol: PROTOCOL_CMD_NUM_V3\n");
		ilitek_data->process_and_report = ilitek_read_data_and_report_3XX;
		ilitek_data->reset_time = 200;
		tp_log_info("report function: ilitek_read_data_and_report_3XX\n");
	}
	return ret;
}

int api_protocol_get_funcmode(int mode)
{
	int ret = ILITEK_SUCCESS;
	uint8_t inbuf[2], outbuf[64] = {0};

	inbuf[0] = ILITEK_TP_CMD_SET_FUNC_MODE;
	ret = ilitek_i2c_write_and_read(inbuf, 1, 10, outbuf, 3);
	tp_log_info("mode:%d\n", outbuf[2]);
	if(mode != outbuf[2]) {
		tp_log_info("Set mode fail, set:%d, read:%d\n", mode, outbuf[2]);
	}
	return ret;
}

int api_protocol_set_funcmode(int mode)
{
	int ret = ILITEK_SUCCESS, i = 0;
	uint8_t inbuf[4] = {ILITEK_TP_CMD_SET_FUNC_MODE, 0x55, 0xAA, mode}, outbuf[64] = {0};;
	tp_log_info("ilitek_data->ptl.ver:0x%x\n", ilitek_data->ptl.ver);
	if(ilitek_data->ptl.ver >= 0x30400){
		ret = api_protocol_control_funcmode(inbuf, NULL);
		for(i = 0; i < 20; i++)
		{
			mdelay(100);
			ret = api_protocol_system_busy(inbuf, outbuf);
			if(ret < ILITEK_SUCCESS)
				return ret;
			if(outbuf[0] == ILITEK_TP_SYSTEM_READY){
				tp_log_info("system is ready\n");
				ret = api_protocol_get_funcmode(mode);
				return ret;
			}
		}
		tp_log_info("system is busy\n");
	}
	else {
		tp_log_info("It is protocol not support\n");
	}
	return ret;
}
