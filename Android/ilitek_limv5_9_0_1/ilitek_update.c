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

#include <linux/firmware.h>
#include <linux/vmalloc.h>

#ifdef ILITEK_UPDATE_FW
#include "ilitek_fw.h"
int CTPM_FW_size = sizeof(CTPM_FW);
#endif

static uint16_t UpdateCRC(uint16_t crc, uint8_t newbyte)
{
	char i;			// loop counter
#define CRC_POLY 0x8408		// CRC16-CCITT FCS (X^16+X^12+X^5+1)

	crc = crc ^ newbyte;

	for (i = 0; i < 8; i++) {
		if (crc & 0x01) {
			crc = crc >> 1;
			crc ^= CRC_POLY;
		} else {
			crc = crc >> 1;
		}
	}
	return crc;
}

uint16_t get_dri_crc(uint32_t startAddr,uint32_t endAddr,uint8_t input[])
{
    uint16_t CRC = 0;
    uint32_t i = 0;
    // Process each byte in the page into the running CRC
	// 2 is CRC
    for(i = startAddr; i <= endAddr - 2; i++)
    {
        CRC = UpdateCRC (CRC, input[i]);
    }
    return CRC;
}

uint32_t get_dri_checksum(uint32_t startAddr,uint32_t endAddr,uint8_t input[])
{
    uint32_t sum = 0;
    uint32_t i = 0;
    // Process each byte in the page into the running CRC
    for(i = startAddr; i < endAddr; i++)
    {
        sum += input[i];
    }
    return sum;
}

static uint32_t findstr(uint8_t *data, int start, int n, const uint8_t *str, int len)
{
	int i, j;

	for (i = start; i <= (n - len); i++) {
		for (j = 0; j < len; j++)
			if (data[i + j] != str[j])
				break;
		if (j == len)
			return i;
	}

	return 0;
}

uint32_t get_ap_endaddr(uint32_t startAddr,uint32_t endAddr, uint8_t input[]) {
	uint32_t addr;
	const uint8_t TekTag[] ="ILITek AP CRC   ";
	uint8_t tag[32];

	memset(tag, 0xff, 32);
	memcpy(tag + 0x10, TekTag, 0x10);
	tp_log_info("start=0x%x, end=0x%x\n", startAddr, endAddr);
	addr = findstr(input, startAddr, endAddr, tag, 32) + 1 + 32; // 32 is Tag
	if(addr == 0)
		addr = endAddr;
	tp_log_info("Tag addr=0x%x\n", addr);
	return addr;
}

uint32_t get_block_endaddr(uint32_t startAddr,uint32_t endAddr, uint8_t input[]) {
	uint32_t addr;
	const uint8_t TekTag[] ="ILITek END TAG  ";
	uint8_t tag[32];

	memset(tag, 0xff, 32);
	memcpy(tag + 0x10, TekTag, 0x10);
	tp_log_info("start=0x%x, end=0x%x\n", startAddr, endAddr);
	addr = findstr(input, startAddr, endAddr, tag, 32) + 1 + 32; // 32 is Tag
	tp_log_info("Tag addr=0x%x\n", addr);
	return addr;
}

static int32_t check_busy(int32_t delay)
{
	int32_t i;
	uint8_t inbuf[2], outbuf[2];
	for (i = 0; i < 1000; i++) {
		inbuf[0] = ILITEK_TP_CMD_GET_SYSTEM_BUSY;
		if (ilitek_i2c_write_and_read(inbuf, 1, delay, outbuf, 1) < 0) {
			return ILITEK_I2C_TRANSFER_ERR;
		}
		if (outbuf[0] == ILITEK_TP_SYSTEM_READY) {
			//tp_log_info("check_busy i = %d\n", i);
			return 0;
		}
	}
	tp_log_info("check_busy error\n");
	return -1;
}

static int32_t ilitek_changetoblmode(bool mode)
{
	int32_t i = 0;
	uint8_t outbuf[64];
	if(api_protocol_set_cmd(ILITEK_TP_CMD_READ_MODE, NULL, outbuf) < ILITEK_SUCCESS)
		return ILITEK_I2C_TRANSFER_ERR;
	msleep(30);
	tp_log_info("ilitek ic. mode =%d , it's %s\n", outbuf[0],
		    ((outbuf[0] == 0x5A) ? "AP MODE" : ((outbuf[0] == ILITEK_TP_MODE_BOOTLOADER) ? "BL MODE" : "UNKNOW MODE")));
	if ((outbuf[0] == ILITEK_TP_MODE_APPLICATION && !mode) || (outbuf[0] == ILITEK_TP_MODE_BOOTLOADER && mode)) {
		if (mode) {
			tp_log_info("ilitek change to BL mode ok already BL mode\n");
		} else {
			tp_log_info("ilitek change to AP mode ok already AP mode\n");
		}
	} else {
		for (i = 0; i < 5; i++) {
			if((ilitek_data->bl_ver[0] == 0x1 && ilitek_data->bl_ver[1] >= 0x8) || (ilitek_data->ptl.ver_major == 0x6)) {
				if(api_protocol_set_cmd(ILITEK_TP_CMD_WRITE_FLASH_ENABLE, NULL, outbuf) < ILITEK_SUCCESS) {
					return ILITEK_I2C_TRANSFER_ERR;
				}
			}
			else {
				if(api_protocol_set_cmd(ILITEK_TP_CMD_WRITE_ENABLE, NULL, outbuf) < ILITEK_SUCCESS) {
					return ILITEK_I2C_TRANSFER_ERR;
				}
			}

			msleep(20);
			if (mode) {
				if(api_protocol_set_cmd(ILITEK_TP_CMD_SET_BLMODE, NULL, outbuf) < ILITEK_SUCCESS) {
					return ILITEK_I2C_TRANSFER_ERR;
				}
			} else {
				if(api_protocol_set_cmd(ILITEK_TP_CMD_SET_APMODE, NULL, outbuf) < ILITEK_SUCCESS) {
					return ILITEK_I2C_TRANSFER_ERR;
				}
			}
			msleep(500 + i*100);
			if(api_protocol_set_cmd(ILITEK_TP_CMD_READ_MODE, NULL, outbuf) < ILITEK_SUCCESS)
				return ILITEK_I2C_TRANSFER_ERR;
			msleep(30);
			tp_log_info("ilitek ic. mode =%d , it's  %s\n", outbuf[0],
				    ((outbuf[0] == ILITEK_TP_MODE_APPLICATION) ? "AP MODE" : ((outbuf[0] == ILITEK_TP_MODE_BOOTLOADER) ? "BL MODE" : "UNKNOW MODE")));
			if ((outbuf[0] == ILITEK_TP_MODE_APPLICATION && !mode) || (outbuf[0] == ILITEK_TP_MODE_BOOTLOADER && mode)) {
				if (mode) {
					tp_log_info("ilitek change to BL mode ok\n");
				} else {
					tp_log_info("ilitek change to AP mode ok\n");
				}
				break;
			}
		}
	}
	if (i >= 5) {
		if (mode) {
			tp_log_err("change to bl mode err, 0x%X\n", outbuf[0]);
			return ILITEK_TP_CHANGETOBL_ERR;
		} else {
			tp_log_err("change to ap mode err, 0x%X\n", outbuf[0]);
			return ILITEK_TP_CHANGETOAP_ERR;
		}
	} else {
		return ILITEK_SUCCESS;
	}
}

static int32_t ilitek_upgrade_BL1_6(uint32_t df_startaddr, uint32_t df_endaddr, uint32_t df_checksum,
			     uint32_t ap_startaddr, uint32_t ap_endaddr, uint32_t ap_checksum, uint8_t *CTPM_FW)
{
	int32_t ret = 0, upgrade_status = 0, i = 0, j = 0, k = 0, tmp_ap_end_addr = 0;
	uint8_t buf[64] = { 0 };
	if (ilitek_data->bl_ver[0] >= 1 && ilitek_data->bl_ver[1] >= 4) {
		buf[0] = (uint8_t)ILITEK_TP_CMD_WRITE_ENABLE;	//0xc4
		buf[1] = 0x5A;
		buf[2] = 0xA5;
		buf[3] = 0x81;
		if (!ilitek_data->has_df) {
			tp_log_info("ilitek no df data set df_endaddr 0x1ffff\n");
			df_endaddr = 0x1ffff;
			df_checksum = 0x1000 * 0xff;
			buf[4] = df_endaddr >> 16;
			buf[5] = (df_endaddr >> 8) & 0xFF;
			buf[6] = (df_endaddr) & 0xFF;
			buf[7] = df_checksum >> 16;
			buf[8] = (df_checksum >> 8) & 0xFF;
			buf[9] = df_checksum & 0xFF;
		} else {
			buf[4] = df_endaddr >> 16;
			buf[5] = (df_endaddr >> 8) & 0xFF;
			buf[6] = (df_endaddr) & 0xFF;
			buf[7] = df_checksum >> 16;
			buf[8] = (df_checksum >> 8) & 0xFF;
			buf[9] = df_checksum & 0xFF;
		}
		tp_log_info("ilitek df_startaddr=0x%X, df_endaddr=0x%X, df_checksum=0x%X\n", df_startaddr, df_endaddr, df_checksum);
		ret = ilitek_i2c_write(buf, 10);
		if (ret < 0) {
			tp_log_err("ilitek_i2c_write\n");
		}
		msleep(20);
		j = 0;
		for (i = df_startaddr; i < df_endaddr; i += 32) {
			buf[0] = ILITEK_TP_CMD_WRITE_DATA;
			for (k = 0; k < 32; k++) {
				if (ilitek_data->has_df) {
					if ((i + k) >= df_endaddr) {
						buf[1 + k] = 0x00;
					} else {
						buf[1 + k] = CTPM_FW[i + 32 + k];
					}
				} else {
					buf[1 + k] = 0xff;
				}
			}

			ret = ilitek_i2c_write(buf, 33);
			if (ret < 0) {
				tp_log_err("ilitek_i2c_write_and_read\n");
				return ILITEK_I2C_TRANSFER_ERR;
			}
			j += 1;

#ifdef ILI_UPDATE_BY_CHECK_INT
			for (k = 0; k < 40; k++) {
				if (!(ilitek_poll_int())) {
					break;
				} else {
					mdelay(1);
				}
			}
			if (k >= 40) {
				tp_log_err("upgrade check int fail retry = %d\n", k);
			}
#else
			if ((j % (ilitek_data->page_number)) == 0) {
				//msleep(30);
				mdelay(20);
			}
			mdelay(10);
#endif
			upgrade_status = ((i - df_startaddr) * 100) / (df_endaddr - df_startaddr);
			if (upgrade_status % 10 == 0) {
				tp_log_info("%c ilitek ILITEK: Firmware Upgrade(Data flash), %02d%c.\n", 0x0D, upgrade_status, '%');
			}
		}
		buf[0] = (uint8_t)ILITEK_TP_CMD_WRITE_ENABLE;	//0xc4
		buf[1] = 0x5A;
		buf[2] = 0xA5;
		buf[3] = 0x80;
		if (((ap_endaddr + 1) % 32) != 0) {
			tp_log_info("ap_endaddr = 0x%X\n", (int32_t)(ap_endaddr + 32 + 32 - ((ap_endaddr + 1) % 32)));
			buf[4] = (ap_endaddr + 32 + 32 - ((ap_endaddr + 1) % 32)) >> 16;
			buf[5] = ((ap_endaddr + 32 + 32 - ((ap_endaddr + 1) % 32)) >> 8) & 0xFF;
			buf[6] = ((ap_endaddr + 32 + 32 - ((ap_endaddr + 1) % 32))) & 0xFF;
			tp_log_info("ap_checksum = 0x%X\n", (int32_t)(ap_checksum + (32 + 32 - ((ap_endaddr + 1) % 32)) * 0xff));
			buf[7] = (ap_checksum + (32 + 32 - ((ap_endaddr + 1) % 32)) * 0xff) >> 16;
			buf[8] = ((ap_checksum + (32 + 32 - ((ap_endaddr + 1) % 32)) * 0xff) >> 8) & 0xFF;
			buf[9] = (ap_checksum + (32 + 32 - ((ap_endaddr + 1) % 32)) * 0xff) & 0xFF;
		} else {
			tp_log_info("32 0 ap_endaddr  32 = 0x%X\n", (int32_t)(ap_endaddr + 32));
			tp_log_info("ap_endaddr = 0x%X\n", (int32_t)(ap_endaddr + 32));
			buf[4] = (ap_endaddr + 32) >> 16;
			buf[5] = ((ap_endaddr + 32) >> 8) & 0xFF;
			buf[6] = ((ap_endaddr + 32)) & 0xFF;
			tp_log_info("ap_checksum = 0x%X\n", (int32_t)(ap_checksum + 32 * 0xff));
			buf[7] = (ap_checksum + 32 * 0xff) >> 16;
			buf[8] = ((ap_checksum + 32 * 0xff) >> 8) & 0xFF;
			buf[9] = (ap_checksum + 32 * 0xff) & 0xFF;
		}
		ret = ilitek_i2c_write(buf, 10);
		msleep(20);
		tmp_ap_end_addr = ap_endaddr;
		ap_endaddr += 32;
		j = 0;
		for (i = ap_startaddr; i < ap_endaddr; i += 32) {
			buf[0] = ILITEK_TP_CMD_WRITE_DATA;
			for (k = 0; k < 32; k++) {
				if ((i + k) > tmp_ap_end_addr) {
					buf[1 + k] = 0xff;
				} else {
					buf[1 + k] = CTPM_FW[i + k + 32];
				}
			}

			buf[0] = 0xc3;
			ret = ilitek_i2c_write(buf, 33);
			if (ret < 0) {
				tp_log_err("%s, ilitek_i2c_write_and_read\n", __func__);
				return ILITEK_I2C_TRANSFER_ERR;
			}
			j += 1;
#ifdef ILI_UPDATE_BY_CHECK_INT
			for (k = 0; k < 40; k++) {
				if (!(ilitek_poll_int())) {
					break;
				} else {
					mdelay(1);
				}
			}
			if (k >= 40) {
				tp_log_err("upgrade check int fail retry = %d\n", k);
			}
#else
			if ((j % (ilitek_data->page_number)) == 0) {
				mdelay(20);
			}
			mdelay(10);
#endif
			upgrade_status = (i * 100) / ap_endaddr;
			if (upgrade_status % 10 == 0) {
				tp_log_info("%c ilitek ILITEK: Firmware Upgrade(AP), %02d%c.\n", 0x0D, upgrade_status, '%');
			}
		}
	} else {
		tp_log_info("not support this bl version upgrade flow\n");
	}
	return 0;
}

static int32_t ilitek_upgrade_BL1_7(uint32_t df_startaddr, uint32_t df_endaddr, uint32_t ap_startaddr, uint32_t ap_endaddr, uint8_t *CTPM_FW)
{
	int32_t ret = 0, upgrade_status = 0, i = 0, k = 0, CRC_DF = 0, CRC_AP = 0;
	uint8_t buf[64] = { 0 };
	for (i = df_startaddr + 2; i < df_endaddr; i++) {
		CRC_DF = UpdateCRC(CRC_DF, CTPM_FW[i + 32]);
	}
	tp_log_info("CRC_DF = 0x%X\n", CRC_DF);

	CRC_AP = get_dri_crc(ap_startaddr, ap_endaddr, CTPM_FW + 32);

	tp_log_info("CRC_AP = 0x%x\n", CRC_AP);

	buf[0] = (uint8_t)ILITEK_TP_CMD_WRITE_ENABLE;	//0xc4
	buf[1] = 0x5A;
	buf[2] = 0xA5;
	buf[3] = 0x01;
	buf[4] = df_endaddr >> 16;
	buf[5] = (df_endaddr >> 8) & 0xFF;
	buf[6] = (df_endaddr) & 0xFF;
	buf[7] = CRC_DF >> 16;
	buf[8] = (CRC_DF >> 8) & 0xFF;
	buf[9] = CRC_DF & 0xFF;
	ret = ilitek_i2c_write(buf, 10);
	if (ret < 0) {
		tp_log_err("ilitek_i2c_write\n");
		return ILITEK_I2C_TRANSFER_ERR;
	}
	check_busy(1);
	if (((df_endaddr) % 32) != 0) {
		df_endaddr += 32;
	}
	tp_log_info("write data to df mode\n");
	for (i = df_startaddr; i < df_endaddr; i += 32) {
		// we should do delay when the size is equal to 512 bytes
		buf[0] = (uint8_t)ILITEK_TP_CMD_WRITE_DATA;
		for (k = 0; k < 32; k++) {
			buf[1 + k] = CTPM_FW[i + k + 32];
		}
		if (ilitek_i2c_write(buf, 33) < 0) {
			tp_log_err("%s, ilitek_i2c_write_and_read\n", __func__);
			return ILITEK_I2C_TRANSFER_ERR;
		}
		check_busy(1);
		upgrade_status = ((i - df_startaddr) * 100) / (df_endaddr - df_startaddr);
		//tp_log_info("%c ilitek ILITEK: Firmware Upgrade(Data flash), %02d%c.\n",0x0D,upgrade_status,'%');
	}

	buf[0] = (uint8_t)0xC7;
	if (ilitek_i2c_write(buf, 1) < 0) {
		tp_log_err("ilitek_i2c_write\n");
		return ILITEK_I2C_TRANSFER_ERR;
	}
	check_busy(1);
	buf[0] = (uint8_t)0xC7;
	ilitek_i2c_write_and_read(buf, 1, 10, buf, 4);
	tp_log_info("upgrade end write c7 read 0x%X, 0x%X, 0x%X, 0x%X\n", buf[0], buf[1], buf[2], buf[3]);
	if (CRC_DF != (buf[1] << 8) + buf[0]) {
		tp_log_err("CRC DF compare error\n");
		//return ILITEK_UPDATE_FAIL;
	} else {
		tp_log_info("CRC DF compare Right\n");
	}
	buf[0] = (uint8_t)ILITEK_TP_CMD_WRITE_ENABLE;	//0xc4
	buf[1] = 0x5A;
	buf[2] = 0xA5;
	buf[3] = 0x00;
	buf[4] = (ap_endaddr + 1) >> 16;
	buf[5] = ((ap_endaddr + 1) >> 8) & 0xFF;
	buf[6] = ((ap_endaddr + 1)) & 0xFF;
	buf[7] = 0;
	buf[8] = (CRC_AP & 0xFFFF) >> 8;
	buf[9] = CRC_AP & 0xFFFF;
	ret = ilitek_i2c_write(buf, 10);
	if (ret < 0) {
		tp_log_err("ilitek_i2c_write\n");
		return ILITEK_I2C_TRANSFER_ERR;
	}
	check_busy(1);
	if (((ap_endaddr + 1) % 32) != 0) {
		tp_log_info("ap_endaddr += 32\n");
		ap_endaddr += 32;
	}
	tp_log_info("write data to AP mode\n");
	for (i = ap_startaddr; i < ap_endaddr; i += 32) {
		buf[0] = (uint8_t)ILITEK_TP_CMD_WRITE_DATA;
		for (k = 0; k < 32; k++) {
			buf[1 + k] = CTPM_FW[i + k + 32];
		}
		if (ilitek_i2c_write(buf, 33) < 0) {
			tp_log_err("ilitek_i2c_write\n");
			return ILITEK_I2C_TRANSFER_ERR;
		}
		check_busy(1);
		upgrade_status = ((i - ap_startaddr) * 100) / (ap_endaddr - ap_startaddr);
		//tp_log_info("%c ilitek ILITEK: Firmware Upgrade(AP), %02d%c.\n",0x0D,upgrade_status,'%');
	}
	for (i = 0; i < 20; i++) {
		buf[0] = (uint8_t)0xC7;
		if (ilitek_i2c_write(buf, 1) < 0) {
			tp_log_err("ilitek_i2c_write\n");
			return ILITEK_I2C_TRANSFER_ERR;
		}
		check_busy(1);
		buf[0] = (uint8_t)0xC7;
		ret = ilitek_i2c_write_and_read(buf, 1, 10, buf, 4);
		if (ret < 0) {
			tp_log_err("ilitek_i2c_write_and_read 0xc7\n");
			return ILITEK_I2C_TRANSFER_ERR;
		}
		tp_log_info("upgrade end write c7 read 0x%X, 0x%X, 0x%X, 0x%X\n", buf[0], buf[1], buf[2], buf[3]);
		if (CRC_AP != (buf[1] << 8) + buf[0]) {
			tp_log_err("CRC compare error retry\n");
			//return ILITEK_TP_UPGRADE_FAIL;
		} else {
			tp_log_info("CRC compare Right\n");
			break;
		}
	}
	if (i >= 20) {
		tp_log_err("CRC compare error\n");
		return ILITEK_TP_UPGRADE_FAIL;
	}
	return 0;
}

int program_block_BL1_8(uint8_t *buffer, int block, uint32_t len) {
    int k = 0, ret = ILITEK_SUCCESS, i;
    uint16_t dri_crc = 0, ic_crc = 0;
	static uint8_t *buff;

	//buff = vmalloc(sizeof(uint8_t) * (len+1));
	buff = kmalloc(len+1, GFP_KERNEL);
	if (NULL == buff) {
		tp_log_err("buff NULL\n");
		return -ENOMEM;
	}
	memset(buff, 0xff, len+1);
    dri_crc = get_dri_crc(ilitek_data->blk[block].start, ilitek_data->blk[block].end, buffer);
    ret = api_write_flash_enable_BL1_8(ilitek_data->blk[block].start, ilitek_data->blk[block].end);
	printk("Upgrade block %d\n", block);
    for (i = ilitek_data->blk[block].start; i < ilitek_data->blk[block].end; i += len) //i += update_len - 1)
    {
        buff[0] = (unsigned char)ILITEK_TP_CMD_WRITE_DATA;
        for (k = 0; k < len; k++)
        {
            buff[1 + k] = buffer[i + k];
        }
		ret = ilitek_i2c_write_and_read(buff, len + 1, 1, NULL, 0);
		if (ilitek_check_busy(40, 50, ILITEK_TP_SYSTEM_BUSY) < 0)
		{
			tp_log_err("%s, Write Datas: CheckBusy Failed\n", __func__);
			return ILITEK_FAIL;
		}
        printk("ILITEK: Firmware Upgrade, %02d%c.\n",((i - ilitek_data->blk[block].start + 1) * 100) / (ilitek_data->blk[block].end - ilitek_data->blk[block].start), '%');
    }
    tp_log_info("upgrade firmware, 100%c\n", '%');

    ic_crc = api_get_block_crc(ilitek_data->blk[block].start, ilitek_data->blk[block].end, CRC_GET_FROM_FLASH);
    tp_log_info("Block:%d start:0x%x end:0x%x Real=0x%x,Get=0x%x\n\n", block, ilitek_data->blk[block].start, ilitek_data->blk[block].end, dri_crc, ic_crc);
    kfree(buff);
	if (ic_crc < 0)
    {
        tp_log_err("WriteAPDatas Last: CheckBusy Failed\n");
        return ILITEK_FAIL;
    }
    if (dri_crc != ic_crc)
    {
        tp_log_err("WriteAPData: CheckSum Failed! Real=0x%x,Get=0x%x\n", dri_crc, ic_crc);
        return ILITEK_FAIL;
    }
	return ILITEK_SUCCESS;
}

int program_slave_BL1_8(uint8_t *buffer, int block, uint32_t len) {
    int ret = ILITEK_SUCCESS, i;
    uint16_t dri_crc = 0;
    bool update = false;


    ilitek_data->ic = (struct ilitek_ic_info*)kmalloc(ilitek_data->slave_count, sizeof(struct ilitek_ic_info));
    //check protocol
    if(ilitek_data->ptl.ver < PROTOCOL_V6) {
        tp_log_info("It is protocol not support\n");
        return ILITEK_FAIL;
    }
    ret = api_protocol_set_testmode(true);

    ret = api_get_slave_mode(ilitek_data->slave_count);

    ret = api_get_ap_crc(ilitek_data->slave_count);

    dri_crc = get_dri_crc(ilitek_data->blk[block].start, ilitek_data->blk[block].end, buffer);
    
    for(i = 0; i < ilitek_data->slave_count; i++) {
        if(dri_crc != ilitek_data->ic[i].crc) {
            tp_log_info("Check CRC fail, must FW upgrade\n Driver CRC:0x%x ,IC[%d]_CRC:0x%x\n", dri_crc, i, ilitek_data->ic[i].crc);
            update = true;
            break;
        }
        if(ilitek_data->ic[i].mode != ILITEK_TP_MODE_APPLICATION) {
            tp_log_info("Check IC Mode fail, must FW upgrade\n IC[%d]_Mode:0x%x\n", i, ilitek_data->ic[i].mode);
            update = true;
            break;
        }
    }

    if(update) {
        ret = api_set_access_slave(ilitek_data->slave_count);
        ret = api_write_flash_enable_BL1_8(ilitek_data->blk[block].start, ilitek_data->blk[block].end);
        tp_log_info("Please wait updating...\n");
        msleep(15000);
        ret = api_get_slave_mode(ilitek_data->slave_count);
        ret = api_get_ap_crc(ilitek_data->slave_count);

        dri_crc = get_dri_crc(ilitek_data->blk[block].start, ilitek_data->blk[block].end, buffer);

        for(i = 0; i < ilitek_data->slave_count; i++) {
            if(dri_crc != ilitek_data->ic[i].crc) {
                tp_log_info("Check CRC fail, FW upgrade Fail\n Driver CRC:0x%x ,IC[%d]_CRC:0x%x\n", dri_crc, i, ilitek_data->ic[i].crc);
                return ILITEK_FAIL;
            }
            if(ilitek_data->ic[i].mode != ILITEK_TP_MODE_APPLICATION) {
                tp_log_info("Check IC Mode fail, FW upgrade Fail\n IC[%d]_Mode:0x%x\n", i, ilitek_data->ic[i].mode);
                return ILITEK_FAIL;
            }
        }
    }

	if(ilitek_read_tp_info(false) < 0) {
		tp_log_err("init read tp info error so exit\n");
		return ILITEK_FAIL;
	}
	return ILITEK_SUCCESS;
}

static int32_t ilitek_upgrade_BL1_8(uint8_t *CTPM_FW)
{
    int ret = ILITEK_SUCCESS, count = 0;
    //uint32_t update_len = UPGRADE_LENGTH_BLV1_8;
	uint32_t update_len = set_update_len;
    ret = api_set_data_length(update_len);
    for(count = 0; count < ilitek_data->upg.blk_num; count++) {
        if(ilitek_data->blk[count].chk_crc == false)
        {
            if(program_block_BL1_8(CTPM_FW+32, count, update_len) < 0) {
                tp_log_err("Upgrade Block:%d Fail\n", count);
                return ILITEK_FAIL;
            }
        }
    }

    if (ilitek_changetoblmode(false) == ILITEK_FAIL)
    {
        tp_log_err("Change to ap mode failed\n");
        return ILITEK_FAIL;
    }
    msleep(500);
	if(ilitek_read_tp_info(false) < 0) {
		tp_log_err("init read tp info error so exit\n");
		return ILITEK_FAIL;
	}
	tp_log_info("");
    if(ilitek_data->mcu_ver[0] + (ilitek_data->mcu_ver[1] << 8) == 0x2326) {
        tp_log_info("Firmware Upgrade on Slave\n");
        ret = program_slave_BL1_8(CTPM_FW+32, 0, update_len);
    }
    tp_log_info("Firmware Upgrade Success\n");
	ilitek_reset(ilitek_data->reset_time, false);
    return ret;
}

bool check_FW_upgrade(unsigned char *buffer) {
	uint16_t dri_crc = 0, ic_crc = 0;
	int count = 0;
	if(ilitek_data->ptl.ver >= PROTOCOL_V6 || ilitek_data->ptl.ver == BL_V1_8) {
		int update = false;

		for(count = 0; count < ilitek_data->upg.blk_num; count++) {
			ilitek_data->blk[count].chk_crc = false;
			ic_crc = api_get_block_crc(ilitek_data->blk[count].start, ilitek_data->blk[count].end, CRC_CALCULATION_FROM_IC);
			dri_crc = get_dri_crc(ilitek_data->blk[count].start, ilitek_data->blk[count].end, buffer);
			if(ic_crc == dri_crc)
				ilitek_data->blk[count].chk_crc = true;
			if(ilitek_data->blk[count].chk_crc == false)
				update = true;
			tp_log_info("Block:%d, Start address:0x%6X, End address:0x%6X, IC CRC:0x%x, Driver CRC:0x%x Check:%d\n",
			count, ilitek_data->blk[count].start, ilitek_data->blk[count].end, ic_crc, dri_crc, ilitek_data->blk[count].chk_crc);
		}
		return update;
	}
	return true;
}
int32_t ilitek_upgrade_frimware(uint32_t df_startaddr, uint32_t df_endaddr, uint32_t df_checksum,
				 uint32_t ap_startaddr, uint32_t ap_endaddr, uint32_t ap_checksum, uint8_t *CTPM_FW)
{
	int32_t ret = 0, retry = 0;
	uint8_t buf[64] = { 0 };
	bool update;

Retry:
	update = true;
	if (retry < 2) {
		retry++;
	} else {
		tp_log_err("retry 2 times upgrade fail\n");
		return ret;
	}

	tp_log_info("upgrade firmware start	reset\n");
	ilitek_reset(ilitek_data->reset_time, false);
	if (api_protocol_set_testmode(true) < ILITEK_SUCCESS)
		goto Retry;
	ilitek_check_busy(1000, 1, ILITEK_TP_SYSTEM_BUSY);
	//check ic type
	if (api_protocol_set_cmd(ILITEK_TP_CMD_GET_KERNEL_VERSION, NULL, buf) < ILITEK_SUCCESS)
		goto Retry;
	update = check_FW_upgrade(CTPM_FW+32);
	if(update) {
		ret = ilitek_changetoblmode(true);
		if (ret) {
			tp_log_err("change to bl mode err ret = %d\n", ret);
			goto Retry;
		} else {
			tp_log_info("ilitek change to bl mode ok\n");
		}
		if (api_protocol_set_cmd(ILITEK_TP_CMD_GET_PROTOCOL_VERSION, NULL, buf) < ILITEK_SUCCESS)
			goto Retry;
		tp_log_info("bl protocol version %d.%d\n", buf[0], buf[1]);
		ilitek_data->bl_ver[0] = buf[0];
		ilitek_data->bl_ver[1] = buf[1];
		ilitek_data->page_number = 16;
		buf[0] = 0xc7;
		ret = ilitek_i2c_write_and_read(buf, 1, 10, buf, 1);
		tp_log_info("0xc7 read= %x\n", buf[0]);
	}
	if(((ilitek_data->ptl.ver & 0xFFFF00) == BL_V1_8) || ilitek_data->ptl.ver >= PROTOCOL_V6) {
		ret = ilitek_upgrade_BL1_8(CTPM_FW);
		if (ret < 0) {
			tp_log_err("ilitek_upgrade_BL1_8 err ret = %d\n", ret);
			goto Retry;
		}
	} else if((ilitek_data->ptl.ver & 0xFFFF00) == BL_V1_7) {
		//df_startaddr = 0xF000;
		ret = ilitek_upgrade_BL1_7(df_startaddr, df_endaddr, ap_startaddr, ap_endaddr, CTPM_FW);
		if (ret < 0) {
			tp_log_err("ilitek_upgrade_BL1_7 err ret = %d\n", ret);
			goto Retry;
		}
	} else if((ilitek_data->ptl.ver & 0xFFFF00) == BL_V1_6) {
		//df_startaddr = 0x1F000;
		if (df_startaddr < df_endaddr) {
			ilitek_data->has_df = true;
		} else {
			ilitek_data->has_df = false;
		}
		ret = ilitek_upgrade_BL1_6(df_startaddr, df_endaddr, df_checksum, ap_startaddr, ap_endaddr, ap_checksum, CTPM_FW);
		if (ret < 0) {
			tp_log_err("ilitek_upgrade_BL1_6 err ret = %d\n", ret);
			goto Retry;
		}
	} else {
		tp_log_err("not support is protocol, BL protocol:%d.%d\n", ilitek_data->bl_ver[0], ilitek_data->bl_ver[1]);
		goto Retry;
	}
	tp_log_info("upgrade firmware completed	reset\n");
	ilitek_reset(ilitek_data->reset_time, false);

	ret = ilitek_changetoblmode(false);
	if (ret) {
		tp_log_err("change to ap mode err\n");
		goto Retry;
	} else {
		tp_log_info("ilitek change to ap mode ok\n");
	}
	ret = api_protocol_init_func();
	return 0;
}

#ifdef ILITEK_UPDATE_FW
int32_t ilitek_boot_upgrade_3XX(void)
{
	int32_t ret = 0, i = 0, ap_len = 0, df_len = 0;
	uint32_t ap_startaddr = 0, df_startaddr = 0, ap_endaddr = 0, df_endaddr = 0, ap_checksum = 0, df_checksum = 0;
	uint8_t firmware_ver[8] = { 0 };
	uint8_t compare_version_count = 0;
	uint8_t buf[5] = {0};

	ap_startaddr = (CTPM_FW[0] << 16) + (CTPM_FW[1] << 8) + CTPM_FW[2];
	ap_endaddr = (CTPM_FW[3] << 16) + (CTPM_FW[4] << 8) + CTPM_FW[5];
	ap_checksum = (CTPM_FW[6] << 16) + (CTPM_FW[7] << 8) + CTPM_FW[8];
	ilitek_data->upgrade_mcu_ver[0] = CTPM_FW[10];
	ilitek_data->upgrade_mcu_ver[1] = CTPM_FW[11];
	df_endaddr = (CTPM_FW[12] << 16) + (CTPM_FW[13] << 8) + CTPM_FW[14];
	df_checksum = (CTPM_FW[15] << 16) + (CTPM_FW[16] << 8) + CTPM_FW[17];
	firmware_ver[0] = CTPM_FW[18];
	firmware_ver[1] = CTPM_FW[19];
	firmware_ver[2] = CTPM_FW[20];
	firmware_ver[3] = CTPM_FW[21];
	firmware_ver[4] = CTPM_FW[22];
	firmware_ver[5] = CTPM_FW[23];
	firmware_ver[6] = CTPM_FW[24];
	firmware_ver[7] = CTPM_FW[25];
	df_len = (CTPM_FW[26] << 16) + (CTPM_FW[27] << 8) + CTPM_FW[28];
	ap_len = (CTPM_FW[29] << 16) + (CTPM_FW[30] << 8) + CTPM_FW[31];
	compare_version_count = 8;
	tp_log_info("compare_version_count = %d\n", compare_version_count);

	if (!(ilitek_data->force_update)) {
		for (i = 0; i < compare_version_count; i++) {
			tp_log_info("ilitek_data.firmware_ver[%d] = %d, firmware_ver[%d] = %d\n", i, ilitek_data->firmware_ver[i], i, firmware_ver[i]);
			if (firmware_ver[i] < ilitek_data->firmware_ver[i]) {
				i = compare_version_count;
				break;
			}
			if (firmware_ver[i] > ilitek_data->firmware_ver[i]) {
				break;
			}
		}
		if (i >= compare_version_count) {
			tp_log_info("firmware version is older so not upgrade\n");
			return ILITEK_SUCCESS;
		}
	}
	ret = ILITEK_FAIL;
	if ((ilitek_data->upgrade_mcu_ver[0] != 0 || ilitek_data->upgrade_mcu_ver[1] != 0)
		&& (ilitek_data->mcu_ver[0] != ilitek_data->upgrade_mcu_ver[0] || ilitek_data->mcu_ver[1] != ilitek_data->upgrade_mcu_ver[1])) {
		tp_log_info("upgrade file mismatch!!! ic is ILI%02X%02X, upgrade file is ILI%02X%02X\n", ilitek_data->mcu_ver[1],
				ilitek_data->mcu_ver[0], ilitek_data->upgrade_mcu_ver[1], ilitek_data->upgrade_mcu_ver[0]);
	} else if (ilitek_data->upgrade_mcu_ver[0] == 0 && ilitek_data->upgrade_mcu_ver[1] == 0 && ilitek_data->mcu_ver[0] != 0x03
			&& ilitek_data->mcu_ver[0] != 0x09) {
		tp_log_info("upgrade file  mismatch!!! ic is ILI%02X%02X, upgrade file is maybe ILI230X\n", ilitek_data->mcu_ver[1],
				ilitek_data->mcu_ver[0]);
	} else {
		ret = ilitek_changetoblmode(true);
		if (ret) {
			tp_log_err("change to bl mode err ret = %d\n", ret);
			return ILITEK_FAIL;
		} else {
			tp_log_info("ilitek change to bl mode ok\n");
		}
		ret = api_protocol_set_cmd(ILITEK_TP_CMD_GET_KERNEL_VERSION, NULL, buf);
		df_startaddr = (buf[2] << 16)  + (buf[3] << 8) + buf[4];
		tp_log_info("ilitek ap_startaddr=0x%X, ap_endaddr=0x%X, ap_checksum=0x%X, ap_len = %d\n", ap_startaddr, ap_endaddr, ap_checksum, ap_len);
		tp_log_info("ilitek df_startaddr=0x%X, df_endaddr=0x%X, df_checksum=0x%X, df_len = %d\n", df_startaddr, df_endaddr, df_checksum, df_len);
		ret = ilitek_upgrade_frimware(df_startaddr, df_endaddr, df_checksum, ap_startaddr, ap_endaddr, ap_checksum, CTPM_FW);
	}
	if (ret < 0) {
		tp_log_err("upgrade fail\n");
		return ret;
	}
	return ILITEK_SUCCESS;
}

int32_t ilitek_boot_upgrade_6XX(void)
{
	int32_t ret = ILITEK_SUCCESS, i = 0;
	uint8_t firmware_ver[8] = { 0 };
	uint8_t compare_version_count = 8;
	uint8_t *update_buf;

	firmware_ver[0] = CTPM_FW[18];
	firmware_ver[1] = CTPM_FW[19];
	firmware_ver[2] = CTPM_FW[20];
	firmware_ver[3] = CTPM_FW[21];
	firmware_ver[4] = CTPM_FW[22];
	firmware_ver[5] = CTPM_FW[23];
	firmware_ver[6] = CTPM_FW[24];
	firmware_ver[7] = CTPM_FW[25];
	if (!(ilitek_data->force_update)) {
		for (i = 0; i < compare_version_count; i++) {
			tp_log_info("ilitek_data.firmware_ver[%d] = %d, firmware_ver[%d] = %d\n", i, ilitek_data->firmware_ver[i], i, firmware_ver[i]);
			if (firmware_ver[i] < ilitek_data->firmware_ver[i]) {
				i = compare_version_count;
				break;
			}
			if (firmware_ver[i] > ilitek_data->firmware_ver[i]) {
				break;
			}
		}
		if (i >= compare_version_count) {
			tp_log_info("firmware version is older so not upgrade\n");
			return ILITEK_SUCCESS;
		}
	}
	update_buf = vmalloc(ILITEK_HEX_UPGRADE_SIZE);
	memset(update_buf, 0xff, ILITEK_HEX_UPGRADE_SIZE);
	ilitek_data->upgrade_FW_info_addr = (CTPM_FW[10] << 8) + CTPM_FW[11];
	tp_log_info("upgrade_FW_info_addr=0x%x\n", ilitek_data->upgrade_FW_info_addr);
	hex_mapping_convert(ilitek_data->upgrade_FW_info_addr, CTPM_FW+32);
	if(ilitek_data->upg.hex_ic_type[0] != ilitek_data->mcu_ver[0] && ilitek_data->upg.hex_ic_type[1] != ilitek_data->mcu_ver[1]) {
		tp_log_info("ic and hex no match, ic:%x  hex:%x\n", ilitek_data->mcu_ver[0] + (ilitek_data->mcu_ver[1] << 8),
		ilitek_data->upg.hex_ic_type[0] + (ilitek_data->upg.hex_ic_type[1] << 8));
		return ILITEK_FAIL;
	}
	memcpy(update_buf, CTPM_FW, CTPM_FW_size);
	ret = ilitek_upgrade_frimware(0, 0, 0, 0, 0, 0, update_buf);
	vfree(update_buf);
	if (ret < 0) {
		tp_log_err("upgrade fail\n");
		return ret;
	}
	return ILITEK_SUCCESS;
}
#endif
int32_t ilitek_bin_upgrade_3XX(struct device *dev, const char *fn) {
	const struct firmware *fw = NULL;
	uint8_t *CTPM_FW;
	int ret;
	uint32_t ap_startaddr = ILI25XX_AP_START_ADDRESS, ap_endaddr = 0, ap_checksum = 0;
	uint32_t df_startaddr = 0xF000, df_endaddr = 0, df_checksum = 0;
	uint8_t buf[5] = {0};

	tp_log_info("path :%s\n", fn);
	ret = request_firmware(&fw, fn, dev);
	if (ret) {
		tp_log_err("Unable to open firmware %s, %d\n", fn, ret);
		return ret;
	}
	//tp_log_info("size=%d\n", fw->size);
	CTPM_FW = vmalloc(fw->size + ILI25XX_AP_START_ADDRESS + ILITEK_ILI_HEADER_LENGTH);
	//tp_log_info("size=%d\n", fw->size + ILI25XX_AP_START_ADDRESS + ILITEK_ILI_HEADER_LENGTH);
	memcpy(CTPM_FW + ILITEK_ILI_HEADER_LENGTH, fw->data, fw->size);
	msleep(10);

	ret = ilitek_changetoblmode(true);
	if (ret) {
		tp_log_err("change to bl mode err ret = %d\n", ret);
		goto FAIL;
	} else {
		tp_log_info("ilitek change to bl mode ok\n");
	}
	ret = api_protocol_set_cmd(ILITEK_TP_CMD_GET_KERNEL_VERSION, NULL, buf);
	if (buf[1] == 0x23) {
		ap_startaddr = ILI23XX_AP_START_ADDRESS;
	}
	else if (buf[1] == 0x25 || buf[1] == 0x27) {
		ap_startaddr = ILI25XX_AP_START_ADDRESS;
	}
	else {
		tp_log_info("no support is IC:0x%x\n", (buf[1] << 8) + buf[0]);
		goto FAIL;
	}
	df_startaddr = (buf[2] << 16)  + (buf[3] << 8) + buf[4];
	ap_endaddr = get_ap_endaddr(ap_startaddr, df_startaddr,CTPM_FW + ILITEK_ILI_HEADER_LENGTH);
	df_endaddr = fw->size - 1;
	if (buf[1] == 0x23) {
		ap_endaddr += 4; // 4 is 4 byte Checksum
		ap_checksum = get_dri_checksum(ap_startaddr, ap_endaddr, CTPM_FW + ILITEK_ILI_HEADER_LENGTH);
	}
	else {
		ap_endaddr += 2; //2 is 2 byte CRC
		ap_checksum = get_dri_crc(ap_startaddr, ap_endaddr, CTPM_FW + ILITEK_ILI_HEADER_LENGTH);
	}

	df_checksum = get_dri_checksum(df_startaddr, df_endaddr, CTPM_FW + ILITEK_ILI_HEADER_LENGTH);
	tp_log_info("ilitek ap_startaddr=0x%X, ap_endaddr=0x%X, ap_checksum=0x%X\n", ap_startaddr, ap_endaddr, ap_checksum);
	tp_log_info("ilitek df_startaddr=0x%X, df_endaddr=0x%X, df_checksum=0x%X\n", df_startaddr, df_endaddr, df_checksum);
	ret = ilitek_upgrade_frimware( df_startaddr, df_endaddr, df_checksum, ap_startaddr, ap_endaddr, ap_checksum, CTPM_FW);
	if (ret < 0) {
		tp_log_err("upgrade fail\n");
		goto FAIL;
	}
FAIL:
	vfree(CTPM_FW);
	tp_log_info("ret = %d\n", ret);
	return ret;
}

int32_t ilitek_bin_upgrade_6XX(struct device *dev, const char *fn) {
	const struct firmware *fw = NULL;
	uint8_t *CTPM_FW;
	int ret;

	tp_log_info("path :%s\n", fn);
	ret = request_firmware(&fw, fn, dev);
	if (ret) {
		tp_log_err("Unable to open firmware %s, %d\n", fn, ret);
		return ret;
	}
	//tp_log_info("size=%d\n", fw->size);
	CTPM_FW = vmalloc(fw->size + LEGO_AP_START_ADDRESS + ILITEK_ILI_HEADER_LENGTH);
	//tp_log_info("size=%d\n", fw->size + LEGO_AP_START_ADDRESS + ILITEK_ILI_HEADER_LENGTH);
	memcpy(CTPM_FW+LEGO_AP_START_ADDRESS+ILITEK_ILI_HEADER_LENGTH, fw->data, fw->size);
	msleep(10);

	ilitek_data->upgrade_FW_info_addr = MEMONY_MAPPING_ADDRESS_V6;
	tp_log_info("upgrade_FW_info_addr=0x%x\n", ilitek_data->upgrade_FW_info_addr);
	hex_mapping_convert(ilitek_data->upgrade_FW_info_addr, CTPM_FW+32);
	ret = ilitek_upgrade_frimware(0, 0, 0, 0, 0, 0, CTPM_FW);
	if (ret < 0) {
		tp_log_err("upgrade fail\n");
		return ret;
	}
	vfree(CTPM_FW);
	return 0;
}
