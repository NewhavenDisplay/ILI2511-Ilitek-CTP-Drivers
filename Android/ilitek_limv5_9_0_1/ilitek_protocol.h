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
#ifndef __ILITEK_PROTOCOL_H__
#define __ILITEK_PROTOCOL_H__

/* Private typedef -----------------------------------------------------------*/
typedef int protocol_func(uint8_t *inbuf, uint8_t *outbuf);

typedef struct
{
    uint16_t cmd;
    protocol_func *func;
    uint8_t *name;
} PROTOCOL_MAP;
/* Private define ------------------------------------------------------------*/
//command spec
#define ILITEK_TP_CMD_GET_TP_RESOLUTION								0x20
#define ILITEK_TP_CMD_GET_SCREEN_RESOLUTION							0x21
#define ILITEK_TP_CMD_GET_KEY_INFORMATION							0x22
#define ILITEK_TP_CMD_SET_SLEEP										0x30
#define ILITEK_TP_CMD_SET_WAKEUP									0x31
#define ILITEK_TP_CMD_GET_FIRMWARE_VERSION							0x40
#define ILITEK_TP_CMD_GET_PROTOCOL_VERSION							0x42
#define ILITEK_TP_CMD_GET_KERNEL_VERSION							0x61
#define ILITEK_TP_CMD_SET_FUNC_MODE     							0x68
#define ILITEK_TP_CMD_SET_SHORT_INFO                                0x6A    //no support V3 protocol
#define ILITEK_TP_CMD_SET_OPEN_INFO                                 0x6D    //no support V3 protocol
#define ILITEK_TP_CMD_GET_SYSTEM_BUSY   							0x80
#define ILITEK_TP_CMD_READ_MODE										0xC0
#define ILITEK_TP_CMD_SET_APMODE									0xC1
#define ILITEK_TP_CMD_SET_BLMODE									0xC2
#define ILITEK_TP_GET_AP_CRC        								0xC7
#define ILITEK_TP_CMD_SET_DATA_LENGTH								0xC9    //no support V3 protocol
#define ILITEK_TP_CMD_ACCESS_SLAVE  								0xCB    //no support V3 protocol
#define ILITEK_TP_CMD_WRITE_FLASH_ENABLE							0xCC    //no support V3 protocol
#define ILITEK_TP_CMD_GET_BLOCK_CRC 			                    0xCD    //no support V3 protocol
#define ILITEK_TP_CMD_SET_MODE_CONTORL                              0xF0    //no support V3 protocol
#define ILITEK_TP_CMD_SET_CDC_INITOAL_V6		                    0xF1    //no support V3 protocol
#define ILITEK_TP_CMD_GET_CDC_DATA_V6   		                    0xF2    //no support V3 protocol
#define ILITEK_TP_CMD_CONTROL_TESTMODE								0xF2    //no support V6 protocol
#define ILITEK_TP_CMD_GET_MODULE_NAME								0x100

//TP system status
#define ILITEK_TP_SYSTEM_READY  									0x50
#define ILITEK_TP_NO_NEED                                           0
#define ILITEK_TP_SYSTEM_BUSY                                       0x1
#define ILITEK_TP_INITIAL_BUSY                                      0x1 << 1
#define ILITEK_TP_SET_MODE_0                                        0x0
#define ILITEK_TP_SET_MODE_1                                        0x1
#define ILITEK_TP_SET_MODE_2                                        0x2

//define Mode Control value
#define ENTER_NORMAL_MODE                                           0
#define ENTER_TEST_MODE                                             1
#define ENTER_DEBUG_MODE                                            2
#define ENTER_SUSPEND_MODE                                          3  //for 2326 Suspend Scan

#define CRC_CALCULATION_FROM_IC                                     0
#define CRC_GET_FROM_FLASH                                          1

//define raw data type value
#define TEST_MODE_V6_MC_RAW_BK                                      0x1
#define TEST_MODE_V6_MC_RAW_NBK                                     0x2
#define TEST_MODE_V6_MC_BG_BK                                       0x3
#define TEST_MODE_V6_MC_SE_BK                                       0x4
#define TEST_MODE_V6_MC_DAC_P                                       0x5
#define TEST_MODE_V6_MC_DAC_N                                       0x6
#define TEST_MODE_V6_SC_RAW_BK                                      0x9
#define TEST_MODE_V6_SC_RAW_NBK                                     0xA
#define TEST_MODE_V6_SC_BG_BK                                       0xB
#define TEST_MODE_V6_SC_SE_BK                                       0xC
#define TEST_MODE_V6_SC_DAC_P                                       0xD
#define TEST_MODE_V6_SC_DAC_N                                       0xE
#define TEST_MODE_V6_ICON_RAW_BK                                    0x11
#define TEST_MODE_V6_ICON_RAW_NBK                                   0x12
#define TEST_MODE_V6_ICON_BG_BK                                     0x13
#define TEST_MODE_V6_ICON_SE_BK                                     0x14
#define TEST_MODE_V6_OPEN                                           0x19
#define TEST_MODE_V6_SHORT_RX                                       0x1A
#define TEST_MODE_V6_SHORT_TX                                       0x1B
#define DEBUG_MODE_V6_SE_BK                                         0x23


#define PROTOCOL_CMD_NUM_V3		(sizeof(ptl_func_map_v3)/sizeof(PROTOCOL_MAP))
#define PROTOCOL_CMD_NUM_V6		(sizeof(ptl_func_map_v6)/sizeof(PROTOCOL_MAP))
/* Extern variables ----------------------------------------------------------*/
extern PROTOCOL_MAP *ptl_cb_func;
/* Extern functions ---------------------------------------------------------*/
extern int api_protocol_get_mcuver(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_check_mode(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_get_fwver(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_get_ptl(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_get_sc_resolution(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_get_v3_key_info(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_get_tp_resolution(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_init_func(void);
extern int api_protocol_set_cmd(uint16_t cmd, uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_write_enable(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_set_apmode(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_set_blmode(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_system_busy(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_control_tsmode(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_set_testmode(bool testmode);
extern int api_protocol_get_v6_tp_resolution(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_get_funcmode(int mode);
extern int api_protocol_set_funcmode(int mode);
extern int ilitek_read_data_and_report_3XX(void);
extern int ilitek_read_data_and_report_6XX(void);
extern int32_t ilitek_boot_upgrade_3XX(void);
extern int32_t ilitek_boot_upgrade_6XX(void);
extern uint16_t api_get_block_crc(uint32_t start, uint32_t end, uint32_t type);
extern int api_write_flash_enable_BL1_8(uint32_t start,uint32_t end);
extern int api_protocol_get_block_crc(uint8_t *inbuf, uint8_t *outbuf);
extern int api_set_data_length(uint32_t data_len);
extern int api_get_slave_mode(int number);
extern int api_set_access_slave(int number);
extern uint32_t api_get_ap_crc(int number);
extern int api_set_short_Info(int32_t Dump1, int32_t Dump2,uint8_t verf, uint8_t posidleL, uint8_t posidleH);
extern int api_set_open_Info(uint8_t freqL, uint8_t freqH,uint8_t gain);
#endif