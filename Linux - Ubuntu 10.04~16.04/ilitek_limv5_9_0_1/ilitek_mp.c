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
#include "ilitek_mp.h"

#define PARSER_MAX_CFG_BUF          (512 * 3)
#define PARSER_MAX_KEY_NUM	        (600)
#define PARSER_MAX_KEY_NAME_LEN	    100
#define PARSER_MAX_KEY_VALUE_LEN	2000

#define INI_ERR_OUT_OF_LINE     -1

struct ini_file_data {
	char pSectionName[PARSER_MAX_KEY_NAME_LEN];
	char pKeyName[PARSER_MAX_KEY_NAME_LEN];
	char *pKeyValue;
	//char pKeyValue[PARSER_MAX_KEY_VALUE_LEN];
	int iSectionNameLen;
	int iKeyNameLen;
	int iKeyValueLen;
} *ilitek_ini_file_data;

int g_ini_items;

extern int32_t short_test_result;
extern int32_t open_test_result;
extern int32_t mopen_test_result;
extern int32_t allnode_test_result;
extern int32_t ilitek_short_threshold;
extern int32_t ilitek_open_threshold;
extern int32_t ilitek_allnode_max_threshold;
extern int32_t ilitek_allnode_min_threshold;
extern int32_t ilitek_open_txdeltathrehold;
extern int32_t ilitek_open_rxdeltathrehold;
extern int32_t ilitek_win1_failpoint;
extern int32_t ilitek_win2_failpoint;
extern int32_t ilitek_allnodetestw1;
extern int32_t ilitek_allnodetestw2;
extern int32_t ilitek_allnodemodule;
extern int32_t ilitek_allnodetx;
extern int32_t ilitek_printsensortestdata;
extern int32_t ilitek_Dump1_Value;
extern int32_t ilitek_Dump2_Value;
extern int32_t noisefre_start;
extern int32_t noisefre_end;
extern int32_t noisefre_step;
extern int32_t ilitek_short_Vref;
extern int32_t ilitek_posidle_L;
extern int32_t ilitek_posidle_H;

extern char sensor_test_data_path[256];
extern char sensor_test_data_path_tmp[256];
extern char noisefre_data_path[256];
extern char noisefre_data_path_tmp[256];

static char *ini_str_trim_r(char *buf)
{
	int len, i;
	char *tmp;

	len = strlen(buf);
	tmp = kmalloc(len, GFP_KERNEL);
	for (i = 0; i < len; i++) {
		if (buf[i] != ' ')
			break;
	}

	if (i < len)
		strncpy(tmp, (buf + i), (len - i));

	strncpy(buf, tmp, len);
	kfree(tmp);
	return buf;
}

/* Count the number of each line and assign the content to tmp buffer */
static int get_ini_phy_line(char *data, char *buffer, int maxlen)
{
	int i = 0;
	int j = 0;
	int iRetNum = -1;
	char ch1 = '\0';

	for (i = 0, j = 0; i < maxlen; j++) {
		ch1 = data[j];
		iRetNum = j + 1;
		if (ch1 == '\n' || ch1 == '\r') {	/* line end */
			ch1 = data[j + 1];
			if (ch1 == '\n' || ch1 == '\r') {
				iRetNum++;
			}

			break;
		} else if (ch1 == 0x00) {
			//iRetNum = -1;
			break;	/* file end */
		}

		buffer[i++] = ch1;
	}

	buffer[i] = '\0';
	return iRetNum;
}

static int get_ini_phy_data(char *data, int fsize)
{
	int i, n = 0, ret = 0, node_flag = 0, empty_section;
	int offset = 0, isEqualSign = 0;
	char *ini_buf = NULL, *tmpSectionName = NULL, *test_buf;
	char M_CFG_SSL = '[';
	char M_CFG_SSR = ']';
	char M_CFG_NTS = '#';
	char M_CFG_EQS = '=';
	bool format = false;
	
	if (data == NULL) {
		tp_log_err("INI data is NULL\n");
		ret = -EINVAL;
		goto out;
	}

	ini_buf = kzalloc((PARSER_MAX_CFG_BUF + 1) * sizeof(char), GFP_KERNEL);
	test_buf = kzalloc((PARSER_MAX_CFG_BUF + 1) * sizeof(char), GFP_KERNEL);
	if (ERR_ALLOC_MEM(ini_buf)) {
		tp_log_err("Failed to allocate ini_buf memory, %ld\n", PTR_ERR(ini_buf));
		ret = -ENOMEM;
		goto out;
	}
	tmpSectionName = kzalloc((PARSER_MAX_CFG_BUF + 1) * sizeof(char), GFP_KERNEL);
	if (ERR_ALLOC_MEM(tmpSectionName)) {
		tp_log_err("Failed to allocate tmpSectionName memory, %ld\n", PTR_ERR(tmpSectionName));
		ret = -ENOMEM;
		goto out;
	}
	while (true) {
		node_flag = 0;
		empty_section = 0;
		if (g_ini_items > PARSER_MAX_KEY_NUM) {
			tp_log_err("MAX_KEY_NUM: Out of length\n");
			goto out;
		}
		if (offset >= fsize)
			goto out;/*over size*/
		n = get_ini_phy_line(data + offset, ini_buf, PARSER_MAX_CFG_BUF);
		if (n < 0) {
			tp_log_err("End of Line\n");
			goto out;
		}
		memcpy(test_buf, ini_buf, n);
		offset += n;
		n = strlen(ini_str_trim_r(ini_buf));
		if (n == 0 || ini_buf[0] == M_CFG_NTS)
			continue;
		/* Get section names */
		if (n > 2 && ((ini_buf[0] == M_CFG_SSL && ini_buf[n - 1] != M_CFG_SSR))) {
			tp_log_err("Bad Section: %s\n", ini_buf);
			ret = -EINVAL;
			goto out;
		} else {
			if (ini_buf[0] == M_CFG_SSL) {
				ilitek_ini_file_data[g_ini_items].iSectionNameLen = n - 2;
				if (ilitek_ini_file_data[g_ini_items].iSectionNameLen > PARSER_MAX_KEY_NAME_LEN) {
					tp_log_err("MAX_KEY_NAME_LEN: Out Of Length\n");
					ret = INI_ERR_OUT_OF_LINE;
					goto out;
				}
				
				ini_buf[n - 1] = 0x00;
				strcpy((char *)tmpSectionName, ini_buf + 1);
				node_flag = 0;
				tp_log_info("Section Name: %s, Len: %d, offset = %d\n", tmpSectionName, n - 2, offset);
				continue;
			}
		}
		/* copy section's name without square brackets to its real buffer */
		strcpy(ilitek_ini_file_data[g_ini_items].pSectionName, tmpSectionName);
		ilitek_ini_file_data[g_ini_items].iSectionNameLen = strlen(tmpSectionName);
		isEqualSign = 0;
		for (i = 0; i < n; i++) {
			if (ini_buf[i] == M_CFG_EQS) {
				isEqualSign = i;
				break;
			}
			if (ini_buf[i] == M_CFG_SSL || ini_buf[i] == M_CFG_SSR) {
				empty_section = 1;
				break;
			}
		}
		if (isEqualSign == 0) {
			if (empty_section)
				continue;

			if (strcmp(ilitek_ini_file_data[g_ini_items].pSectionName, BENCHMARK_KEY_NAME) == 0 ||
				strcmp(ilitek_ini_file_data[g_ini_items].pSectionName, ALLNODE_WIN2_KEY_NAME) == 0 ||
				strcmp(ilitek_ini_file_data[g_ini_items].pSectionName, ALLNODE_RAWDATA_KEY_NAME) == 0 ||
				strcmp(ilitek_ini_file_data[g_ini_items].pSectionName, ALLNODE_WIN1_KEY_NAME) == 0 || 
				strcmp(ilitek_ini_file_data[g_ini_items].pSectionName, MIRCOOPEN_RX_DELTA) == 0 || 
				strcmp(ilitek_ini_file_data[g_ini_items].pSectionName, MIRCOOPEN_TX_AVG_DELTA) == 0 ||
				strcmp(ilitek_ini_file_data[g_ini_items].pSectionName, BENCHMARK_RAW_KEY_NAME) == 0 ||
				strcmp(ilitek_ini_file_data[g_ini_items].pSectionName, BENCHMARK_WIN1_KEY_NAME) == 0 ||
				strcmp(ilitek_ini_file_data[g_ini_items].pSectionName, BENCHMARK_WIN2_KEY_NAME) == 0) {
				node_flag = 1;
				isEqualSign = -1;
				format = true;
			} else {
				continue;
			}
		}
		if (node_flag) {
		/* Get Key names */
			ilitek_ini_file_data[g_ini_items].iKeyNameLen = strlen(NODE_KEY_NAME);
			strcpy(ilitek_ini_file_data[g_ini_items].pKeyName, NODE_KEY_NAME);
			ilitek_ini_file_data[g_ini_items].iKeyValueLen = n;
        ilitek_ini_file_data[g_ini_items].pKeyValue = kzalloc((ilitek_ini_file_data[g_ini_items].iKeyValueLen + 1)* sizeof(u8), GFP_KERNEL);
        if (ERR_ALLOC_MEM(ilitek_ini_file_data[g_ini_items].pKeyValue)) {
            tp_log_err("Failed to allocate Key Value memory, %ld\n", PTR_ERR(ilitek_ini_file_data[g_ini_items].pKeyValue));
            goto out;
        }
		}
		else {
		/* Get Key names */
			ilitek_ini_file_data[g_ini_items].iKeyNameLen = isEqualSign;
			if (ilitek_ini_file_data[g_ini_items].iKeyNameLen > PARSER_MAX_KEY_NAME_LEN) {
				/* ret = CFG_ERR_OUT_OF_LEN; */
				tp_log_err("MAX_KEY_NAME_LEN: Out Of Length\n");
				ret = INI_ERR_OUT_OF_LINE;
				goto out;
			}

			ilitek_memcpy(ilitek_ini_file_data[g_ini_items].pKeyName, ini_buf,
										ilitek_ini_file_data[g_ini_items].iKeyNameLen, PARSER_MAX_KEY_NAME_LEN);
			ilitek_ini_file_data[g_ini_items].iKeyValueLen = n - isEqualSign - 1;
			ilitek_ini_file_data[g_ini_items].pKeyValue = kzalloc((ilitek_ini_file_data[g_ini_items].iKeyValueLen + 1)* sizeof(u8), GFP_KERNEL);
			if (ERR_ALLOC_MEM(ilitek_ini_file_data[g_ini_items].pKeyValue)) {
				tp_log_err("Failed to allocate Key Value memory, %ld\n", PTR_ERR(ilitek_ini_file_data[g_ini_items].pKeyValue));
				goto out;
			}
		}

		/* Get a value assigned to a key */
		if (ilitek_ini_file_data[g_ini_items].iKeyValueLen > PARSER_MAX_KEY_VALUE_LEN) {
			tp_log_err("MAX_KEY_VALUE_LEN: Out Of Length\n");
			ret = INI_ERR_OUT_OF_LINE;
			goto out;
		}
		//tp_log_info("iKeyNameLen=%d, pKeyValue=%d\n", ilitek_ini_file_data[g_ini_items].iKeyNameLen, ilitek_ini_file_data[g_ini_items].iKeyValueLen);
		memcpy(ilitek_ini_file_data[g_ini_items].pKeyValue,
		       ini_buf + isEqualSign + 1, ilitek_ini_file_data[g_ini_items].iKeyValueLen);
		if(format == false)
			tp_log_info("[%d]:%s = %s\n", g_ini_items, ilitek_ini_file_data[g_ini_items].pKeyName,
				ilitek_ini_file_data[g_ini_items].pKeyValue);
		g_ini_items++;
	}
out:
	ilitek_kfree((void **)&ini_buf);
	ilitek_kfree((void **)&tmpSectionName);
	return ret;
}

static void init_ilitek_ini_data(void)
{
	int i;

	g_ini_items = 0;
	/* Initialise ini strcture */
	for (i = 0; i < PARSER_MAX_KEY_NUM; i++) {
		memset(ilitek_ini_file_data[i].pSectionName, 0, PARSER_MAX_KEY_NAME_LEN);
		memset(ilitek_ini_file_data[i].pKeyName, 0, PARSER_MAX_KEY_NAME_LEN);
		ilitek_ini_file_data[i].iSectionNameLen = 0;
		ilitek_ini_file_data[i].iKeyNameLen = 0;
		ilitek_ini_file_data[i].iKeyValueLen = 0;
	}
	ilitek_data->mp.uni.ben = (struct ilitek_node_info*)kzalloc(
		ilitek_data->x_ch * ilitek_data->y_ch * sizeof(struct ilitek_node_info), GFP_KERNEL);
	ilitek_data->mp.txavg = (struct ilitek_node_info*)kzalloc(
		(ilitek_data->y_ch - 1) * sizeof(struct ilitek_node_info), GFP_KERNEL);
	ilitek_data->mp.rxdiff = (struct ilitek_node_info*)kzalloc(
		(ilitek_data->x_ch - 1) * ilitek_data->y_ch * sizeof(struct ilitek_node_info), GFP_KERNEL);
	ilitek_data->mp.uni.raw = (struct ilitek_node_info*)kzalloc(
		ilitek_data->x_ch * (ilitek_data->y_ch - 1)* sizeof(struct ilitek_node_info), GFP_KERNEL);
	ilitek_data->mp.uni.win1 = (struct ilitek_node_info*)kzalloc(
		ilitek_data->x_ch * ilitek_data->y_ch * sizeof(struct ilitek_node_info), GFP_KERNEL);
	ilitek_data->mp.uni.win2 = (struct ilitek_node_info*)kzalloc(
		(ilitek_data->x_ch - 1) * (ilitek_data->y_ch -1 )* sizeof(struct ilitek_node_info), GFP_KERNEL);
}

/* get_ini_key_value - get ini's key and value based on its section from its array
 *
 * A function is digging into the key and value by its section from the ini array.
 * The comparsion is not only a string's name, but its length.
 */
static int get_ini_key_value(char *section, char *key, char *value)
{
	int i = 0;
	int ret = ILITEK_FAIL;
	int len = 0;

	len = strlen(key);
	memset(value, 0, INI_ROW_LENGTH);

	for (i = 0; i < g_ini_items; i++) {
		if (strcmp(section, ilitek_ini_file_data[i].pSectionName) != 0)
			continue;

		if (strcmp(key, ilitek_ini_file_data[i].pKeyName) == 0) {
			ilitek_memcpy(value, ilitek_ini_file_data[i].pKeyValue, ilitek_ini_file_data[i].iKeyValueLen, PARSER_MAX_KEY_VALUE_LEN);
			//tp_log_info( " value:%s , pKeyValue: %s\n", value, ilitek_ini_file_data[i].pKeyValue);
			ret = ILITEK_SUCCESS;
			break;
		}
	}
	return ret;
}


void get_parser_node(char *section, struct ilitek_node_info *ptr, int x)
{
	int i = 0, j = 0, index1 = 0, temp, count = 0, start = 0, y = 0;
	char str[512] = { 0 }, record = ',';
	int data[4];

	/* format complete string from the name of section "_Benchmark_Data". */
	for (i = 0; i < g_ini_items; i++) {
		if (strcmp(section, ilitek_ini_file_data[i].pSectionName) == 0) {
			tp_log_info("find section, set:%s find:%s, count:%d\n", section, ilitek_ini_file_data[i].pSectionName, i);
			start = i;
			break;
		}
	}
	//for (i = start, y = 0; i < start + ilitek_data->y_ch; i++, y++) {
	while (true) {
		if(strcmp(NODE_KEY_NAME, ilitek_ini_file_data[i].pKeyName) != 0 || strcmp(section, ilitek_ini_file_data[i].pSectionName) != 0)
			break;
		//record = ',';
		count = 0;
		for (j = 0, index1 = 0; j <= ilitek_ini_file_data[i].iKeyValueLen; j++) {
			if (ilitek_ini_file_data[i].pKeyValue[j] == ',' || ilitek_ini_file_data[i].pKeyValue[j] == ';' 
				|| ilitek_ini_file_data[i].pKeyValue[j] == '.' || j == ilitek_ini_file_data[i].iKeyValueLen) {

				if (record != '.') {
					memset(str, 0, sizeof(str));
					ilitek_memcpy(str, &ilitek_ini_file_data[i].pKeyValue[index1], (j - index1), sizeof(str));
					//temp = katoi(str);
					sscanf(str, "%d", &temp);
					data[(count % 4)] = temp;
					/* Over boundary, end to calculate. */
					if ((count / 4) >= ilitek_ini_file_data[i].iKeyValueLen) {
						tp_log_err("count (%d) is larger than frame length, break, iKeyValueLen : %d\n", (count/4), ilitek_ini_file_data[i].iKeyValueLen);
						break;
					}
					if ((count % 4) == 3) {
						ptr[y * x + (count/4)].data = data[0];
						ptr[y * x + (count/4)].max = data[1];
						ptr[y * x + (count/4)].min = data[2];
						ptr[y * x + (count/4)].type = data[3];
					}
					count++;
				}
				record = ilitek_ini_file_data[i].pKeyValue[j];
				index1 = j + 1;
			}
		}
		i++;
		y++;
	}
	tp_log_info("[%s] count=%d, x=%d, y=%d\n", section, count, x, y);
	printk("%s data\n", section);
	for(i = 0; i < x * y; i++) {
		if(i % x == 0)
			printk("\n");
		printk("%d,", ptr[i].data);
	}
	printk("\n %s max\n", section);
	for(i = 0; i < x * y; i++) {
		if(i % x == 0)
			printk("\n");
		printk("%d,", ptr[i].max);
	}
	printk("\n %s min\n", section);
	for(i = 0; i < x * y; i++) {
		if(i % x == 0)
			printk("\n");
		printk("%d,", ptr[i].min);
	}
	printk("\n");
}
//EXPORT_SYMBOL(get_parser_node);
//void parser_node(char *pKeyValue, int frame_len)
void parser_node(char *pKeyValue, struct ilitek_node_info * ptr, int frame_len, int y)
{
	int j = 0, index1 = 0, temp, count = 0;
	char str[512] = { 0 }, record = ',';
	uint16_t data[4];

	/* format complete string from the name of section "_Benchmark_Data". */
	//for (i = 0; i < g_ini_items; i++) {
		record = ',';
		for (j = 0, index1 = 0; j <= frame_len; j++) {
			if (pKeyValue[j] == ',' || pKeyValue[j] == ';' || pKeyValue[j] == '.' || j == frame_len) {

				if (record != '.') {
					memset(str, 0, sizeof(str));
					ilitek_memcpy(str, &pKeyValue[index1], (j - index1), sizeof(str));
					//temp = katoi(str);
					sscanf(str, "%d", &temp);
					data[(count % 4)] = temp;

					/* Over boundary, end to calculate. */
					if ((count / 4) >= frame_len) {
						tp_log_err("count (%d) is larger than frame length, break, frame_len : %d\n", (count/4), frame_len);
						break;
					}
					if ((count % 4) == 3) {
						ptr[y * ilitek_data->x_ch + (count/4)].data = data[0];
						ptr[y * ilitek_data->x_ch + (count/4)].max = data[1];
						ptr[y * ilitek_data->x_ch + (count/4)].min = data[2];
						ptr[y * ilitek_data->x_ch + (count/4)].type = data[3];
					}
					count++;
				}
				record = pKeyValue[j];
				index1 = j + 1;
			}
		}
	//}
}
//EXPORT_SYMBOL(parser_node);

int parser_get_int_data(char *section, char *keyname, char *rv)
{
	int data = 0;
	char value[512] = { 0 };

	if (rv == NULL || section == NULL || keyname == NULL) {
		tp_log_err("Parameters are invalid\n");
		return -EINVAL;
	}

	/* return a white-space string if get nothing */
	if (get_ini_key_value(section, keyname, value) < 0) {
		return ILITEK_FAIL;
	}
	if(strstr(value, "0x") > 0) {
		sscanf(value, "%x", &data);
		tp_log_info("[%s]%s:0x%x\n", section, keyname, data);
	}
	else {
		sscanf(value, "%d", &data);
		tp_log_info("[%s]%s:%d\n", section, keyname, data);
	}
	return data;
}
//EXPORT_SYMBOL(parser_get_int_data);

int GetVerfMapping(uint8_t *tmp) {
    if(strncmp(tmp,"0.3", 3) == 0 || strncmp(tmp," 0.3", 4) == 0) {
        tp_log_info("0.3\n");
        ilitek_data->mp.vref_s = 0xB;
        return 3;
    }
    else if(strncmp(tmp,"0.4", 3) == 0 || strncmp(tmp," 0.4", 4) == 0) {
        tp_log_info("0.4\n");
        ilitek_data->mp.vref_s = 0xC;
        return 4;
    }
    else if(strncmp(tmp,"0.5", 3) == 0 || strncmp(tmp," 0.5", 4) == 0) {
        tp_log_info("0.5\n");
        ilitek_data->mp.vref_s = 0xD;
        return 5;
    }
    else if(strncmp(tmp,"0.6", 3) == 0 || strncmp(tmp," 0.6", 4) == 0) {
        tp_log_info("0.6\n");
        ilitek_data->mp.vref_s = 0xE;
        return 6;
    }
    else if(strncmp(tmp,"0.7", 3) == 0 || strncmp(tmp," 0.7", 4) == 0) {
        tp_log_info("0.7\n");
        ilitek_data->mp.vref_s = 0xF;
        return 7;
    }
    else {
        tp_log_info("0xFF\n");
        ilitek_data->mp.vref_s = 0xB;
        return ILITEK_FAIL;
    }
}

void get_ini_set(void) {
	char data[INI_ROW_LENGTH] = {0};
	char data1[100] = {0};
	//int i = 0;
	int32_t tmp[4];

	if(get_ini_key_value("System", "ProfileVersion", data1) == ILITEK_SUCCESS) {
		sscanf(data1, "%d.%d.%d.%d", &tmp[0], &tmp[1], &tmp[2], &tmp[3]);
		ilitek_data->mp.pf_ver = (tmp[0] << 24) + (tmp[1] << 16) + (tmp[2] << 8) + tmp[3];
		tp_log_info("%d,%d,%d,%d,0x%x\n", tmp[0], tmp[1], tmp[2], tmp[3], ilitek_data->mp.pf_ver);
	}
	else {
		tp_log_info("get profile version fail\n");
	}

	//read open set value
	if(get_ini_key_value("Open_Test", "Enable", data) == ILITEK_SUCCESS) {
		if(strcmp(data, "True") == 0) {
			ilitek_data->mp.open = true;
			ilitek_data->mp.open_min_thr = parser_get_int_data("Open_Test", "Min_Threshold", data);
			if(ilitek_data->mp.pf_ver > INI_PROFILE_V1_0_2_0) {
				ilitek_data->mp.open_tx_avg = parser_get_int_data("Open_Test", "TX_Avg_Delta_Threshold", data);
				if(get_ini_key_value("Open_Test", "TX_Avg_Delta_Threshold_AvoidCorner", data) == ILITEK_SUCCESS) {
					if(strcmp(data, "True") == 0)
					ilitek_data->mp.open_tx_avg_corner = true;
				}
				ilitek_data->mp.open_rx_diff = parser_get_int_data("Open_Test", "RX_Delta_Threshold", data);
				ilitek_data->mp.open_rx_diff_fcnt = parser_get_int_data("Open_Test", "RX_Delta_Threshold_Tolerance", data);
			} 
			else {
				ilitek_data->mp.open_tx_avg = parser_get_int_data("Open_Test", "TX_Average_Diff_Gap", data);
				if(get_ini_key_value("Open_Test", "TX_Average_Diff_Gap_AvoidCorner", data) == ILITEK_SUCCESS) {
					if(strcmp(data, "True") == 0)
					ilitek_data->mp.open_tx_avg_corner = true;
				}
				ilitek_data->mp.open_rx_diff = parser_get_int_data("Open_Test", "RX_Diff_Gap", data);
				ilitek_data->mp.open_rx_diff_fcnt = parser_get_int_data("Open_Test", "RX_Diff_Gap_Tolerance", data);
			}

			tp_log_info("TX_Average_Diff_Gap_AvoidCorner=%s\n", ilitek_data->mp.open_tx_avg_corner ? "True" : "False");
			//tp_log_info("Open_Test Min_Threshold:%d\n", ilitek_data->mp.open_min_thr);
		}
	}
	else
		ilitek_data->mp.open = false;
	tp_log_info("Open_Test Enable:%s\n", ilitek_data->mp.open ? "True" : "False");

	if(get_ini_key_value("MicroOpen_Test", "Enable", data) == ILITEK_SUCCESS) {
		if(strcmp(data, "True") == 0) {
			ilitek_data->mp.m_open = true;
			ilitek_data->mp.m_open_freqH = parser_get_int_data("MicroOpen_Test", "Freq_H", data);
			ilitek_data->mp.m_open_freqL = parser_get_int_data("MicroOpen_Test", "Freq_L", data);
			ilitek_data->mp.m_open_gain = parser_get_int_data("MicroOpen_Test", "Gain", data);
			if(get_ini_key_value("MicroOpen_Test", "RX_Delta_En", data) == ILITEK_SUCCESS) {
				if(strcmp(data, "True") == 0) {
					ilitek_data->mp.m_open_rx_diff = parser_get_int_data("MicroOpen_Test", "RX_Delta_Threshold", data);
					ilitek_data->mp.m_open_rx_diff_fcnt = parser_get_int_data("MicroOpen_Test", "RX_Delta_Threshold_Tolerance", data);
					ilitek_data->mp.m_open_rx = true;
				}
				else
					ilitek_data->mp.m_open_rx = false;
			}
			else
				ilitek_data->mp.m_open_rx = false;
			if(get_ini_key_value("MicroOpen_Test", "TX_Avg_Delta_En", data) == ILITEK_SUCCESS) {
				if(strcmp(data, "True") == 0) {
					ilitek_data->mp.m_open_tx = true;
					ilitek_data->mp.m_open_tx_avg = parser_get_int_data("MicroOpen_Test", "TX_Avg_Delta_Threshold", data);
					if(get_ini_key_value("MicroOpen_Test", "TX_Avg_Delta_Threshold_AvoidCorner", data) == ILITEK_SUCCESS) {
						if(strcmp(data, "True") == 0)
							ilitek_data->mp.m_open_tx_avg_corner = true;
						}
					tp_log_info("TX_Average_Diff_Gap_AvoidCorner=%s\n", ilitek_data->mp.m_open_tx_avg_corner ? "True" : "False");
				}
				else
					ilitek_data->mp.m_open_tx = false;
			}
			else
				ilitek_data->mp.m_open_tx = false;

			get_parser_node(MIRCOOPEN_RX_DELTA, ilitek_data->mp.rxdiff, ilitek_data->x_ch - 1);
		}
		ilitek_data->mp.m_open = false;
	}
	else
		ilitek_data->mp.m_open = false;
	tp_log_info("MicroOpen_Test Enable:%s\n", ilitek_data->mp.m_open ? "True" : "False");

	//read short set value
	if(get_ini_key_value("Short_Test", "Enable", data) == ILITEK_SUCCESS) {
		if(strcmp(data, "True") == 0) {
			ilitek_data->mp.Short = true;
			ilitek_data->mp.short_max_thr = parser_get_int_data("Short_Test", "Max_Threshold", data);
			ilitek_data->mp.dump1 = parser_get_int_data("Short_Test", "Dump1", data);
			ilitek_data->mp.dump2 = parser_get_int_data("Short_Test", "Dump2", data);
			tp_log_info("Short_Test Enable:%s\n", ilitek_data->mp.Short ? "True" : "False");
			tp_log_info("Short_Test Max_Threshold:%d\n", ilitek_data->mp.short_max_thr);
			tp_log_info("Short_Test Dump1:%d\n", ilitek_data->mp.dump1);
			tp_log_info("Short_Test Dump2:%d\n", ilitek_data->mp.dump2);
			get_ini_key_value("Short_Test", "VrefL", data);
			tp_log_info("%s\n", data);
			ilitek_data->mp.vref_v = GetVerfMapping(data);
			ilitek_data->mp.posidleL = parser_get_int_data("Short_Test", "Short_PostIdle_L", data);
			ilitek_data->mp.posidleH = parser_get_int_data("Short_Test", "Short_PostIdle_H", data);
		} else
			ilitek_data->mp.Short = false;
	}
	else
		ilitek_data->mp.Short = false;
	tp_log_info("\n");
	//read Uniformity set value
	if(get_ini_key_value("Uniformity_Test", "Enable", data) == ILITEK_SUCCESS) {
		if(strcmp(data, "True") == 0) {
			ilitek_data->mp.unifo = true;
			if(get_ini_key_value("Uniformity_Test", "Uniformity_RawData_En", data) == ILITEK_SUCCESS) {
				tp_log_info("%s\n", data);
				ilitek_data->mp.uni.max_thr = parser_get_int_data("Uniformity_Test", "Uniformity_RawData_Max_Threshold", data);
				ilitek_data->mp.uni.up_fail = parser_get_int_data("Uniformity_Test", "Uniformity_RawData_Max_Threshold_Tolerance", data);
				ilitek_data->mp.uni.min_thr = parser_get_int_data("Uniformity_Test", "Uniformity_RawData_Min_Threshold", data);
				ilitek_data->mp.uni.low_fail = parser_get_int_data("Uniformity_Test", "Uniformity_RawData_Min_Threshold_Tolerance", data);
				ilitek_data->mp.uni.win1_thr = parser_get_int_data("Uniformity_Test", "Uniformity_Win1_Max_Threshold", data);
				ilitek_data->mp.uni.win1_fail = parser_get_int_data("Uniformity_Test", "Uniformity_Win1_Max_Threshold_Tolerance", data);
				ilitek_data->mp.uni.win2_thr = parser_get_int_data("Uniformity_Test", "Uniformity_Win2_Max_Threshold", data);
				ilitek_data->mp.uni.win2_fail = parser_get_int_data("Uniformity_Test", "Uniformity_Win2_Max_Threshold_Tolerance", data);
				ilitek_data->mp.uni.allnode_raw_section = kmalloc(18, GFP_KERNEL);
				scnprintf(ilitek_data->mp.uni.allnode_raw_section, PAGE_SIZE, "Uniformity_RawData");
				ilitek_data->mp.uni.allnode_win1_section = kmalloc(15, GFP_KERNEL);
				scnprintf(ilitek_data->mp.uni.allnode_win1_section, PAGE_SIZE, "Uniformity_Win1");
				ilitek_data->mp.uni.allnode_win2_section = kmalloc(15, GFP_KERNEL);
				scnprintf(ilitek_data->mp.uni.allnode_win2_section, PAGE_SIZE, "Uniformity_Win2");
			}
			else {
				tp_log_info("%s\n", data);
				ilitek_data->mp.uni.max_thr = parser_get_int_data("Uniformity_Test", "Max_Threshold", data);
				ilitek_data->mp.uni.up_fail = parser_get_int_data("Uniformity_Test", "Up_FailCount", data);
				ilitek_data->mp.uni.min_thr = parser_get_int_data("Uniformity_Test", "Min_Threshold", data);
				ilitek_data->mp.uni.low_fail = parser_get_int_data("Uniformity_Test", "Low_FailCount", data);
				ilitek_data->mp.uni.win1_fail = parser_get_int_data("Uniformity_Test", "Win1_FailCount", data);
				ilitek_data->mp.uni.win1_thr = parser_get_int_data("Uniformity_Test", "Win1_Threshold", data);
				ilitek_data->mp.uni.win2_fail = parser_get_int_data("Uniformity_Test", "Win2_FailCount", data);
				ilitek_data->mp.uni.win2_thr = parser_get_int_data("Uniformity_Test", "Win2_Threshold", data);
				ilitek_data->mp.uni.allnode_raw_section = kmalloc(18, GFP_KERNEL);
				scnprintf(ilitek_data->mp.uni.allnode_raw_section, PAGE_SIZE, "AllNode_RawData_En");
				ilitek_data->mp.uni.allnode_win1_section = kmalloc(15, GFP_KERNEL);
				scnprintf(ilitek_data->mp.uni.allnode_win1_section, PAGE_SIZE, "AllNode_Win1_En");
				ilitek_data->mp.uni.allnode_win2_section = kmalloc(15, GFP_KERNEL);
				scnprintf(ilitek_data->mp.uni.allnode_win2_section, PAGE_SIZE, "AllNode_Win2_En");
			}
			tp_log_info("Uniformity_Test Enable:%s\n", ilitek_data->mp.unifo ? "True" : "False");
			tp_log_info("Uniformity_Test Max_Threshold:%d\n", ilitek_data->mp.uni.max_thr);
			tp_log_info("Uniformity_Test Min_Threshold:%d\n", ilitek_data->mp.uni.up_fail);
			tp_log_info("Uniformity_Test Min_Threshold:%d\n", ilitek_data->mp.uni.min_thr);
			tp_log_info("Uniformity_Test Low_FailCount:%d\n", ilitek_data->mp.uni.low_fail);
			tp_log_info("Uniformity_Test Win1_FailCount:%d\n", ilitek_data->mp.uni.win1_fail);
			tp_log_info("Uniformity_Test Win1_Threshold:%d\n", ilitek_data->mp.uni.win1_thr);
			tp_log_info("Uniformity_Test Win2_FailCount:%d\n", ilitek_data->mp.uni.win2_fail);
			tp_log_info("Uniformity_Test Win2_Threshold:%d\n", ilitek_data->mp.uni.win2_thr);
            //print INI file benchmark
			if(get_ini_key_value("Uniformity_Test", "Benchmark_Enable", data) == ILITEK_SUCCESS) {
				if(strcmp(data, "True") == 0) {
					ilitek_data->mp.uni.bench = true;
					get_parser_node(BENCHMARK_KEY_NAME, ilitek_data->mp.uni.ben, ilitek_data->x_ch);
				}
				else
					ilitek_data->mp.uni.bench = false;
			}
            printk("\n");
            //print INI file allnode raw
			if(get_ini_key_value("Uniformity_Test", ilitek_data->mp.uni.allnode_raw_section, data) == ILITEK_SUCCESS) {
				if(strcmp(data, "True") == 0){
					ilitek_data->mp.uni.allnode_raw = true;
					get_parser_node(ALLNODE_RAWDATA_KEY_NAME, ilitek_data->mp.uni.raw, ilitek_data->x_ch);
				}
				else
					ilitek_data->mp.uni.allnode_raw = false;
			}
            printk("\n");
            //print INI file allnode win1
			if(get_ini_key_value("Uniformity_Test", ilitek_data->mp.uni.allnode_win1_section, data) == ILITEK_SUCCESS) {
				if(strcmp(data, "True") == 0) {
					ilitek_data->mp.uni.allnode_win1 = true;
					get_parser_node(ALLNODE_WIN1_KEY_NAME, ilitek_data->mp.uni.win1, ilitek_data->x_ch);
				}
				else
					ilitek_data->mp.uni.allnode_win1 = false;
			}
			tp_log_info("ilitek_data->mp.uni.allnode_win1_section = %s\n", ilitek_data->mp.uni.allnode_win1_section);
            printk("\n");
            //print INI file allnode win2
			if(get_ini_key_value("Uniformity_Test", ilitek_data->mp.uni.allnode_win2_section, data) == ILITEK_SUCCESS) {
				if(strcmp(data, "True") == 0) {
					ilitek_data->mp.uni.allnode_win2 = true;
					get_parser_node(ALLNODE_WIN2_KEY_NAME, ilitek_data->mp.uni.win2, ilitek_data->x_ch - 1);
				}
				else
					ilitek_data->mp.uni.allnode_win2 = false;
			}
            printk("\n");
			tp_log_info("ilitek_data->mp.uni.allnode_win2_section = %s\n", ilitek_data->mp.uni.allnode_win2_section);
		}
		else
			ilitek_data->mp.unifo = false;
	}
	else
		ilitek_data->mp.unifo = false;
	tp_log_info("\n");
}

int parser_path(char *path)
{
	int ret = 0, fsize = 0, i = 0;
	char *tmp = NULL;
	struct file *f = NULL;
	struct inode *inode;
	mm_segment_t old_fs;
	loff_t pos = 0;

	tp_log_info("path = %s\n", path);
	f = filp_open(path, O_RDONLY, 0);
	if (ERR_ALLOC_MEM(f)) {
		tp_log_err("Failed to open the file at %ld.\n", PTR_ERR(f));
		ret = -ENOENT;
		return ret;
	}

	ilitek_ini_file_data = (struct ini_file_data *)vmalloc(sizeof(struct ini_file_data) * PARSER_MAX_KEY_NUM);
	if (ERR_ALLOC_MEM(ilitek_ini_file_data)) {
		tp_log_info("Failed to malloc ilitek_ini_file_data\n");
		ret = -EADDRNOTAVAIL;
		goto array_fail;
	}

#if KERNEL_VERSION(3, 18, 0) >= LINUX_VERSION_CODE
	inode = f->f_dentry->d_inode;
#else
	inode = f->f_path.dentry->d_inode;
#endif

	fsize = inode->i_size;
	tp_log_info("fsize = %d\n", fsize);
	if (fsize <= 0) {
		tp_log_err("The size of file is invaild\n");
		ret = -EINVAL;
		goto out;
	}
	tmp = vmalloc(fsize+1);
	if (ERR_ALLOC_MEM(tmp)) {
		tp_log_err("Failed to allocate tmp memory, %ld\n", PTR_ERR(tmp));
		ret = -ENOMEM;
		goto out;
	}
	/* ready to map user's memory to obtain data by reading files */
	old_fs = get_fs();
	set_fs(get_ds());
	vfs_read(f, tmp, fsize, &pos);
	set_fs(old_fs);
	tmp[fsize] = 0x00;
	init_ilitek_ini_data();
	/* change all characters to lower case */
	// for (i = 0; i < strlen(tmp); i++) {
	// 	tmp[i] = tolower(tmp[i]);
	// 	tp_log_info("%s\n", tmp[i]);
	// }
	//file is UTF-8 have bom
	if(tmp[0] == 0xef && tmp[1] == 0xbb && tmp[2] == 0xbf) {
		tp_log_info("UTF-8 have Bom\n");
		for (i = 0; i < 10; i++)
			printk("%c,", tmp[i]);
		printk("\n");
		for (i = 0; i < strlen(tmp) - 1; i++)
			tmp[i] = tmp[i+3];
		fsize -= 3;
	}
	else
		tp_log_info("UTF-8 no have Bom\n");
	ret = get_ini_phy_data(tmp, fsize);
	if (ret < 0) {
		tp_log_err("Failed to get physical ini data, ret = %d\n", ret);
		goto out;
	}
	get_ini_set();
out:
	ipio_vfree((void **)&tmp);
	filp_close(f, NULL);

array_fail:
	ipio_vfree((void **)&ilitek_ini_file_data);
	return ret;
}
//EXPORT_SYMBOL(parser_path);

void ilitek_sensortest_init(void)
{
	short_test_result = 0;
	open_test_result = 0;
	allnode_test_result = 0;
	ilitek_short_threshold = 7;
	ilitek_open_threshold = 0;	//200;
	ilitek_allnode_max_threshold = 8500;
	ilitek_allnode_min_threshold = 0;	//1500;
	ilitek_open_txdeltathrehold = 120;
	ilitek_open_rxdeltathrehold = 120;
	ilitek_allnodetestw1 = 140;
	ilitek_allnodetestw2 = 140;
	ilitek_allnodemodule = 20;
	ilitek_win1_failpoint = 40;
	ilitek_win2_failpoint = 40;
    ilitek_Dump1_Value = 0x25;
    ilitek_Dump2_Value = 0x15;
	ilitek_allnodetx = 3;
	ilitek_printsensortestdata = 1;
	noisefre_start = 30;
	noisefre_end = 120;
	noisefre_step = 5;
}
void save_short_v6_csv(struct file *filp, int32_t *short_xdata1, int32_t *short_ydata1) {
    int32_t j = 0, len = 0;
    uint8_t buf[128];   
    uint32_t impedance = 0;

    if(short_test_result == 0)
        len = scnprintf(buf, PAGE_SIZE, "[Short_Test]                           ,OK ,\n\n,(Spec.)\n");
    else
        len = scnprintf(buf, PAGE_SIZE, "[Short_Test]                           ,NG ,\n\n,(Spec.)\n");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    len = scnprintf(buf, PAGE_SIZE, "Frame Count,1\n");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    len = scnprintf(buf, PAGE_SIZE, "Max_Threshold,%d\n\n", ilitek_short_threshold);
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));

    len = scnprintf(buf, PAGE_SIZE, "      Normal        ,");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for (j = 0; j < ilitek_data->x_ch; j++) {
        len = scnprintf(buf, PAGE_SIZE, "(X_%3d),", j);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    }
    len = scnprintf(buf, PAGE_SIZE, "\n       X_SLK        ,");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for (j = 0; j < ilitek_data->x_ch; j++) {
        len = scnprintf(buf, PAGE_SIZE, "%7d,", short_xdata1[j]);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    }
    len = scnprintf(buf, PAGE_SIZE, "\n      X_Resistance  ,");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for (j = 0; j < ilitek_data->x_ch; j++) {
        if(short_xdata1[j] < DIVIDE_10M)
            len = scnprintf(buf, PAGE_SIZE, " 10.00M,");
        else {
            //IMPEDANCE_MACRO(fout, dump1, dump2) ((2140810 * abs(dump2-dump1)) /(216 * fout)) 
            impedance = IMPEDANCE_MACRO(short_xdata1[j], ilitek_data->mp.dump1, ilitek_data->mp.dump2);
            len = scnprintf(buf, PAGE_SIZE, "%5d.%02dM,", impedance/100, impedance%100);
        }

        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    }
    len = scnprintf(buf, PAGE_SIZE, "\n                    ,");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for (j = 0; j < ilitek_data->y_ch; j++) {
        len = scnprintf(buf, PAGE_SIZE, "(Y_%3d),", j);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    }
    len = scnprintf(buf, PAGE_SIZE, "\n       Y_SLK        ,");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for (j = 0; j < ilitek_data->y_ch; j++) {
        len = scnprintf(buf, PAGE_SIZE, "%7d,", short_ydata1[j]);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    }
    len = scnprintf(buf, PAGE_SIZE, "\n      Y_Resistance  ,");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for (j = 0; j < ilitek_data->y_ch; j++) {
        if(short_ydata1[j] < DIVIDE_10M)
            len = scnprintf(buf, PAGE_SIZE, " 10.00M,");
        else {
            impedance = IMPEDANCE_MACRO(short_ydata1[j], ilitek_data->mp.dump1, ilitek_data->mp.dump2);
            len = scnprintf(buf, PAGE_SIZE, "%5d.%02dM,", impedance/100, impedance%100);            
        }
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    }
    len = scnprintf(buf, PAGE_SIZE, "\n\n");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
}

void save_open_v6_csv(struct file *filp, int32_t *open_data, int32_t *tx_avg, int32_t *rx_diff) {
    int32_t j = 0, len = 0, i = 0;
    uint8_t buf[128];

    if(open_test_result == 0)
        len = scnprintf(buf, PAGE_SIZE, "[Open_Test]                            ,OK ,\n\n      ,(Spec.),\n");
    else
        len = scnprintf(buf, PAGE_SIZE, "[Open_Test]                            ,NG\n\n      ,(Spec.),\n");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    len = scnprintf(buf, PAGE_SIZE, "      Frame_Count                ,1\n");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    len = scnprintf(buf, PAGE_SIZE, "      Min_Threshold              ,%d\n", ilitek_open_threshold);
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    len = scnprintf(buf, PAGE_SIZE, "      TX_Avg_Delta_Threshold     ,%d\n", ilitek_data->mp.open_tx_avg);
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    len = scnprintf(buf, PAGE_SIZE, "      RX_Delta_Threshold         ,%d\n", ilitek_data->mp.open_rx_diff);
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    len = scnprintf(buf, PAGE_SIZE, "      RX_Delta_Threshold_Tolerance,%d\n", ilitek_data->mp.open_rx_diff_fcnt);
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));

    len = scnprintf(buf, PAGE_SIZE, "\n      Normal        ,");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for(j = 0; j < ilitek_data->x_ch; j++)
    {
        len = scnprintf(buf, PAGE_SIZE, "(X_%3d),", j);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    }
    len = scnprintf(buf, PAGE_SIZE, "(Avg)  ,\n");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for (j = 0; j < ilitek_data->y_ch * ilitek_data->x_ch; j++) {
        if(j % ilitek_data->x_ch == 0)
        {
            len = scnprintf(buf, PAGE_SIZE, "      Y_%03d         ,", j / ilitek_data->x_ch);
            vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        }
        if(open_data[j] < ilitek_open_threshold)
            len = scnprintf(buf, PAGE_SIZE, "*%6d,", open_data[j]);
        else
            len = scnprintf(buf, PAGE_SIZE, "%7d,", open_data[j]);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		//avg data
        if ((j + 1) % ilitek_data->x_ch == 0) {
			if(j < ilitek_data->y_ch * ilitek_data->x_ch) {
				if(ilitek_data->mp.txavg[i].max_st)
					len = scnprintf(buf, PAGE_SIZE, "%7d,", tx_avg[i]);
				else {
					len = scnprintf(buf, PAGE_SIZE, "*%6d,", tx_avg[i]);
				}
				i++;
				vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
			}
            len = scnprintf(buf, PAGE_SIZE, "\n");
            vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        }
    }
	if (ilitek_data->mp.open_rx_diff > 0) {
		len = scnprintf(buf, PAGE_SIZE, "\n      X_Delta       ,");
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		for(j = 0; j < ilitek_data->x_ch - 1; j++)
		{
			len = scnprintf(buf, PAGE_SIZE, "(X_%3d),", j);
			vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		}
		for (j = 0; j < ilitek_data->y_ch * (ilitek_data->x_ch - 1); j++) {
			if(j  % (ilitek_data->x_ch - 1) == 0 )
			{
				len = scnprintf(buf, PAGE_SIZE, "\n      Y_%03d         ,", j / ilitek_data->x_ch);
				vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
			}
			if(ilitek_data->mp.rxdiff[j].max_st) {
				len = scnprintf(buf, PAGE_SIZE, "%7d,", rx_diff[j]);
			}
			else {
				len = scnprintf(buf, PAGE_SIZE, "*%6d,", rx_diff[j]);
			}
			vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		}
		len = scnprintf(buf, PAGE_SIZE, "\n");
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));	
	}
}

void save_mopen_v6_csv(struct file *filp, int32_t *open_data, int32_t *tx_avg, int32_t *rx_diff) {
    int32_t j = 0, len = 0, i = 0;
    uint8_t buf[128];

    if(open_test_result == 0)
        len = scnprintf(buf, PAGE_SIZE, "[MicroOpen_Test]                            ,OK ,\n\n      ,(Spec.),\n");
    else
        len = scnprintf(buf, PAGE_SIZE, "[MicroOpen_Test]                            ,NG\n\n      ,(Spec.),\n");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    len = scnprintf(buf, PAGE_SIZE, "      Frame_Count                ,1\n");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    len = scnprintf(buf, PAGE_SIZE, "      TX_Avg_Delta_Threshold     ,%d\n", ilitek_data->mp.m_open_tx_avg);
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    len = scnprintf(buf, PAGE_SIZE, "      RX_Delta_Threshold         ,%d\n", ilitek_data->mp.m_open_rx_diff);
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    len = scnprintf(buf, PAGE_SIZE, "      RX_Delta_Threshold_Tolerance,%d\n", ilitek_data->mp.m_open_rx_diff_fcnt);
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));

    len = scnprintf(buf, PAGE_SIZE, "\n      Normal        ,");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for(j = 0; j < ilitek_data->x_ch; j++)
    {
        len = scnprintf(buf, PAGE_SIZE, "(X_%3d),", j);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    }
    len = scnprintf(buf, PAGE_SIZE, "(Avg)  ,\n");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for (j = 0; j < ilitek_data->y_ch * ilitek_data->x_ch; j++) {
        if(j % ilitek_data->x_ch == 0)
        {
            len = scnprintf(buf, PAGE_SIZE, "      Y_%03d         ,", j / ilitek_data->x_ch);
            vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		}
        len = scnprintf(buf, PAGE_SIZE, "%7d,", open_data[j]);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		//avg data
        if ((j + 1) % ilitek_data->x_ch == 0) {
			if(j < ilitek_data->y_ch * ilitek_data->x_ch) {
				len = scnprintf(buf, PAGE_SIZE, "%7d,", tx_avg[i]);
				i++;
				vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
			}
            len = scnprintf(buf, PAGE_SIZE, "\n");
            vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        }
    }

    len = scnprintf(buf, PAGE_SIZE, "\n      RX_Delta      ,");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for(j = 0; j < ilitek_data->x_ch - 1; j++)
    {
        len = scnprintf(buf, PAGE_SIZE, "(X_%3d),", j);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    }
    for (j = 0; j < ilitek_data->y_ch * (ilitek_data->x_ch - 1); j++) {
        if(j  % (ilitek_data->x_ch - 1) == 0 )
        {
            len = scnprintf(buf, PAGE_SIZE, "\n      Y_%03d         ,", j / ilitek_data->x_ch);
            vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        }
        if(rx_diff[j] > ilitek_data->mp.rxdiff[j].max_st) {
			len = scnprintf(buf, PAGE_SIZE, "%7d,", rx_diff[j]);
		}
        else {
			len = scnprintf(buf, PAGE_SIZE, "*%6d,", rx_diff[j]);
		}
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    }
	len = scnprintf(buf, PAGE_SIZE, "\n");
	vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));

    len = scnprintf(buf, PAGE_SIZE, "\n      TX_Avg_Delta  ,(Avg_D),");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for (j = 0; j < ilitek_data->y_ch - 1 ; j++) {
		len = scnprintf(buf, PAGE_SIZE, "      D_%2d          ,", j + 1);
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        if(ilitek_data->mp.txavg[j].max_st) {
			len = scnprintf(buf, PAGE_SIZE, "%7d,\n", ilitek_data->mp.txavg[j].data);
		}
        else {
			len = scnprintf(buf, PAGE_SIZE, "*%6d,\n", ilitek_data->mp.txavg[j].data);
		}
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    }
	len = scnprintf(buf, PAGE_SIZE, "\n");
	vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));	
}

void save_uniformity_v6_csv(struct file *filp, int32_t *allnode_data, int32_t *allnode_win1, int32_t *allnode_win2) {
    int32_t i,j = 0, len = 0;
    uint8_t buf[128];

    if(allnode_test_result == 0)
        len = scnprintf(buf, PAGE_SIZE, "[Uniformity_Test]                      ,OK ,\n\n      ,(Spec.),\n");
    else
        len = scnprintf(buf, PAGE_SIZE, "[Uniformity_Test]                      ,NG ,\n\n      ,(Spec.),\n");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));

    len = scnprintf(buf, PAGE_SIZE, "   Min_Threshold           ,%d\n", ilitek_allnode_min_threshold);
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    len = scnprintf(buf, PAGE_SIZE, "   Max_Threshold           ,%d\n", ilitek_allnode_max_threshold);
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    len = scnprintf(buf, PAGE_SIZE, "   Win1_Threshold          ,%d\n", ilitek_allnodetestw1);
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    len = scnprintf(buf, PAGE_SIZE, "   Win1_FailCount          ,%d\n", ilitek_win1_failpoint);
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    len = scnprintf(buf, PAGE_SIZE, "   Win2_Threshold          ,%d\n", ilitek_allnodetestw2);
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    len = scnprintf(buf, PAGE_SIZE, "   Win2_FailCount          ,%d\n", ilitek_win2_failpoint);
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));

    len = scnprintf(buf, PAGE_SIZE, "\n            Limit   ,");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for(j = 0; j < ilitek_data->x_ch; j++)
    {
        len = scnprintf(buf, PAGE_SIZE, "(X_%3d),", j);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    }
    len = scnprintf(buf, PAGE_SIZE, "\n");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for (j = 0; j < ilitek_data->y_ch * ilitek_data->x_ch; j++) {
        if(j % ilitek_data->x_ch == 0)
        {
            len = scnprintf(buf, PAGE_SIZE, "      Y_%3d         ,", j / ilitek_data->x_ch);
            vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        }
        if(allnode_data[j] > ilitek_allnode_max_threshold || allnode_data[j] < ilitek_allnode_min_threshold)
            len = scnprintf(buf, PAGE_SIZE, "*%6d,", allnode_data[j]);
        else
            len = scnprintf(buf, PAGE_SIZE, "%7d,", allnode_data[j]);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        if ((j + 1) % ilitek_data->x_ch == 0) {
            len = scnprintf(buf, PAGE_SIZE, "\n");
            vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        }
    }

    len = scnprintf(buf, PAGE_SIZE, "\n      Win1          ,");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for(j = 0; j < ilitek_data->x_ch; j++)
    {
        len = scnprintf(buf, PAGE_SIZE, "(X_%3d),", j);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    }
    len = scnprintf(buf, PAGE_SIZE, "\n");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for(j = 0; j < ilitek_data->y_ch -1; j++)
    {
        len = scnprintf(buf, PAGE_SIZE, "      Y_%03d         ,", j);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        for(i = 0; i < ilitek_data->x_ch; i++)
        {
            if(allnode_win1[j * ilitek_data->x_ch + i] > ilitek_allnodetestw1)
                len = scnprintf(buf, PAGE_SIZE, "*%6d,", allnode_win1[j * ilitek_data->x_ch + i]);
            else
                len = scnprintf(buf, PAGE_SIZE, "%7d,", allnode_win1[j * ilitek_data->x_ch + i]);
            vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        }
        len = scnprintf(buf, PAGE_SIZE, "\n");
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    }
    
    len = scnprintf(buf, PAGE_SIZE, "\n      Win2          ,");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for(j = 0; j < ilitek_data->x_ch -1; j++)
    {
        len = scnprintf(buf, PAGE_SIZE, "(X_%3d),", j);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    }
    len = scnprintf(buf, PAGE_SIZE, "\n");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for(j = 0; j < ilitek_data->y_ch - 1; j++)
    {
        len = scnprintf(buf, PAGE_SIZE, "      Y_%03d         ,", j);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        for(i = 0; i < ilitek_data->x_ch -1; i++)
        {
            if( allnode_win2[j * (ilitek_data->x_ch - 1) + i] > ilitek_allnodetestw2)
                len = scnprintf(buf, PAGE_SIZE, "*%6d,", allnode_win2[j * (ilitek_data->x_ch - 1) + i]);
            else
                len = scnprintf(buf, PAGE_SIZE, "%7d,", allnode_win2[j * (ilitek_data->x_ch - 1) + i]);
            vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        }
        len = scnprintf(buf, PAGE_SIZE, "\n");
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    }	
}

void save_benchmark_v6_csv(struct file *filp, int32_t *allnode_data) {
    int32_t j = 0, len = 0;
    uint8_t buf[128];

    tp_log_info("max:%d min:%d\n", ilitek_data->mp.uni.ben[0].max_st, ilitek_data->mp.uni.ben[0].min_st);
    len = scnprintf(buf, PAGE_SIZE, "\n      Benchmark     ,");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for(j = 0; j < ilitek_data->x_ch; j++)
    {
        len = scnprintf(buf, PAGE_SIZE, "(X_%3d),", j);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    }
    len = scnprintf(buf, PAGE_SIZE, "\n");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for (j = 0; j < ilitek_data->y_ch * ilitek_data->x_ch; j++) {
        if(j % ilitek_data->x_ch == 0)
        {
            len = scnprintf(buf, PAGE_SIZE, "      Y_%3d         ,", j / ilitek_data->x_ch);
            vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        }
        if(ilitek_data->mp.uni.ben[j].max_st == false || ilitek_data->mp.uni.ben[j].min_st == false)
            len = scnprintf(buf, PAGE_SIZE, "*%6d,", allnode_data[j]);
        else
            len = scnprintf(buf, PAGE_SIZE, "%7d,", allnode_data[j]);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        if ((j + 1) % ilitek_data->x_ch == 0) {
            len = scnprintf(buf, PAGE_SIZE, "\n");
            vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        }
    }
}

void save_allnode_raw_v6_csv(struct file *filp, int32_t *allnode_data) {
    int32_t j = 0, len = 0;
    uint8_t buf[128];

    len = scnprintf(buf, PAGE_SIZE, "\n      ANode_Raw_Up  ,");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for(j = 0; j < ilitek_data->x_ch; j++)
    {
        len = scnprintf(buf, PAGE_SIZE, "(X_%3d),", j);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    }
    len = scnprintf(buf, PAGE_SIZE, "\n");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for (j = 0; j < ilitek_data->y_ch * ilitek_data->x_ch; j++) {
        if(j % ilitek_data->x_ch == 0)
        {
            len = scnprintf(buf, PAGE_SIZE, "      Y_%3d         ,", j / ilitek_data->x_ch);
            vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        }
        if(ilitek_data->mp.uni.raw[j].max_st == false)
            len = scnprintf(buf, PAGE_SIZE, "*%6d,", allnode_data[j]);
        else
            len = scnprintf(buf, PAGE_SIZE, "%7d,", allnode_data[j]);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        if ((j + 1) % ilitek_data->x_ch == 0) {
            len = scnprintf(buf, PAGE_SIZE, "\n");
            vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        }
    }
    
    len = scnprintf(buf, PAGE_SIZE, "\n      ANode_Raw_Low ,");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for(j = 0; j < ilitek_data->x_ch; j++)
    {
        len = scnprintf(buf, PAGE_SIZE, "(X_%3d),", j);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    }
    len = scnprintf(buf, PAGE_SIZE, "\n");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for (j = 0; j < ilitek_data->y_ch * ilitek_data->x_ch; j++) {
        if(j % ilitek_data->x_ch == 0)
        {
            len = scnprintf(buf, PAGE_SIZE, "      Y_%3d         ,", j / ilitek_data->x_ch);
            vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        }
        if(ilitek_data->mp.uni.raw[j].min_st == false)
            len = scnprintf(buf, PAGE_SIZE, "*%6d,", allnode_data[j]);
        else
            len = scnprintf(buf, PAGE_SIZE, "%7d,", allnode_data[j]);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        if ((j + 1) % ilitek_data->x_ch == 0) {
            len = scnprintf(buf, PAGE_SIZE, "\n");
            vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        }
    }
}

void save_allnode_win1_v6_csv(struct file *filp, int32_t *allnode_win1) {
    int32_t j = 0, len = 0;
    uint8_t buf[128];

    len = scnprintf(buf, PAGE_SIZE, "\n      ANode_Win1_Up ,");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for(j = 0; j < ilitek_data->x_ch; j++)
    {
        len = scnprintf(buf, PAGE_SIZE, "(X_%3d),", j);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    }
    len = scnprintf(buf, PAGE_SIZE, "\n");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for (j = 0; j < (ilitek_data->y_ch - 1) * ilitek_data->x_ch; j++) {
        if(j % ilitek_data->x_ch == 0)
        {
            len = scnprintf(buf, PAGE_SIZE, "      Y_%3d         ,", j / ilitek_data->x_ch);
            vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        }
        if(ilitek_data->mp.uni.win1[j].max_st == false)
            len = scnprintf(buf, PAGE_SIZE, "*%6d,", allnode_win1[j]);
        else
            len = scnprintf(buf, PAGE_SIZE, "%7d,", allnode_win1[j]);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        if ((j + 1) % ilitek_data->x_ch == 0) {
            len = scnprintf(buf, PAGE_SIZE, "\n");
            vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        }
    }
    len = scnprintf(buf, PAGE_SIZE, "\n      ANode_Win1_Low,");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for(j = 0; j < ilitek_data->x_ch; j++)
    {
        len = scnprintf(buf, PAGE_SIZE, "(X_%3d),", j);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    }
    len = scnprintf(buf, PAGE_SIZE, "\n");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for (j = 0; j < (ilitek_data->y_ch - 1) * ilitek_data->x_ch; j++) {
        if(j % ilitek_data->x_ch == 0)
        {
            len = scnprintf(buf, PAGE_SIZE, "      Y_%3d         ,", j / ilitek_data->x_ch);
            vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        }
        if(ilitek_data->mp.uni.win1[j].min_st == false)
            len = scnprintf(buf, PAGE_SIZE, "*%6d,", allnode_win1[j]);
        else
            len = scnprintf(buf, PAGE_SIZE, "%7d,", allnode_win1[j]);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        if ((j + 1) % ilitek_data->x_ch == 0) {
            len = scnprintf(buf, PAGE_SIZE, "\n");
            vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        }
    }
}

void save_allnode_win2_v6_csv(struct file *filp, int32_t *allnode_win2) {
    int32_t j = 0, len = 0;
    uint8_t buf[128];

    len = scnprintf(buf, PAGE_SIZE, "\n      ANode_Win2_Up ,");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for(j = 0; j < (ilitek_data->x_ch - 1); j++)
    {
        len = scnprintf(buf, PAGE_SIZE, "(X_%3d),", j);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    }
    len = scnprintf(buf, PAGE_SIZE, "\n");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for (j = 0; j < (ilitek_data->y_ch - 1) * (ilitek_data->x_ch - 1); j++) {
        if(j % (ilitek_data->x_ch - 1) == 0)
        {
            len = scnprintf(buf, PAGE_SIZE, "      Y_%3d         ,", j / (ilitek_data->x_ch - 1));
            vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        }
        if(ilitek_data->mp.uni.win2[j].max_st == false)
            len = scnprintf(buf, PAGE_SIZE, "*%6d,", allnode_win2[j]);
        else
            len = scnprintf(buf, PAGE_SIZE, "%7d,", allnode_win2[j]);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        if ((j + 1) % (ilitek_data->x_ch - 1) == 0) {
            len = scnprintf(buf, PAGE_SIZE, "\n");
            vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        }
    }
    len = scnprintf(buf, PAGE_SIZE, "\n      ANode_Win2_Low,");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for(j = 0; j < ilitek_data->x_ch - 1; j++)
    {
        len = scnprintf(buf, PAGE_SIZE, "(X_%3d),", j);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    }
    len = scnprintf(buf, PAGE_SIZE, "\n");
    vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
    for (j = 0; j < (ilitek_data->y_ch - 1) * (ilitek_data->x_ch - 1); j++) {
        if(j % (ilitek_data->x_ch - 1) == 0)
        {
            len = scnprintf(buf, PAGE_SIZE, "      Y_%3d         ,", j / (ilitek_data->x_ch - 1));
            vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        }
        if(ilitek_data->mp.uni.win2[j].min_st == false)
            len = scnprintf(buf, PAGE_SIZE, "*%6d,", allnode_win2[j]);
        else
            len = scnprintf(buf, PAGE_SIZE, "%7d,", allnode_win2[j]);
        vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        if ((j + 1) % (ilitek_data->x_ch - 1) == 0) {
            len = scnprintf(buf, PAGE_SIZE, "\n");
            vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
        }
    }    
}

void ilitek_printf_sensortest_data_v6(int32_t *short_xdata1, int32_t *short_ydata1, int32_t *open_data, int32_t *open_tx_avg, 
						int32_t *open_rx_diff, int32_t *allnode_data, int32_t *allnode_win1, int32_t *allnode_win2, struct seq_file *m)
{
	int32_t j = 0, len = 0, i = 0;
	struct file *filp;
	mm_segment_t fs;
	uint8_t buf[128];
	struct timeval time_now;
	struct rtc_time tm;

	do_gettimeofday(&time_now);
	rtc_time_to_tm(time_now.tv_sec, &tm);
	tp_log_info("%d_%d_%d_%d_%d_%d\n", (tm.tm_year + 1900), tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    tp_log_info("short=%d,open=%d,allnode=%d\n", short_test_result, open_test_result, allnode_test_result);
	if (short_test_result == 0 && open_test_result == 0 && allnode_test_result == 0) {
		len =
		    scnprintf(buf, PAGE_SIZE, "ilitek_sensortest_%d%02d%02d%02d%02d%02d_pass.csv", (tm.tm_year + 1900), tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min,
			    tm.tm_sec);
	} else {
		len =
		    scnprintf(buf, PAGE_SIZE, "ilitek_sensortest_%d%02d%02d%02d%02d%02d_fail.csv", (tm.tm_year + 1900), tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min,
			    tm.tm_sec);
	}
	for (j = 0; j < 256; j++) {
		sensor_test_data_path_tmp[j] = sensor_test_data_path[j];
	}
	strcat(sensor_test_data_path, buf);
	tp_log_info("sensor_test_data_path = %s\n", sensor_test_data_path);
	filp = filp_open(sensor_test_data_path, O_CREAT | O_WRONLY, 0777);
	if (IS_ERR(filp)) {
		tp_log_err("save sensor test  File Open Error path = %s\n", sensor_test_data_path);
	} else {
		fs = get_fs();
		set_fs(KERNEL_DS);

		len = scnprintf(buf, PAGE_SIZE, "ILITEK LINUX DRIVER V%d.%d.%d.%d\n\n",DERVER_VERSION_MAJOR,DERVER_VERSION_MINOR,CUSTOMER_ID,MODULE_ID);
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));

		if(ilitek_data->mp.Short) {
            save_short_v6_csv(filp, short_xdata1, short_ydata1);
		}

		if(ilitek_data->mp.open) {
            save_open_v6_csv(filp, open_data, open_tx_avg, open_rx_diff);
		}
		if(ilitek_data->mp.m_open) {
            save_mopen_v6_csv(filp, open_data, open_tx_avg, open_rx_diff);
		}		
		if(ilitek_data->mp.unifo) {
            save_uniformity_v6_csv(filp, allnode_data, allnode_win1, allnode_win2);	
            if(ilitek_data->mp.uni.bench) {
                save_benchmark_v6_csv(filp, allnode_data);
            }
            if(ilitek_data->mp.uni.allnode_raw) {
                save_allnode_raw_v6_csv(filp, allnode_data);
            }

            if(ilitek_data->mp.uni.allnode_win1) {
                save_allnode_win1_v6_csv(filp, allnode_win1);
            }

            if(ilitek_data->mp.uni.allnode_win2) {
                save_allnode_win2_v6_csv(filp, allnode_win2);
            }
		}

		set_fs(fs);
	}
	for (j = 0; j < 256; j++) {
		sensor_test_data_path[j] = sensor_test_data_path_tmp[j];
	}

	if (ilitek_printsensortestdata) {
		if (short_test_result == 0 && open_test_result == 0 && allnode_test_result == 0) {
			printk("pass\n");
		} else {
			printk("fail\n");
		}

		printk("%s", "\n");
		printk("%s", "short\n");
		printk("short_threshold test = %d\n", ilitek_short_threshold);

		
		printk("\nx_ch data\n");

		for(j = 0; j < ilitek_data->x_ch; j++)
		{
			printk("ch%02d,",j);
		}
		printk("\n");
		for (j = 0; j < ilitek_data->x_ch; j++) {
			printk("%4d,", short_xdata1[j]);
		}
		printk("%s", "\n");
		printk("\ny_ch data\n");
		for(j = 0; j < ilitek_data->y_ch; j++)
		{
			printk("ch%02d,",j);
		}
		printk("\n");
		for (j = 0; j < ilitek_data->y_ch; j++) {
			printk("%4d,", short_ydata1[j]);
		}
		printk("%s", "\n");
		printk("%s", "\n");
		printk("%s", "open:\n");
		printk("Min_Threshold = %d\n", ilitek_data->mp.open_min_thr);
		printk("TX_Avg_Delta_Threshold = %d\n", ilitek_data->mp.open_tx_avg);
		printk("RX_Delta_Threshold = %d\n", ilitek_data->mp.open_rx_diff);
		printk("RX_Diff_Gap_Tolerance = %d\n", ilitek_data->mp.open_rx_diff_fcnt);
		printk("\nopen data\n");
		for(j = 0; j < ilitek_data->x_ch; j++)
		{
			printk("ch%02d,",j);
		}
		printk("(Avg)  ,\n");
		for (j = 0; j < ilitek_data->y_ch * ilitek_data->x_ch; j++) {
			printk("%4d,", open_data[j]);
			if ((j + 1) % ilitek_data->x_ch == 0) {
				printk("%d,\n", open_tx_avg[i]);
				i++;
		 	}
		}
		printk("%s", "\n");
		printk("%s", "allnode:\n");
		printk("allnode_max_threshold = %d\n", ilitek_allnode_max_threshold);
		printk("allnode_min_threshold = %d\n", ilitek_allnode_min_threshold);
		printk("allnodetestw1 = %d\n", ilitek_allnodetestw1);
		printk("allnodetestw2 = %d\n", ilitek_allnodetestw2);
		printk("allnodemodule = %d\n", ilitek_allnodemodule);
		printk("allnodetx = %d\n", ilitek_allnodetx);

		printk("\nallnode data\n");
		for(j = 0; j < ilitek_data->x_ch; j++)
		{
			printk("ch%02d,",j);
		}
		printk("%s", "\n");
		for (j = 0; j < ilitek_data->y_ch * ilitek_data->x_ch; j++) {
			printk("%4d,", allnode_data[j]);
			if ((j + 1) % ilitek_data->x_ch == 0) {
				printk("%s", "\n");
			}
		 }
		printk("%s", "\n");
	}

	if (ilitek_printsensortestdata) {
		if (short_test_result == 0 && open_test_result == 0 && mopen_test_result == 0 
			&& allnode_test_result == 0) {
			seq_printf(m, "pass\n");
		} else {
			seq_printf(m, "fail\n");
		}
		seq_printf(m, "%s", "\n");
		seq_printf(m, "short:%s\n", short_test_result ? "FAIL" : "PASS");
		seq_printf(m, "short_threshold test = %d\n", ilitek_short_threshold);

		
		seq_printf(m,"\nx_ch data\n");

		for(j = 0; j < ilitek_data->x_ch; j++)
		{
			seq_printf(m,"ch%02d,",j);
		}
		seq_printf(m,"\n");
		for (j = 0; j < ilitek_data->x_ch; j++) {
			seq_printf(m, "%4d,", short_xdata1[j]);
		}
		seq_printf(m, "%s", "\n");

		seq_printf(m,"\ny_ch data\n");

		for(j = 0; j < ilitek_data->y_ch; j++)
		{
			seq_printf(m,"ch%02d,",j);
		}
		seq_printf(m,"\n");
		for (j = 0; j < ilitek_data->y_ch; j++) {
			seq_printf(m, "%4d,", short_ydata1[j]);
		}
		seq_printf(m, "%s", "\n");

		seq_printf(m, "%s", "\n");
		seq_printf(m, "open:%s\n", open_test_result ? "FAIL" : "PASS");
		seq_printf(m, "Min_Threshold = %d\n", ilitek_data->mp.open_min_thr);
		seq_printf(m, "TX_Average_Diff_Gap = %d\n", ilitek_data->mp.open_tx_avg);
		seq_printf(m, "TX_Average_Diff_Gap_AvoidCorner = %s\n", ilitek_data->mp.open_tx_avg_corner ? "True" : "False");
		seq_printf(m, "RX_Diff_Gap = %d\n", ilitek_data->mp.open_rx_diff);
		seq_printf(m, "RX_Diff_Gap_Tolerance = %d\n", ilitek_data->mp.open_rx_diff_fcnt);

		seq_printf(m,"\nopen data\n");
		for(j = 0; j < ilitek_data->x_ch; j++)
		{
			seq_printf(m,"ch%02d,",j);
		}
		seq_printf(m, "Avg %s", "\n");
		for (i = 0, j = 0; j < ilitek_data->y_ch * ilitek_data->x_ch; j++) {
			seq_printf(m, "%4d,", open_data[j]);
			if ((j + 1) % ilitek_data->x_ch == 0) {
				seq_printf(m, "%4d\n", open_tx_avg[i]);
				i++;
		 	}
		}
		seq_printf(m, "%s\n", "Open Rx Diff:");
		for(j = 0; j < ilitek_data->x_ch - 1; j++)
		{
			seq_printf(m,"ch%02d,",j);
		}
		seq_printf(m, "%s", "\n");
		for (j = 0; j < ilitek_data->y_ch * (ilitek_data->x_ch - 1); j++) {
			seq_printf(m, "%4d,", open_rx_diff[j]);
			if ((j + 1) % (ilitek_data->x_ch - 1) == 0) {
				seq_printf(m, "\n");
				i++;
		 	}
		}

		seq_printf(m, "%s", "\n");
		seq_printf(m, "allnode:%s\n", allnode_test_result ? "FAIL" : "PASS");
		seq_printf(m, "allnode_max_threshold = %d\n", ilitek_allnode_max_threshold);
		seq_printf(m, "allnode_min_threshold = %d\n", ilitek_allnode_min_threshold);
		seq_printf(m, "allnodetestw1 = %d\n", ilitek_allnodetestw1);
		seq_printf(m, "allnodetestw2 = %d\n", ilitek_allnodetestw2);
		seq_printf(m, "allnodemodule = %d\n", ilitek_allnodemodule);
		seq_printf(m, "allnodetx = %d\n", ilitek_allnodetx);

		seq_printf(m,"\nallnode data\n");
		for(j = 0; j < ilitek_data->x_ch; j++)
		{
			seq_printf(m,"ch%02d,",j);
		}
		seq_printf(m, "%s", "\n");
		for (j = 0; j < ilitek_data->y_ch * ilitek_data->x_ch; j++) {
			seq_printf(m, "%4d,", allnode_data[j]);
			if ((j + 1) % ilitek_data->x_ch == 0) {
				seq_printf(m, "%s", "\n");
			}
		 }
		seq_printf(m, "%s", "\n");
		tp_log_info("m->size = %d  m->count = %d\n", (int32_t)m->size, (int32_t)m->count);
	}
	filp_close(filp, NULL);
	return;
}

void ilitek_printf_sensortest_data(int32_t *short_xdata1, int32_t *short_xdata2, int32_t *short_ydata1,
					  int32_t *short_ydata2, int32_t *open_data, int32_t *allnode_data, struct seq_file *m)
{
	int32_t j = 0, len = 0;
	struct file *filp;
	mm_segment_t fs;
	uint8_t buf[128];
	struct timeval time_now;
	struct rtc_time tm;
	do_gettimeofday(&time_now);
	rtc_time_to_tm(time_now.tv_sec, &tm);
	tp_log_info("%d_%d_%d_%d_%d_%d\n", (tm.tm_year + 1900), tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	if (short_test_result == 0 && open_test_result == 0 && mopen_test_result == 0 && allnode_test_result == 0) {
		len = scnprintf(buf, PAGE_SIZE, "ilitek_sensortest_%d%02d%02d%02d%02d%02d_pass.csv",
		 	(tm.tm_year + 1900), tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min,tm.tm_sec);
	} else {
		len = scnprintf(buf, PAGE_SIZE, "ilitek_sensortest_%d%02d%02d%02d%02d%02d_fail.csv", 
			(tm.tm_year + 1900), tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min,tm.tm_sec);
	}
	for (j = 0; j < 256; j++) {
		sensor_test_data_path_tmp[j] = sensor_test_data_path[j];
	}
	strcat(sensor_test_data_path, buf);
	tp_log_info("sensor_test_data_path = %s\n", sensor_test_data_path);
	filp = filp_open(sensor_test_data_path, O_CREAT | O_WRONLY, 0777);
	if (IS_ERR(filp)) {
		tp_log_err("save sensor test  File Open Error path = %s\n", sensor_test_data_path);
	} else {
		fs = get_fs();
		set_fs(KERNEL_DS);

		len = scnprintf(buf, PAGE_SIZE, "short\n");
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		len = scnprintf(buf, PAGE_SIZE, "short_threshold = %d\n", ilitek_short_threshold);
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		for (j = 0; j < ilitek_data->x_ch; j++) {
			len = scnprintf(buf, PAGE_SIZE, "%d,", short_xdata1[j]);
			vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		}
		len = scnprintf(buf, PAGE_SIZE, "\n");
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		for (j = 0; j < ilitek_data->x_ch; j++) {
			len = scnprintf(buf, PAGE_SIZE, "%d,", short_xdata2[j]);
			vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		}
		len = scnprintf(buf, PAGE_SIZE, "\n");
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		len = scnprintf(buf, PAGE_SIZE, "\n");
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		for (j = 0; j < ilitek_data->y_ch; j++) {
			len = scnprintf(buf, PAGE_SIZE, "%d,", short_ydata1[j]);
			vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		}
		len = scnprintf(buf, PAGE_SIZE, "\n");
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		for (j = 0; j < ilitek_data->y_ch; j++) {
			len = scnprintf(buf, PAGE_SIZE, "%d,", short_ydata2[j]);
			vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		}
		len = scnprintf(buf, PAGE_SIZE, "\n");
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		len = scnprintf(buf, PAGE_SIZE, "open:\n");
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		len = scnprintf(buf, PAGE_SIZE, "open_threshold = %d\n", ilitek_open_threshold);
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		len = scnprintf(buf, PAGE_SIZE, "open_txdeltathrehold = %d\n", ilitek_open_txdeltathrehold);
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		len = scnprintf(buf, PAGE_SIZE, "open_rxdeltathrehold = %d\n", ilitek_open_rxdeltathrehold);
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		for (j = 0; j < ilitek_data->y_ch * ilitek_data->x_ch; j++) {
			len = scnprintf(buf, PAGE_SIZE, "%d,", open_data[j]);
			vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
			if ((j + 1) % ilitek_data->x_ch == 0) {
				len = scnprintf(buf, PAGE_SIZE, "\n");
				vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
			}
		}
		len = scnprintf(buf, PAGE_SIZE, "\n");
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		len = scnprintf(buf, PAGE_SIZE, "allnode:\n");
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		len = scnprintf(buf, PAGE_SIZE, "allnode_max_threshold = %d\n", ilitek_allnode_max_threshold);
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		len = scnprintf(buf, PAGE_SIZE, "allnode_min_threshold = %d\n", ilitek_allnode_min_threshold);
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		len = scnprintf(buf, PAGE_SIZE, "allnodetestw1 = %d\n", ilitek_allnodetestw1);
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		len = scnprintf(buf, PAGE_SIZE, "allnodetestw2 = %d\n", ilitek_allnodetestw2);
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		len = scnprintf(buf, PAGE_SIZE, "allnodemodule = %d\n", ilitek_allnodemodule);
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		len = scnprintf(buf, PAGE_SIZE, "allnodetx = %d\n", ilitek_allnodetx);
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		for (j = 0; j < ilitek_data->y_ch * ilitek_data->x_ch; j++) {
			len = scnprintf(buf, PAGE_SIZE, "%d,", allnode_data[j]);
			vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
			if ((j + 1) % ilitek_data->x_ch == 0) {
				len = scnprintf(buf, PAGE_SIZE, "\n");
				vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
			}
		}
		len = scnprintf(buf, PAGE_SIZE, "\n");
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));

		set_fs(fs);
	}
	for (j = 0; j < 256; j++) {
		sensor_test_data_path[j] = sensor_test_data_path_tmp[j];
	}

	if (ilitek_printsensortestdata) {
		if (short_test_result == 0 && open_test_result == 0 && mopen_test_result == 0 
			&& allnode_test_result == 0) {
			printk("pass\n");
		} else {
			printk("fail\n");
		}

		printk("%s", "\n");
		printk("%s", "short\n");
		printk("short_threshold = %d\n", ilitek_short_threshold);

		for (j = 0; j < ilitek_data->x_ch; j++) {
			printk("%d,", short_xdata1[j]);
		}
		printk("%s", "\n");
		for (j = 0; j < ilitek_data->x_ch; j++) {
			printk("%d,", short_xdata2[j]);
		}
		printk("%s", "\n");
		printk("%s", "\n");
		for (j = 0; j < ilitek_data->y_ch; j++) {
			printk("%d,", short_ydata1[j]);
		}
		printk("%s", "\n");
		for (j = 0; j < ilitek_data->y_ch; j++) {
			printk("%d,", short_ydata2[j]);
		}

		printk("%s", "\n");
		printk("%s", "open:\n");
		printk("open_threshold = %d\n", ilitek_open_threshold);
		printk("open_txdeltathrehold = %d\n", ilitek_open_txdeltathrehold);
		printk("open_rxdeltathrehold = %d\n", ilitek_open_rxdeltathrehold);
		for (j = 0; j < ilitek_data->y_ch * ilitek_data->x_ch; j++) {
			printk("%d,", open_data[j]);
			if ((j + 1) % ilitek_data->x_ch == 0) {
				printk("%s", "\n");
			}
		}
		printk("%s", "\n");
		printk("%s", "allnode:\n");
		printk("allnode_max_threshold = %d\n", ilitek_allnode_max_threshold);
		printk("allnode_min_threshold = %d\n", ilitek_allnode_min_threshold);
		printk("allnodetestw1 = %d\n", ilitek_allnodetestw1);
		printk("allnodetestw2 = %d\n", ilitek_allnodetestw2);
		printk("allnodemodule = %d\n", ilitek_allnodemodule);
		printk("allnodetx = %d\n", ilitek_allnodetx);
		for (j = 0; j < ilitek_data->y_ch * ilitek_data->x_ch; j++) {
			printk("%d,", allnode_data[j]);
			if ((j + 1) % ilitek_data->x_ch == 0) {
				printk("%s", "\n");
			}
		}
		printk("%s", "\n");
	}

	if (ilitek_printsensortestdata) {
		if (short_test_result == 0 && open_test_result == 0 && mopen_test_result == 0 
			&& allnode_test_result == 0) {
			seq_printf(m, "pass\n");
		} else {
			seq_printf(m, "fail\n");
		}
		seq_printf(m, "%s", "\n");
		seq_printf(m, "%s", "short\n");
		seq_printf(m, "short_threshold = %d\n", ilitek_short_threshold);

		for (j = 0; j < ilitek_data->x_ch; j++) {
			seq_printf(m, "%d,", short_xdata1[j]);
		}
		seq_printf(m, "%s", "\n");
		for (j = 0; j < ilitek_data->x_ch; j++) {
			seq_printf(m, "%d,", short_xdata2[j]);
		}
		seq_printf(m, "%s", "\n");
		seq_printf(m, "%s", "\n");
		for (j = 0; j < ilitek_data->y_ch; j++) {
			seq_printf(m, "%d,", short_ydata1[j]);
		}
		seq_printf(m, "%s", "\n");
		for (j = 0; j < ilitek_data->y_ch; j++) {
			seq_printf(m, "%d,", short_ydata2[j]);
		}

		seq_printf(m, "%s", "\n");
		seq_printf(m, "%s", "open:\n");
		seq_printf(m, "open_threshold = %d\n", ilitek_open_threshold);
		seq_printf(m, "open_txdeltathrehold = %d\n", ilitek_open_txdeltathrehold);
		seq_printf(m, "open_rxdeltathrehold = %d\n", ilitek_open_rxdeltathrehold);
		for (j = 0; j < ilitek_data->y_ch * ilitek_data->x_ch; j++) {
			seq_printf(m, "%d,", open_data[j]);
			if ((j + 1) % ilitek_data->x_ch == 0) {
				seq_printf(m, "%s", "\n");
			}
		}
		seq_printf(m, "%s", "\n");
		seq_printf(m, "%s", "allnode:\n");
		seq_printf(m, "allnode_max_threshold = %d\n", ilitek_allnode_max_threshold);
		seq_printf(m, "allnode_min_threshold = %d\n", ilitek_allnode_min_threshold);
		seq_printf(m, "allnodetestw1 = %d\n", ilitek_allnodetestw1);
		seq_printf(m, "allnodetestw2 = %d\n", ilitek_allnodetestw2);
		seq_printf(m, "allnodemodule = %d\n", ilitek_allnodemodule);
		seq_printf(m, "allnodetx = %d\n", ilitek_allnodetx);
		for (j = 0; j < ilitek_data->y_ch * ilitek_data->x_ch; j++) {
			seq_printf(m, "%d,", allnode_data[j]);
			if ((j + 1) % ilitek_data->x_ch == 0) {
				seq_printf(m, "%s", "\n");
			}
		}
		seq_printf(m, "%s", "\n");
		tp_log_info("m->size = %d  m->count = %d\n", (int32_t)m->size, (int32_t)m->count);
	}
	return;
}

int ilitek_inital_rawdata_v6(uint8_t cmd)
{
	uint8_t inbuf[2] = {0};

	inbuf[1] = cmd;
	if (api_protocol_set_cmd(ILITEK_TP_CMD_SET_CDC_INITOAL_V6, inbuf, NULL) < 0) {
		return ILITEK_FAIL;
	}
	if (ilitek_check_busy(100, 10, ILITEK_TP_SYSTEM_BUSY|ILITEK_TP_INITIAL_BUSY) < 0)
	{
		tp_log_err("%s, CDC Initial: CheckBusy Failed\n", __func__);
		return ILITEK_FAIL;
	}
	return ILITEK_SUCCESS;
}

int ilitek_get_rawdata_v6(uint32_t d_len, uint32_t *rawdata)
{
    uint32_t uiReadCounts = 0;
    uint32_t uiIndex = 0;
    int inNeedCounts = d_len, t_len = RAW_DATA_TRANSGER_LENGTH;
    int inGettedCounts = 0;
    int ret = 0;
	int header = 6;     //cmd 1byte, sense channel 2byte, drive channel 2byte, checksum 1byte.
    int start = 5;      //cmd 1byte, sense channel 2byte, drive channel 2byte.
	int ckbyte = 1;		//checksum 1byte.
    uint8_t Wbuff[64] = {0}, *Rbuff = NULL;

	tp_log_info("inNeedCounts = %d, t_len = %d\n", inNeedCounts, t_len);
    ret = api_set_data_length(t_len);
    Rbuff = vmalloc((t_len + header) * sizeof(uint8_t));
    do {
			if (inNeedCounts >= t_len)
			{
					uiReadCounts = t_len + header;
			}
			else
			{
					uiReadCounts = inNeedCounts + header;
			}

			Wbuff[0] = ILITEK_TP_CMD_GET_CDC_DATA_V6;
			if (ilitek_i2c_write_and_read(Wbuff, 1, 1, Rbuff, uiReadCounts) < 0) {
				return ILITEK_I2C_TRANSFER_ERR;
			}
			if (ret < 0)
			{
					tp_log_err("Error! Read Data error \n");
					vfree(Rbuff);
					return ILITEK_FAIL;
			}

			inGettedCounts = (int)Rbuff[1] + (int)(Rbuff[2] << 8) + (int)(Rbuff[3] + (Rbuff[4] << 8))* ilitek_data->x_ch;
			tp_log_info("inGettedCounts =%d,d_len=%d, uiReadCounts =%d\n", inGettedCounts, d_len, uiReadCounts);
			for (uiIndex = start; uiIndex < uiReadCounts - ckbyte; uiIndex += 2, inGettedCounts++)
			{
					rawdata[inGettedCounts] = (uint16_t)(Rbuff[uiIndex] + (Rbuff[uiIndex+1] << 8));
					//tp_log_info("raw[%d]:%d,", inGettedCounts, rawdata[inGettedCounts]);
			}
			inNeedCounts = d_len - inGettedCounts;
			inNeedCounts -= (uiReadCounts - header);
			//tp_log_info("inNeedCounts = %d, uiReadCounts = %d\n", inNeedCounts, uiReadCounts);
    } while (inNeedCounts > 0);

    vfree(Rbuff);
    return 1;
}


int32_t ilitek_allnode_test(int32_t *allnode_data)
{
	int32_t ret = 0, newMaxSize = 32, i = 0, j = 0, k = 0, read_len = 0, index = 0, diff = 0;
	int32_t maxneighborDiff = 0, maxintercrossdiff = 0, txerror_count = 0, moduleerror_count = 0;
	bool txerror_result = false;
	uint8_t cmd[4] = { 0 };
	uint8_t *buf_recv = NULL;
	int32_t test_32 = 0;
	allnode_test_result = 0;
	test_32 = (ilitek_data->y_ch * ilitek_data->x_ch * 2) / (newMaxSize - 2);
	if ((ilitek_data->y_ch * ilitek_data->x_ch * 2) % (newMaxSize - 2) != 0) {
		test_32 += 1;
	}
	tp_log_info("kzalloc  test_32 = %d\n", test_32);
	buf_recv = (uint8_t *) vmalloc(sizeof(uint8_t) * (ilitek_data->y_ch * ilitek_data->x_ch * 2 + test_32 * 2 + 32));
	if (NULL == buf_recv) {
		tp_log_err("buf_recv NULL\n");
		return -ENOMEM;
	}
	mdelay(10);
	//initial
	cmd[0] = 0xF3;
	cmd[1] = 0x0B;
	cmd[2] = 0x00;
	if (ilitek_data->mcu_ver[1] == 0x23 || ((ilitek_data->mcu_ver[0] == 0x11 || ilitek_data->mcu_ver[0] == 0x10) && ilitek_data->mcu_ver[1] == 0x25)) {
		cmd[3] = 0xE6;
	} else {
		cmd[3] = 0xE2;
	}
	ret = ilitek_i2c_write(cmd, 4);
	if (ret < 0) {
		tp_log_err("allnode test  initial set err ret = %d\n", ret);
	}
	mdelay(1000);
	ret = ilitek_check_busy(1000, 5, ILITEK_TP_SYSTEM_BUSY);
	if (ret < 0) {
		tp_log_err("allnode test  check busy err ret = %d\n", ret);
	}
	mdelay(100);
	if (ilitek_data->mcu_ver[1] == 0x23 || ((ilitek_data->mcu_ver[0] == 0x11 || ilitek_data->mcu_ver[0] == 0x10) && ilitek_data->mcu_ver[1] == 0x25)) {
		test_32 = (ilitek_data->y_ch * ilitek_data->x_ch * 2) / (newMaxSize - 2);
		if ((ilitek_data->y_ch * ilitek_data->x_ch * 2) % (newMaxSize - 2) != 0) {
			test_32 += 1;
		}
		cmd[0] = 0xE6;
		ret = ilitek_i2c_write(cmd, 1);
		mdelay(10);
		tp_log_info("ilitek_allnode_test test_32 = %d\n", test_32);
		for (i = 0; i < test_32; i++) {
			if ((ilitek_data->y_ch * ilitek_data->x_ch * 2) % (newMaxSize - 2) != 0 && i == test_32 - 1) {
				ret = ilitek_i2c_read(buf_recv + newMaxSize * i, (ilitek_data->y_ch * ilitek_data->x_ch * 2) % (newMaxSize - 2) + 2);
			} else {
				ret = ilitek_i2c_read(buf_recv + newMaxSize * i, newMaxSize);
			}
			if (ret < 0) {
				tp_log_err("err,i2c read error ret %d\n", ret);
			}
		}
		index = 0;
		for (i = 0; i < test_32; i++) {
			if (index == (ilitek_data->y_ch * ilitek_data->x_ch)) {
				break;
			}
			for (j = 2; j < newMaxSize;) {
				allnode_data[index++] = (buf_recv[newMaxSize * i + j]) + (buf_recv[newMaxSize * i + j + 1] << 8);
				j += 2;
				if (index == (ilitek_data->y_ch * ilitek_data->x_ch)) {
					break;
				}
			}
		}
	} else {
		test_32 = (ilitek_data->x_ch) / (newMaxSize - 2);
		if ((ilitek_data->x_ch) % (newMaxSize - 2) != 0) {
			test_32 += 1;
		}
		tp_log_info("ilitek_allnode_test test_32 = %d\n", test_32);
		cmd[0] = 0xE2;
		index = 0;
		for (j = 0; j < ilitek_data->y_ch; j++) {
			for (i = 0; i < test_32; i++) {
				if ((ilitek_data->x_ch) % (newMaxSize - 2) != 0 && i == test_32 - 1) {
					read_len = (ilitek_data->x_ch) % (newMaxSize - 2) + 2;
				} else {
					read_len = newMaxSize;
				}
				ret = ilitek_i2c_write_and_read(cmd, 1, 2, buf_recv, read_len);
				if (ret < 0) {
					tp_log_err("i2c read err ret %d\n", ret);
				}
				for (k = 2; k < read_len; k++) {
					allnode_data[index++] = (buf_recv[k]);
					//tp_log_info("buf_recv[newMaxSize * i + j] = %d\n", buf_recv[k]);
				}
				msleep(10);
			}
		}
	}

#if 1
	//handle
	for (i = 0; i < ilitek_data->y_ch; i++) {
		txerror_count = 0;
		for (j = 0; j < ilitek_data->x_ch; j++) {

			if (allnode_data[i * ilitek_data->x_ch + j] > ilitek_allnode_max_threshold ||
			    allnode_data[i * ilitek_data->x_ch + j] < ilitek_allnode_min_threshold) {
				allnode_test_result = -1;
				tp_log_err(" err,allnode_test_result error allnode_data = %d, ilitek_allnode_max_threshold = %d,\
					ilitek_allnode_min_threshold = %d i = %d, j = %d\n", allnode_data[i * ilitek_data->x_ch + j], ilitek_allnode_max_threshold, ilitek_allnode_min_threshold, i, j);
				break;
			}

			if (i > 0) {
				diff = abs(allnode_data[i * ilitek_data->x_ch + j] - allnode_data[(i - 1) * ilitek_data->x_ch + j]);
				if (diff > maxneighborDiff) {
					maxneighborDiff = diff;
				}
				if (j > 0) {
					diff = abs((allnode_data[(i - 1) * ilitek_data->x_ch + j - 1] - allnode_data[i * ilitek_data->x_ch + j - 1]) -
						   (allnode_data[(i - 1) * ilitek_data->x_ch + j] - allnode_data[i * ilitek_data->x_ch + j]));
					if (diff > maxintercrossdiff) {
						maxintercrossdiff = diff;
					}
					if (diff > ilitek_allnodetestw2) {
						moduleerror_count++;
						txerror_count++;
						tp_log_err("allnodetestw2 err i = %d j = %d txerror_count = %d moduleerror_count = %d\n",
							   i, j, txerror_count, moduleerror_count);
					}
				}
			}
		}
		if (txerror_count > ilitek_allnodetx) {
			txerror_result = true;
		}
	}

	if (ilitek_data->mcu_ver[1] == 0x23 || ((ilitek_data->mcu_ver[0] == 0x11 || ilitek_data->mcu_ver[0] == 0x10) && ilitek_data->mcu_ver[1] == 0x25)) {
		if (txerror_result && (moduleerror_count > ilitek_allnodemodule)) {
			allnode_test_result = -1;
		}
	} else {
		if (maxneighborDiff > ilitek_allnodetestw1) {
			allnode_test_result = -1;
			tp_log_err("maxneighborDiff = %d, ilitek_allnodetestw1 = %d\n", maxneighborDiff, ilitek_allnodetestw1);
		}
		if (maxintercrossdiff > ilitek_allnodetestw2) {
			allnode_test_result = -1;
			tp_log_err("maxintercrossdiff = %d, ilitek_allnodetestw2 = %d\n", maxintercrossdiff, ilitek_allnodetestw2);
		}
	}
#endif
	if (buf_recv) {
		vfree(buf_recv);
		buf_recv = NULL;
	}
	return allnode_test_result;
}
 
int32_t ilitek_open_test(int32_t *open_data)
{
	int32_t ret = 0, newMaxSize = 32, i = 0, j = 0, k = 0, read_len = 0, index = 0, value = 0;
	int32_t rxfailindex = 0, rxfail_count = 0;
	int32_t txfailindex = 0, txfail_count = 0;
	uint8_t cmd[4] = { 0 };
	uint8_t *buf_recv = NULL;
	int32_t test_32 = 0;

	open_test_result = 0;
	test_32 = (ilitek_data->y_ch * ilitek_data->x_ch * 2) / (newMaxSize - 2);
	if ((ilitek_data->y_ch * ilitek_data->x_ch * 2) % (newMaxSize - 2) != 0) {
		test_32 += 1;
	}
	tp_log_info("kzalloc  test_32 = %d\n", test_32);
	buf_recv = (uint8_t *) vmalloc(sizeof(uint8_t) * (ilitek_data->y_ch * ilitek_data->x_ch * 2 + test_32 * 2 + 32));
	if (NULL == buf_recv) {
		tp_log_err("buf_recv NULL\n");
		return -ENOMEM;
	}
	mdelay(10);
	//initial
	cmd[0] = 0xF3;
	cmd[1] = 0x0C;
	cmd[2] = 0x00;
	if (ilitek_data->mcu_ver[1] == 0x23 || ((ilitek_data->mcu_ver[0] == 0x11 || ilitek_data->mcu_ver[0] == 0x10) && ilitek_data->mcu_ver[1] == 0x25)) {
		cmd[3] = 0xE6;
	} else {
		cmd[3] = 0xE2;
	}
	ret = ilitek_i2c_write(cmd, 4);
	if (ret < 0) {
		tp_log_err("open test  initial set err ret = %d\n", ret);
	}
	mdelay(1000);
	ret = ilitek_check_busy(1000, 5, ILITEK_TP_SYSTEM_BUSY);
	if (ret < 0) {
		tp_log_err("open test  check busy err ret = %d\n", ret);
	}
	mdelay(100);
	if (ilitek_data->mcu_ver[1] == 0x23 || ((ilitek_data->mcu_ver[0] == 0x11 || ilitek_data->mcu_ver[0] == 0x10) && ilitek_data->mcu_ver[1] == 0x25)) {
		test_32 = (ilitek_data->y_ch * ilitek_data->x_ch * 2) / (newMaxSize - 2);
		if ((ilitek_data->y_ch * ilitek_data->x_ch * 2) % (newMaxSize - 2) != 0) {
			test_32 += 1;
		}
		cmd[0] = 0xE6;
		ret = ilitek_i2c_write(cmd, 1);
		mdelay(10);
		tp_log_info("ilitek_open_test test_32 = %d\n", test_32);
		for (i = 0; i < test_32; i++) {
			if ((ilitek_data->y_ch * ilitek_data->x_ch * 2) % (newMaxSize - 2) != 0 && i == test_32 - 1) {
				ret = ilitek_i2c_read(buf_recv + newMaxSize * i, (ilitek_data->y_ch * ilitek_data->x_ch * 2) % (newMaxSize - 2) + 2);
			} else {
				ret = ilitek_i2c_read(buf_recv + newMaxSize * i, newMaxSize);
			}
			if (ret < 0) {
				tp_log_err("err,i2c read error ret %d\n", ret);
			}
		}
		index = 0;
		for (i = 0; i < test_32; i++) {
			if (index == (ilitek_data->y_ch * ilitek_data->x_ch)) {
				break;
			}
			for (j = 2; j < newMaxSize;) {
				open_data[index++] = (buf_recv[newMaxSize * i + j]) + (buf_recv[newMaxSize * i + j + 1] << 8);
				j += 2;
				if (index == (ilitek_data->y_ch * ilitek_data->x_ch)) {
					break;
				}
			}
		}
	} else {
		test_32 = (ilitek_data->x_ch) / (newMaxSize - 2);
		if ((ilitek_data->x_ch) % (newMaxSize - 2) != 0) {
			test_32 += 1;
		}
		tp_log_info("ilitek_open_test test_32 = %d\n", test_32);
		cmd[0] = 0xE2;
		index = 0;
		for (j = 0; j < ilitek_data->y_ch; j++) {
			for (i = 0; i < test_32; i++) {
				if ((ilitek_data->x_ch) % (newMaxSize - 2) != 0 && i == test_32 - 1) {
					read_len = (ilitek_data->x_ch) % (newMaxSize - 2) + 2;
				} else {
					read_len = newMaxSize;
				}
				ret = ilitek_i2c_write_and_read(cmd, 1, 2, buf_recv, read_len);
				if (ret < 0) {
					tp_log_err("i2c read err ret %d\n", ret);
				}
				for (k = 2; k < read_len; k++) {
					open_data[index++] = (buf_recv[k]);
				}
				msleep(10);
			}
		}
	}
	//check
#if 1
	index = 0;
	for (i = 0; i < ilitek_data->y_ch; i++) {
		for (j = 0; j < ilitek_data->x_ch; j++) {
			if (open_data[index++] < ilitek_open_threshold) {
				open_test_result = -1;
				tp_log_err(" err,open_test_result error open_data[%d] = %d, ilitek_open_threshold = %d\n",
					   (index - 1), open_data[index - 1], ilitek_open_threshold);
				break;
			}
		}
	}
	if (!open_test_result) {
		if (ilitek_data->mcu_ver[1] == 0x23 ||
		    ((ilitek_data->mcu_ver[0] == 0x11 || ilitek_data->mcu_ver[0] == 0x10) && ilitek_data->mcu_ver[1] == 0x25)) {
			for (i = 0; i < ilitek_data->y_ch; i++) {
				rxfailindex = 0;
				rxfail_count = 0;
				for (j = 0; j < ilitek_data->x_ch - 1; j++) {
					value = abs(open_data[i * ilitek_data->x_ch + j + 1] - open_data[i * ilitek_data->x_ch + j]);
					if (value > ilitek_open_rxdeltathrehold) {
						tp_log_err(" open_test rxfail_count = %d i = %d j = %d\n", rxfail_count, i, j);
						if (rxfail_count == 0) {
							rxfailindex = j;
							rxfail_count++;
						} else {
							if ((j - rxfailindex) == 1) {
								rxfailindex = j;
								rxfail_count++;
								if (rxfail_count >= 3) {
									open_test_result = -1;
									tp_log_err(" err,open_test_result error rxfail_count = %d\n", rxfail_count);
									break;
								}
							} else {
								rxfailindex = j;
								rxfail_count = 1;
							}
						}
					}
				}
			}
			if (!open_test_result) {
				for (i = 0; i < ilitek_data->y_ch - 1; i++) {
					txfailindex = 0;
					txfail_count = 0;
					for (j = 0; j < ilitek_data->x_ch; j++) {
						value = abs(open_data[(i + 1) * ilitek_data->x_ch + j] - open_data[i * ilitek_data->x_ch + j]);
						if (value > ilitek_open_txdeltathrehold) {
							tp_log_err(" open_test txfail_count = %d i = %d j = %d\n", txfail_count, i, j);
							if (txfail_count == 0) {
								txfailindex = j;
								txfail_count++;
							} else {
								if ((j - txfailindex) == 1) {
									txfailindex = j;
									txfail_count++;
									if (txfail_count >= 3) {
										open_test_result = -1;
										tp_log_err(" err,open_test_result error txfail_count = %d\n", txfail_count);
										break;
									}
								} else {
									txfailindex = j;
									txfail_count = 1;
								}
							}
						}
					}
				}
			}
		}
	}
#endif
	if (buf_recv) {
		vfree(buf_recv);
		buf_recv = NULL;
	}
	return open_test_result;
}

int32_t ilitek_short_test(int32_t *short_xdata1, int32_t *short_xdata2, int32_t *short_ydata1, int32_t *short_ydata2)
{
	int32_t ret = 0, newMaxSize = 32, i = 0, j = 0, index = 0;
	uint8_t cmd[4] = { 0 };
	uint8_t *buf_recv = NULL;
	int32_t test_32 = 0;
	short_test_result = 0;
	index = ilitek_data->x_ch;
	if (ilitek_data->x_ch < ilitek_data->y_ch) {
		index = ilitek_data->y_ch;
	}
	test_32 = index / (newMaxSize - 2);
	if (index % (newMaxSize - 2) != 0) {
		test_32 += 1;
	}
	tp_log_info("kzalloc  test_32 = %d\n", test_32);
	buf_recv = (uint8_t *) vmalloc(sizeof(uint8_t) * (index + test_32 * 2 + 32));
	if (NULL == buf_recv) {
		tp_log_err("buf_recv NULL\n");
		return -ENOMEM;
	}
	//initial
	cmd[0] = 0xF3;
	cmd[1] = 0x09;
	cmd[2] = 0x00;
	cmd[3] = 0xE0;
	ret = ilitek_i2c_write(cmd, 4);
	if (ret < 0) {
		tp_log_err("short test  initial set err ret = %d\n", ret);
	}
	ret = ilitek_check_busy(1000, 5, ILITEK_TP_SYSTEM_BUSY);
	if (ret < 0) {
		tp_log_err("short test  check busy err ret = %d\n", ret);
	}
	mdelay(100);
	test_32 = ilitek_data->x_ch / (newMaxSize - 2);
	if (ilitek_data->x_ch % (newMaxSize - 2) != 0) {
		test_32 += 1;
	}
	tp_log_info("short xch  test_32 = %d\n", test_32);
	cmd[0] = 0xE0;
	for (i = 0; i < test_32; i++) {
		if ((ilitek_data->x_ch) % (newMaxSize - 2) != 0 && i == test_32 - 1) {
			ret = ilitek_i2c_write_and_read(cmd, 1, 2, buf_recv + newMaxSize * i, (ilitek_data->x_ch) % (newMaxSize - 2) + 2);
		} else {
			ret = ilitek_i2c_write_and_read(cmd, 1, 2, buf_recv + newMaxSize * i, newMaxSize);
		}
		if (ret < 0) {
			tp_log_err("i2c read err ret %d\n", ret);
		}
	}

	j = 0;
	for (i = 0; i < test_32; i++) {
		if (j == ilitek_data->x_ch) {
			break;
		}
		for (index = 2; index < newMaxSize; index++) {
			short_xdata1[j] = buf_recv[i * newMaxSize + index];
			j++;
			if (j == ilitek_data->x_ch) {
				break;
			}
		}
	}
	if (ilitek_data->mcu_ver[1] == 0x23 || ((ilitek_data->mcu_ver[0] == 0x11 || ilitek_data->mcu_ver[0] == 0x10) && ilitek_data->mcu_ver[1] == 0x25)) {
		cmd[0] = 0xF3;
		cmd[1] = 0x09;
		cmd[2] = 0x00;
		cmd[3] = 0xE1;
		ret = ilitek_i2c_write(cmd, 4);
		if (ret < 0) {
			tp_log_err("short test	initial set err ret = %d\n", ret);
		}
		//check busy
		ret = ilitek_check_busy(1000, 5, ILITEK_TP_SYSTEM_BUSY);
		if (ret < 0) {
			tp_log_err("short test	check busy err ret = %d\n", ret);
		}
	}

	test_32 = ilitek_data->y_ch / (newMaxSize - 2);
	if (ilitek_data->y_ch % (newMaxSize - 2) != 0) {
		test_32 += 1;
	}
	tp_log_info("short ych  test_32 = %d\n", test_32);
	msleep(100);
	cmd[0] = 0xE1;
	for (i = 0; i < test_32; i++) {
		if ((ilitek_data->y_ch) % (newMaxSize - 2) != 0 && i == test_32 - 1) {
			ret = ilitek_i2c_write_and_read(cmd, 1, 2, buf_recv + newMaxSize * i, (ilitek_data->y_ch) % (newMaxSize - 2) + 2);
		} else {
			ret = ilitek_i2c_write_and_read(cmd, 1, 2, buf_recv + newMaxSize * i, newMaxSize);
		}
		if (ret < 0) {
			tp_log_err("i2c read err ret %d\n", ret);
		}
	}

	j = 0;
	for (i = 0; i < test_32; i++) {
		if (j == ilitek_data->y_ch) {
			break;
		}
		for (index = 2; index < newMaxSize; index++) {
			short_ydata1[j] = buf_recv[i * newMaxSize + index];
			j++;
			if (j == ilitek_data->y_ch) {
				break;
			}
		}
	}
	msleep(100);
	//initial
	cmd[0] = 0xF3;
	cmd[1] = 0x0A;
	cmd[2] = 0x00;
	cmd[3] = 0xE0;
	ret = ilitek_i2c_write(cmd, 4);
	if (ret < 0) {
		tp_log_err("short test	initial set err ret = %d\n", ret);
	}
	//check busy
	ret = ilitek_check_busy(1000, 5, ILITEK_TP_SYSTEM_BUSY);
	if (ret < 0) {
		tp_log_err("short test	check busy err ret = %d\n", ret);
	}
	mdelay(100);
	test_32 = ilitek_data->x_ch / (newMaxSize - 2);
	if (ilitek_data->x_ch % (newMaxSize - 2) != 0) {
		test_32 += 1;
	}
	tp_log_info("short xch  test_32 = %d\n", test_32);
	cmd[0] = 0xE0;
	for (i = 0; i < test_32; i++) {
		if ((ilitek_data->x_ch) % (newMaxSize - 2) != 0 && i == test_32 - 1) {
			ret = ilitek_i2c_write_and_read(cmd, 1, 2, buf_recv + newMaxSize * i, (ilitek_data->x_ch) % (newMaxSize - 2) + 2);
		} else {
			ret = ilitek_i2c_write_and_read(cmd, 1, 2, buf_recv + newMaxSize * i, newMaxSize);
		}
		if (ret < 0) {
			tp_log_err("i2c read err ret %d\n", ret);
		}
	}

	j = 0;
	for (i = 0; i < test_32; i++) {
		if (j == ilitek_data->x_ch) {
			break;
		}
		for (index = 2; index < newMaxSize; index++) {
			short_xdata2[j] = buf_recv[i * newMaxSize + index];
			j++;
			if (j == ilitek_data->x_ch) {
				break;
			}
		}
	}

	if (ilitek_data->mcu_ver[1] == 0x23 || ((ilitek_data->mcu_ver[0] == 0x11 || ilitek_data->mcu_ver[0] == 0x10) && ilitek_data->mcu_ver[1] == 0x25)) {
		cmd[0] = 0xF3;
		cmd[1] = 0x0A;
		cmd[2] = 0x00;
		cmd[3] = 0xE1;
		ret = ilitek_i2c_write(cmd, 4);
		if (ret < 0) {
			tp_log_err("short test	initial set err ret = %d\n", ret);
		}
		//check busy
		ret = ilitek_check_busy(1000, 5, ILITEK_TP_SYSTEM_BUSY);
		if (ret < 0) {
			tp_log_err("short test	check busy err ret = %d\n", ret);
		}
	}
	test_32 = ilitek_data->y_ch / (newMaxSize - 2);
	if (ilitek_data->y_ch % (newMaxSize - 2) != 0) {
		test_32 += 1;
	}
	tp_log_info("short ych  test_32 = %d\n", test_32);
	msleep(100);
	cmd[0] = 0xE1;
	for (i = 0; i < test_32; i++) {
		if ((ilitek_data->y_ch) % (newMaxSize - 2) != 0 && i == test_32 - 1) {
			ret = ilitek_i2c_write_and_read(cmd, 1, 2, buf_recv + newMaxSize * i, (ilitek_data->y_ch) % (newMaxSize - 2) + 2);
		} else {
			ret = ilitek_i2c_write_and_read(cmd, 1, 2, buf_recv + newMaxSize * i, newMaxSize);
		}
		if (ret < 0) {
			tp_log_err("i2c read err ret %d\n", ret);
		}
	}

	j = 0;
	for (i = 0; i < test_32; i++) {
		if (j == ilitek_data->y_ch) {
			break;
		}
		for (index = 2; index < newMaxSize; index++) {
			short_ydata2[j] = buf_recv[i * newMaxSize + index];
			j++;
			if (j == ilitek_data->y_ch) {
				break;
			}
		}
	}
	//check pass or fail
	for (i = 0; i < ilitek_data->x_ch; i++) {
		if (abs(short_xdata1[i] - short_xdata2[i]) > ilitek_short_threshold) {
			short_test_result = -1;
			tp_log_err("[TP_selftest] err,short_test_result error short_xdata1[%d] = %d, short_xdata2[%d] = %d, ilitek_short_threshold = %d\n",
				   i, short_xdata1[i], i, short_xdata2[i], ilitek_short_threshold);
			break;
		}
	}
	if (short_test_result == 0) {
		for (i = 0; i < ilitek_data->y_ch; i++) {
			if (abs(short_ydata1[i] - short_ydata2[i]) > ilitek_short_threshold) {
				short_test_result = -1;
				tp_log_err
				    ("[TP_selftest] err,short_test_result error short_ydata1[%d] = %d, short_ydata2[%d] = %d, ilitek_short_threshold = %d\n", i,
				     short_ydata1[i], i, short_ydata2[i], ilitek_short_threshold);
				break;
			}
		}
	}
	if (buf_recv) {
		vfree(buf_recv);
		buf_recv = NULL;
	}
	return short_test_result;
}

int32_t ilitek_short_test_v6(int32_t *short_xdata1, int32_t *short_ydata1)
{
	int32_t ret = 0, index = 0;
	//uint8_t *buf_recv = NULL;
	uint8_t inbuf[2] = {0};

	short_test_result = 0;
	inbuf[1] = ENTER_TEST_MODE;
	if (api_protocol_set_cmd(ILITEK_TP_CMD_SET_MODE_CONTORL, inbuf, NULL) < 0) {
		return ILITEK_FAIL;
	}
    //set FS Info
    if(api_set_short_Info(ilitek_Dump1_Value, ilitek_Dump2_Value, ilitek_short_Vref, ilitek_posidle_L, ilitek_posidle_H) < 0)
    {
        return ILITEK_FAIL;
    }
	//initial
	if(ilitek_inital_rawdata_v6(TEST_MODE_V6_SHORT_RX) == ILITEK_SUCCESS)
	{
		if (ilitek_get_rawdata_v6(ilitek_data->x_ch * 2, short_xdata1) != ILITEK_FAIL)
		{
			ret = ILITEK_SUCCESS;
		}
		else
		{
			tp_log_err("Error! Get RawData Failed!\n");
			return ILITEK_FAIL;
		}
	}
	else
	{
		tp_log_err("Error! Init RawData Failed!\n");
		return ILITEK_FAIL;
	}
	if(ilitek_inital_rawdata_v6(TEST_MODE_V6_SHORT_TX) == ILITEK_SUCCESS)
	{
		if (ilitek_get_rawdata_v6(ilitek_data->y_ch * 2, short_ydata1) != ILITEK_FAIL)
		{
			ret = ILITEK_SUCCESS;
		}
		else
		{
			tp_log_err("Error! Get RawData Failed!\n");
			return ILITEK_FAIL;
		}
	}
	else
	{
		tp_log_err("Error! Init RawData Failed!\n");
		return ILITEK_FAIL;
	}
	for (index = 0; index < ilitek_data->x_ch; index++)
	{
		if(short_xdata1[index] > ilitek_short_threshold) {
			short_test_result = ILITEK_FAIL;
		}
	}
	for (index = 0; index < ilitek_data->y_ch; index++)
	{
		if(short_ydata1[index] > ilitek_short_threshold) {
			short_test_result = ILITEK_FAIL;
		}
	}
	inbuf[1] = ENTER_NORMAL_MODE;
	if (api_protocol_set_cmd(ILITEK_TP_CMD_SET_MODE_CONTORL, inbuf, NULL) < 0) {
		return ILITEK_FAIL;
	}
	msleep(1000);
	return short_test_result;
}

int ilitek_open_rxdiff(int32_t *open, int32_t *rx_diff) {
    int ch_x = 0, ch_y = 0;
    int ret = ILITEK_SUCCESS;
    int count = 0;
    int RXFailCount = 0;


	for (ch_y = 0; ch_y < ilitek_data->y_ch; ch_y++)
	{
		printk("Y_%3dCH:", ch_y);
		for (ch_x = 0; ch_x < ilitek_data->x_ch; ch_x++)
		{
			printk("%d,", open[ch_y*ilitek_data->x_ch+ch_x]);
        }
		printk("\n");
    }
	printk("Rx Diff:");
    for (ch_x = 0; ch_x < ilitek_data->x_ch - 1; ch_x++) {
        printk("%3dCH,", ch_x);
    }
	printk("\n");
	for (ch_y = 0; ch_y < ilitek_data->y_ch; ch_y++)
	{
		printk("Y_%3dCH:", ch_y);
		for (ch_x = 1; ch_x < ilitek_data->x_ch; ch_x++)
		{
			ilitek_data->mp.rxdiff[count].max_st = true;
			rx_diff[count] = abs(open[ch_y*ilitek_data->x_ch + ch_x] - open[ch_y*ilitek_data->x_ch + ch_x - 1]);
			//printk("[%d][%d]=%d - [%d][%d]=%d, [%d]%d\n", ch_y, ch_x, open[ch_y*ch_x + ch_x], ch_y, ch_x - 1,open[ch_y*ch_x + ch_x - 1], count, rx_diff[count]);
            if (rx_diff[count] > ilitek_data->mp.rxdiff[count].max)
            {
				printk("*%4d,", rx_diff[count]);
				ilitek_data->mp.rxdiff[count].max_st = false;
                ++RXFailCount;
            }
			else
			{
				printk("%5d,", rx_diff[count]);
			}
            if (RXFailCount >= ilitek_data->mp.open_rx_diff_fcnt)
            {
                ret = ILITEK_FAIL;
            }
			count++;
        }
		printk("\n");
    }
    return ret;
}

int ilitek_open_avg(int32_t *open, int32_t *tx_avg) {
    int ch_x = 0, ch_y = 0;
    int ret = ILITEK_SUCCESS;
    int count = 0;

    for (ch_y = 0; ch_y <ilitek_data->y_ch; ch_y++)
    {
        count = 0;
		tx_avg[ch_y] = 0;
        for (ch_x = 0; ch_x <ilitek_data->x_ch; ch_x++)
        {
            if(ilitek_data->mp.open_tx_avg_corner) {
                //No corner calculations
                if(ch_y == 0 && (ch_x == 0 || ch_x ==ilitek_data->x_ch - 1)) {
                    //printk("No corner calculations:[%d][%d]\n", ch_x,ch_y);
                    continue;
                }
                if(ch_y ==ilitek_data->y_ch - 1 && (ch_x == 0 || ch_x ==ilitek_data->x_ch - 1)) {
                    //printk("No corner calculations:[%d][%d]\n", ch_x,ch_y);
                    continue;
                }
            }
            count++;
            tx_avg[ch_y] = tx_avg[ch_y] + open[ch_y*ilitek_data->x_ch + ch_x];
			//printk("[%d][%d]=tx_avg=%d, open=%d\n", ch_y, ch_x, tx_avg[ch_y], open[ch_y*ch_x + ch_x]);
        }
        tx_avg[ch_y] = tx_avg[ch_y] / count;
		printk("Tx avg[%d]:%d,conut=%d\n", ch_y, tx_avg[ch_y],count);

    }
	ilitek_data->mp.txavg[ilitek_data->y_ch - 1].max_st = true;
    for (ch_y = 1; ch_y <ilitek_data->y_ch; ch_y++)
    {
		ilitek_data->mp.txavg[ch_y - 1].max_st = true;
		ilitek_data->mp.txavg[ch_y - 1].data = abs(tx_avg[ch_y] - tx_avg[ch_y - 1]);
		if(ilitek_data->mp.txavg[ch_y - 1].data > ilitek_data->mp.txavg[ch_y - 1].max) {
			ilitek_data->mp.txavg[ch_y - 1].max_st = false;
			ret = ILITEK_FAIL;
		}
    }
    return ret;
}

int32_t ilitek_open_test_v6(int32_t *open, int32_t *tx_avg, int32_t *rx_diff)
{
	int32_t ret = ILITEK_SUCCESS, index = 0;
	//uint8_t *buf_recv = NULL;
	uint8_t inbuf[2] = {0};
	int open_st = ILITEK_SUCCESS, rxdiff_st = ILITEK_SUCCESS, txavg_st = ILITEK_SUCCESS;

	tp_log_info("\n");
	open_test_result = 0;
	inbuf[1] = ENTER_TEST_MODE;
	if (api_protocol_set_cmd(ILITEK_TP_CMD_SET_MODE_CONTORL, inbuf, NULL) < 0) {
		return ILITEK_FAIL;
	}
	//initial
	if(api_set_open_Info(ilitek_data->mp.m_open_freqL, ilitek_data->mp.m_open_freqH,
		ilitek_data->mp.m_open_gain) < ILITEK_SUCCESS) {
		return ILITEK_FAIL;
	}
	if(ilitek_inital_rawdata_v6(TEST_MODE_V6_OPEN) == ILITEK_SUCCESS) {
		if (ilitek_get_rawdata_v6(ilitek_data->x_ch * ilitek_data->y_ch * 2, open) != ILITEK_FAIL)
		{
			ret = ILITEK_SUCCESS;
		}
		else
		{
			tp_log_err("Error! Get RawData Failed!\n");
			return ILITEK_FAIL;
		}
	}
	else
	{
		tp_log_err("Error! Init RawData Failed!\n");
		return ILITEK_FAIL;
	}
	msleep(100);
	if(ilitek_data->mp.open_tx_avg > 0) {
		//initial tx average threshold buffer.
		for(index = 0; index < ilitek_data->y_ch - 1; index++)
			ilitek_data->mp.txavg[index].max = ilitek_data->mp.open_tx_avg;
		if(ilitek_open_avg(open, tx_avg) < ILITEK_SUCCESS) {
			txavg_st = ILITEK_FAIL;
		}
		tp_log_info("Open Test Tx Average:%s, result:%d\n", txavg_st ? "FAIL" : "PASS", txavg_st);
	}
	if(ilitek_data->mp.open_rx_diff > 0) {
		//initial tx diff threshold buffer.
		for(index = 0; index < (ilitek_data->x_ch - 1) * ilitek_data->y_ch; index++)
			ilitek_data->mp.rxdiff[index].max = ilitek_data->mp.open_rx_diff;
		if(ilitek_open_rxdiff(open, rx_diff) < ILITEK_SUCCESS) {
			rxdiff_st = ILITEK_FAIL;
		}
		tp_log_info("Open Test Rx Diff:%s, result:%d\n", rxdiff_st ? "FAIL" : "PASS", rxdiff_st);
	}
	inbuf[1] = ENTER_NORMAL_MODE;
	if (api_protocol_set_cmd(ILITEK_TP_CMD_SET_MODE_CONTORL, inbuf, NULL) < 0) {
		return ILITEK_FAIL;
	}
	for(index = 0; index < ilitek_data->x_ch * ilitek_data->y_ch; index++)
	{
		if(open[index] < ilitek_open_threshold)
			open_st = ILITEK_FAIL;
	}
	if(open_st != ILITEK_SUCCESS || rxdiff_st != ILITEK_SUCCESS || txavg_st != ILITEK_SUCCESS)
		open_test_result = ILITEK_FAIL;
	msleep(100);
	return open_test_result;
}

int32_t ilitek_mopen_test_v6(int32_t *open, int32_t *tx_avg, int32_t *rx_diff)
{
	int32_t ret = ILITEK_SUCCESS;
	uint8_t inbuf[2] = {0};
	int rxdiff_st = ILITEK_SUCCESS, txavg_st = ILITEK_SUCCESS;

	tp_log_info("\n");
	mopen_test_result = ILITEK_SUCCESS;
	//If have excuted open test don't need get raw data.
	if(ilitek_data->mp.open != true) {
		inbuf[1] = ENTER_TEST_MODE;
		if (api_protocol_set_cmd(ILITEK_TP_CMD_SET_MODE_CONTORL, inbuf, NULL) < 0) {
			return ILITEK_FAIL;
		}
		//initial
		if(api_set_open_Info(ilitek_data->mp.m_open_freqL, ilitek_data->mp.m_open_freqH,
			ilitek_data->mp.m_open_gain) < ILITEK_SUCCESS) {
			return ILITEK_FAIL;
		}
		if(ilitek_inital_rawdata_v6(TEST_MODE_V6_OPEN) == ILITEK_SUCCESS) {
			if (ilitek_get_rawdata_v6(ilitek_data->x_ch * ilitek_data->y_ch * 2, open) != ILITEK_FAIL)
			{
				ret = ILITEK_SUCCESS;
			}
			else
			{
				tp_log_err("Error! Get RawData Failed!\n");
				return ILITEK_FAIL;
			}
		}
		else
		{
			tp_log_err("Error! Init RawData Failed!\n");
			return ILITEK_FAIL;
		}
		inbuf[1] = ENTER_NORMAL_MODE;
		if (api_protocol_set_cmd(ILITEK_TP_CMD_SET_MODE_CONTORL, inbuf, NULL) < 0) {
			return ILITEK_FAIL;
		}
	}

	if(ilitek_data->mp.m_open_tx) {
		//initial
		ilitek_data->mp.open_tx_avg_corner = ilitek_data->mp.m_open_tx_avg_corner;
		if(ilitek_open_avg(open, tx_avg) < ILITEK_SUCCESS) {
			txavg_st = ILITEK_FAIL;
		}
		tp_log_info("Micro Open Test Tx Average:%s, result:%d\n", txavg_st ? "FAIL" : "PASS", txavg_st);
	}
	if(ilitek_data->mp.m_open_rx) {
		//initial
		ilitek_data->mp.open_rx_diff_fcnt = ilitek_data->mp.m_open_rx_diff_fcnt;
		if(ilitek_open_rxdiff(open, rx_diff) < ILITEK_SUCCESS) {
			rxdiff_st = ILITEK_FAIL;
		}
		tp_log_info("Micro Open Test Rx Diff:%s, result:%d\n", rxdiff_st ? "FAIL" : "PASS", rxdiff_st);
	}

	if(rxdiff_st != ILITEK_SUCCESS || txavg_st != ILITEK_SUCCESS)
		mopen_test_result = ILITEK_FAIL;
	msleep(100);
	return mopen_test_result;
}

int32_t ilitek_allnode_test_v6(int32_t *raw_data, int32_t *win1 , int32_t *win2) {
	int32_t ret = ILITEK_SUCCESS, index = 0;
	int32_t win1_fcnt = 0;	//win1 fail count
	int32_t win2_fcnt = 0;	//win2 fail conut

	tp_log_info("\n");
	ilitek_data->mp.uni.allnode_raw_status = true;
	ilitek_data->mp.uni.allnode_win1_status = true;
	ilitek_data->mp.uni.allnode_win2_status = true;
	if(ilitek_data->mp.uni.allnode_raw) {
		for(index = 0; index < ilitek_data->x_ch * ilitek_data->y_ch; index++)
		{
			ilitek_data->mp.uni.raw[index].max_st = true;
			ilitek_data->mp.uni.raw[index].min_st = true;

			if(raw_data[index] > ilitek_data->mp.uni.raw[index].max && ilitek_data->mp.uni.raw[index].type != 0) {
				tp_log_info("[%d][%d] raw_data:%d max%d\n", index / (ilitek_data->x_ch), index % (ilitek_data->x_ch)
				, raw_data[index], ilitek_data->mp.uni.raw[index].max);
				ilitek_data->mp.uni.raw[index].max_st = false;
				ilitek_data->mp.uni.allnode_raw_status = false;
			}
			if(raw_data[index] < ilitek_data->mp.uni.raw[index].min && ilitek_data->mp.uni.raw[index].type != 0) {
				tp_log_info("[%d][%d] raw_data:%d min%d\n", index / (ilitek_data->x_ch), index % (ilitek_data->x_ch)
				, raw_data[index], ilitek_data->mp.uni.raw[index].min);
				ilitek_data->mp.uni.raw[index].min_st = false;
				ilitek_data->mp.uni.allnode_raw_status = false;
			}
		}
		if(ilitek_data->mp.uni.allnode_raw_status == false)
			tp_log_info("all node raw test: FAIL\n");
		else
			tp_log_info("all node raw test: PASS\n");
	}
	if(ilitek_data->mp.uni.allnode_win1) {
		for(index = 0; index < ilitek_data->x_ch * (ilitek_data->y_ch - 1); index++)
		{
			ilitek_data->mp.uni.win1[index].max_st = true;
			ilitek_data->mp.uni.win1[index].min_st = true;

			if(win1[index] > ilitek_data->mp.uni.win1[index].max && ilitek_data->mp.uni.win1[index].type != 0) {
				tp_log_info("[%d][%d] win1:%d max%d\n", index / (ilitek_data->x_ch), index % (ilitek_data->x_ch)
				, win1[index], ilitek_data->mp.uni.win1[index].max);
				win1_fcnt++;
				ilitek_data->mp.uni.win1[index].max_st = false;
			}
			if(win1[index] < ilitek_data->mp.uni.win1[index].min && ilitek_data->mp.uni.win1[index].type != 0) {
				tp_log_info("[%d][%d] win1:%d min%d\n", index / (ilitek_data->x_ch), index % (ilitek_data->x_ch)
				, raw_data[index], ilitek_data->mp.uni.win1[index].min);
				win1_fcnt++;
				ilitek_data->mp.uni.win1[index].min_st = false;
			}
		}
		if(win1_fcnt > ilitek_win1_failpoint)
			ilitek_data->mp.uni.allnode_win1_status = false;
		if(ilitek_data->mp.uni.allnode_win1_status == false)
			tp_log_info("all node win1 test: FAIL\n");
		else
			tp_log_info("all node win1 test: PASS\n");
	}
	if(ilitek_data->mp.uni.allnode_win2) {
		for(index = 0; index < (ilitek_data->x_ch - 1) * (ilitek_data->y_ch - 1); index++)
		{
			ilitek_data->mp.uni.win2[index].max_st = true;
			ilitek_data->mp.uni.win2[index].min_st = true;

			if(win2[index] > ilitek_data->mp.uni.win2[index].max && ilitek_data->mp.uni.win2[index].type != 0) {
				tp_log_info("[%d][%d] win2:%d max%d\n", index / (ilitek_data->x_ch - 1), index % (ilitek_data->x_ch - 1)
				, win2[index], ilitek_data->mp.uni.win2[index].max);
				win2_fcnt++;
				ilitek_data->mp.uni.win2[index].max_st = false;
			}
			if(win2[index] < ilitek_data->mp.uni.win2[index].min && ilitek_data->mp.uni.win2[index].type != 0) {
				tp_log_info("[%d][%d] win2:%d min%d\n", index / (ilitek_data->x_ch - 1), index % (ilitek_data->x_ch - 1)
				, win2[index], ilitek_data->mp.uni.win2[index].min);
				win2_fcnt++;
				ilitek_data->mp.uni.win2[index].min_st = false;
			}
		}
		if(win2_fcnt > ilitek_win2_failpoint)
			ilitek_data->mp.uni.allnode_win2_status = false;
		if(ilitek_data->mp.uni.allnode_win2_status == false)
			tp_log_info("all node win2 test: FAIL\n");
		else
			tp_log_info("all node win2 test: PASS\n");
	}

	if(ilitek_data->mp.uni.allnode_raw_status == false ||
	   ilitek_data->mp.uni.allnode_win1_status == false ||
	   ilitek_data->mp.uni.allnode_win2_status == false) {
		ret = ILITEK_FAIL;
	}
	tp_log_info("\n");
	return ret;
}

static int32_t ilitek_benchmark_test_v6(int32_t *raw_data) {
	int32_t ret = ILITEK_SUCCESS, index = 0;

	ilitek_data->mp.uni.bench_status = true;

	for(index = 0; index < ilitek_data->x_ch * ilitek_data->y_ch; index++)
	{
		ilitek_data->mp.uni.ben[index].max_st = true;
		ilitek_data->mp.uni.ben[index].min_st = true;
		if(raw_data[index] > ilitek_data->mp.uni.ben[index].max && ilitek_data->mp.uni.ben[index].type != 0) {
			ilitek_data->mp.uni.bench_status = false;
			ilitek_data->mp.uni.ben[index].max_st = false;
		}
		if(raw_data[index] < ilitek_data->mp.uni.ben[index].min && ilitek_data->mp.uni.ben[index].type != 0) {
			ilitek_data->mp.uni.bench_status = false;
			ilitek_data->mp.uni.ben[index].min_st = false;
		}
	}
    tp_log_info("max:%d min:%d\n", ilitek_data->mp.uni.ben[0].max_st, ilitek_data->mp.uni.ben[0].min_st);
	if(ilitek_data->mp.uni.bench_status == false) {
        ret = ILITEK_FAIL;
        tp_log_info("Uniformity benchmark test: FAIL\n");
    }
    else
        tp_log_info("Uniformity benchmark test: PASS\n");

	return ret;	
}

int32_t ilitek_uniformity_test_v6(int32_t *allnode , int32_t *win1 , int32_t *win2)
{
	int32_t ret = 0, index = 0;
	int32_t i = 0,j = 0;
	int32_t win1count = 0, win2count = 0;
	//uint8_t *buf_recv = NULL;
	uint8_t inbuf[2] = {0};
	int32_t uni_result = ILITEK_SUCCESS;

	tp_log_info("\n");
	allnode_test_result = 0;
	inbuf[1] = ENTER_TEST_MODE;
	if (api_protocol_set_cmd(ILITEK_TP_CMD_SET_MODE_CONTORL, inbuf, NULL) < 0) {
		return ILITEK_FAIL;
	}
	//initial
	if(ilitek_inital_rawdata_v6(TEST_MODE_V6_MC_RAW_NBK) == ILITEK_SUCCESS)
	{
		if (ilitek_get_rawdata_v6(ilitek_data->x_ch * ilitek_data->y_ch * 2, allnode) != ILITEK_FAIL)
		{
			ret = ILITEK_SUCCESS;
		}
		else
		{
			tp_log_err("Error! Get RawData Failed!\n");
			return ILITEK_FAIL;
		}
	}
	else
	{
		tp_log_err("Error! Init RawData Failed!\n");
		return ILITEK_FAIL;
	}
	inbuf[1] = ENTER_NORMAL_MODE;
	if (api_protocol_set_cmd(ILITEK_TP_CMD_SET_MODE_CONTORL, inbuf, NULL) < 0) {
		return ILITEK_FAIL;
	}
	/*for(index = ;0 index < ilitek_data->x_ch * (ilitek_data->y_ch - 1); index++)
	{
		win1[index] = abs(allnode[index] - allnode[index + ilitek_data->x_ch]);
	}*/

	for(j = 0; j < ilitek_data->y_ch -1; j++)
	{
		for(i = 0; i < ilitek_data->x_ch; i++)
		{
			win1[j * ilitek_data->x_ch + i] = abs(allnode[j * ilitek_data->x_ch + i] - allnode[(j + 1) * ilitek_data->x_ch + i]);
			if(ilitek_data->mp.pf_ver < INI_PROFILE_V1_0_2_0) {
				if( win1[j * ilitek_data->x_ch + i] > ilitek_allnodetestw1)
					win1count++;
				if(win1count > ilitek_win1_failpoint)
					allnode_test_result = ILITEK_FAIL;
				printk("%4d,", win1[j * ilitek_data->x_ch + i]);
			}

		}
		if(ilitek_data->mp.pf_ver < INI_PROFILE_V1_0_2_0)
			printk("\n");
	}
	printk("\n");
tp_log_info("\n");
	for(j = 0; j < ilitek_data->y_ch - 1; j++)
	{
		for(i = 0; i < ilitek_data->x_ch -1; i++)
		{
			win2[j * (ilitek_data->x_ch - 1) + i] = abs(win1[j * ilitek_data->x_ch + i] - win1[j * ilitek_data->x_ch + i +1]);
			if(ilitek_data->mp.pf_ver < INI_PROFILE_V1_0_2_0) {
				if( win2[j * (ilitek_data->x_ch - 1) + i] > ilitek_allnodetestw2)
					win2count++;
				if(win2count > ilitek_win2_failpoint)
					allnode_test_result = ILITEK_FAIL;
				printk("%4d,",win2[j * (ilitek_data->x_ch - 1) + i]);
			}
		}
		if(ilitek_data->mp.pf_ver < INI_PROFILE_V1_0_2_0) 
			printk("\n");
	}
	printk("\n");
tp_log_info("\n");
	if(ilitek_data->mp.pf_ver < INI_PROFILE_V1_0_2_0) {
		for(index = 0; index < ilitek_data->x_ch * ilitek_data->y_ch; index++)
		{
			if(allnode[index] > ilitek_allnode_max_threshold)
				allnode_test_result = ILITEK_FAIL;
			if(allnode[index] < ilitek_allnode_min_threshold)
				allnode_test_result = ILITEK_FAIL;
		}
	}
tp_log_info("\n");
	if(ilitek_data->mp.uni.bench) {
		uni_result = ilitek_benchmark_test_v6(allnode);
		if(uni_result < 0)
			allnode_test_result = uni_result;
	    tp_log_info("max:%d min:%d\n", ilitek_data->mp.uni.ben[0].max_st, ilitek_data->mp.uni.ben[0].min_st);
	}
	if(ilitek_data->mp.uni.allnode_raw || ilitek_data->mp.uni.allnode_win1 || ilitek_data->mp.uni.allnode_win2) {
		uni_result = ilitek_allnode_test_v6(allnode, win1, win2);
		if(uni_result < 0) {
            allnode_test_result = uni_result;
            tp_log_info("all node test: fail\n");
        }
        else
            tp_log_info("all node test: pass\n");
	}
	msleep(100);
	return allnode_test_result;
}