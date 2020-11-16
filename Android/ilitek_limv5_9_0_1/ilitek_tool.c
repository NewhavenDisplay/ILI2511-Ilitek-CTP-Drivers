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
#ifdef ILITEK_UPDATE_FW
extern int CTPM_FW_size;
#endif
#ifdef ILITEK_TOOL
// device data
struct dev_data {
	// device number
	dev_t devno;
	// character device
	struct cdev cdev;
	// class device
	struct class *class;
};

static struct dev_data ilitek_dev;
static struct proc_dir_entry *ilitek_proc;
static struct proc_dir_entry *ilitek_proc_entry;
#ifdef ILITEK_TUNING_NODE
static struct proc_dir_entry *ilitek_proc_debug;
static struct proc_dir_entry *ilitek_proc_debug_switch;
#endif
static struct proc_dir_entry *ilitek_proc_irq_enable;
uint32_t set_update_len = UPGRADE_LENGTH_BLV1_8;
//static char ilitek_hex_path[256] = "/data/local/tmp/ilitek.hex";

// define the application command
#define ILITEK_IOCTL_BASE                       100
#define ILITEK_IOCTL_I2C_WRITE_DATA             _IOWR(ILITEK_IOCTL_BASE, 0, uint8_t*)
#define ILITEK_IOCTL_I2C_WRITE_LENGTH           _IOWR(ILITEK_IOCTL_BASE, 1, int32_t)
#define ILITEK_IOCTL_I2C_READ_DATA              _IOWR(ILITEK_IOCTL_BASE, 2, uint8_t*)
#define ILITEK_IOCTL_I2C_READ_LENGTH            _IOWR(ILITEK_IOCTL_BASE, 3, int32_t)
#define ILITEK_IOCTL_USB_WRITE_DATA             _IOWR(ILITEK_IOCTL_BASE, 4, uint8_t*)
#define ILITEK_IOCTL_USB_WRITE_LENGTH           _IOWR(ILITEK_IOCTL_BASE, 5, int32_t)
#define ILITEK_IOCTL_USB_READ_DATA              _IOWR(ILITEK_IOCTL_BASE, 6, uint8_t*)
#define ILITEK_IOCTL_USB_READ_LENGTH            _IOWR(ILITEK_IOCTL_BASE, 7, int32_t)
#define ILITEK_IOCTL_DRIVER_INFORMATION		    _IOWR(ILITEK_IOCTL_BASE, 8, int32_t)
#define ILITEK_IOCTL_USB_UPDATE_RESOLUTION      _IOWR(ILITEK_IOCTL_BASE, 9, int32_t)
#define ILITEK_IOCTL_I2C_INT_FLAG	           	_IOWR(ILITEK_IOCTL_BASE, 10, int32_t)
#define ILITEK_IOCTL_I2C_UPDATE                 _IOWR(ILITEK_IOCTL_BASE, 11, int32_t)
#define ILITEK_IOCTL_STOP_READ_DATA             _IOWR(ILITEK_IOCTL_BASE, 12, int32_t)
#define ILITEK_IOCTL_START_READ_DATA            _IOWR(ILITEK_IOCTL_BASE, 13, int32_t)
#define ILITEK_IOCTL_GET_INTERFANCE				_IOWR(ILITEK_IOCTL_BASE, 14, int32_t)	//default setting is i2c interface
#define ILITEK_IOCTL_I2C_SWITCH_IRQ				_IOWR(ILITEK_IOCTL_BASE, 15, int32_t)
#define ILITEK_IOCTL_UPDATE_FLAG				_IOWR(ILITEK_IOCTL_BASE, 16, int32_t)
#define ILITEK_IOCTL_I2C_UPDATE_FW				_IOWR(ILITEK_IOCTL_BASE, 18, int32_t)
#define ILITEK_IOCTL_RESET						_IOWR(ILITEK_IOCTL_BASE, 19, int32_t)
#define ILITEK_IOCTL_INT_STATUS					_IOWR(ILITEK_IOCTL_BASE, 20, int32_t)
#ifdef ILITEK_TUNING_MESSAGE
extern bool ilitek_debug_flag;

#define ILITEK_IOCTL_DEBUG_SWITCH				_IOWR(ILITEK_IOCTL_BASE, 21, int32_t)
#endif

#ifdef ILITEK_TUNING_NODE
#define ILITEK_IOCTL_DEBUG_NODE_SWITCH			_IOWR(ILITEK_IOCTL_BASE, 65, int32_t)
#endif
#define ILITEK_DEVICE_NODE_PERMISSON			0755

static int32_t ilitek_file_open(struct inode *inode, struct file *filp)
{
	ilitek_data->operation_protection = true;
	tp_log_info("operation_protection = %d\n", ilitek_data->operation_protection);
	return 0;
}

static ssize_t ilitek_file_write(struct file *filp, const char *buf, size_t size, loff_t *f_pos)
{
	int32_t ret, count = 0;
	uint8_t buffer[512] = { 0 };
	uint32_t *data;
	char *token = NULL, *cur = NULL;

	// check the buffer size whether it exceeds the local buffer size or not
	if (count > 512) {
		tp_log_err("buffer exceed 512 bytes\n");
		//return -1;
	}
	// copy data from user space
	ret = copy_from_user(buffer, buf, size - 1);
	if (ret < 0) {
		tp_log_err("copy data from user space, failed");
		return -1;
	}

	token = cur = buffer;
	
	data = kcalloc(size, sizeof(u32), GFP_KERNEL);

	while ((token = strsep(&cur, ",")) != NULL) {
		//data[count] = str2hex(token);
		sscanf(token,"%x", &data[count]);
		tp_log_info("data[%d] = %x\n", count, data[count]);
		count++;
	}

	if (buffer[size - 2] == 'I' && (size == 20 || size == 52) && buffer[0] == 0x77 && buffer[1] == 0x77) {

		tp_log_info("IOCTL_WRITE CMD = %d\n", buffer[2]);
		switch (buffer[2]) {
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
			tp_log_info("ilitek The ilitek_debug_flag = %d.\n", buffer[3]);
			if (buffer[3] == 0) {
				ilitek_debug_flag = false;
			} else if (buffer[3] == 1) {
				ilitek_debug_flag = true;
			}
			break;
#endif
		case 15:
			if (buffer[3] == 0) {
				ilitek_irq_disable();
				tp_log_info("ilitek_irq_disable.\n");
			} else {
				ilitek_irq_enable();
				tp_log_info("ilitek_irq_enable.\n");
			}
			break;
		case 16:
			ilitek_data->operation_protection = buffer[3];
			tp_log_info("ilitek_data->operation_protection = %d\n", ilitek_data->operation_protection);
			break;
		case 18:
			ret = ilitek_i2c_write(&buffer[3], 33);
			if (ret < 0) {
				tp_log_err("i2c write error, ret %d, addr %x\n", ret, ilitek_data->client->addr);
				return ret;
			}
			return 0;
			break;
		default:
			return -1;
		}
	}
	if (buffer[size - 2] == 'W') {
		ret = ilitek_i2c_write(buffer, size - 2);
		if (ret < 0) {
			tp_log_err("i2c write error, ret %d, addr %x\n", ret, ilitek_data->client->addr);
			return ret;
		}
	} else if (strcmp(buffer, "unhandle_irq") == 0) {
		ilitek_data->unhandle_irq = !ilitek_data->unhandle_irq;
		tp_log_info("ilitek_data->unhandle_irq = %d.\n", ilitek_data->unhandle_irq);
	} else if (strcmp(buffer, "dbg_debug") == 0) {
		ilitek_data->ilitek_log_level_value = ILITEK_DEBUG_LOG_LEVEL;
		tp_log_info("ilitek_log_level_value = %d.\n", ilitek_data->ilitek_log_level_value);
	} else if (strcmp(buffer, "dbg_info") == 0) {
		ilitek_data->ilitek_log_level_value = ILITEK_INFO_LOG_LEVEL;
		tp_log_info("ilitek_log_level_value = %d.\n", ilitek_data->ilitek_log_level_value);
	} else if (strcmp(buffer, "dbg_err") == 0) {
		ilitek_data->ilitek_log_level_value = ILITEK_ERR_LOG_LEVEL;
		tp_log_info("ilitek_log_level_value = %d.\n", ilitek_data->ilitek_log_level_value);
	}
 else if (strcmp(buffer, "dbg_num") == 0) {
		tp_log_info("ilitek_log_level_value = %d.\n", ilitek_data->ilitek_log_level_value);
	}
#ifdef ILITEK_TUNING_MESSAGE
	else if (strcmp(buffer, "truning_dbg_flag") == 0) {
		ilitek_debug_flag = !ilitek_debug_flag;
		tp_log_info(" %s debug_flag message(%X).\n", ilitek_debug_flag ? "Enabled" : "Disabled", ilitek_debug_flag);
	}
#endif
	else if (strcmp(buffer, "irq_status") == 0) {
		tp_log_info("gpio_get_value(i2c.irq_gpio) = %d.\n", gpio_get_value(ilitek_data->irq_gpio));
	} else if (strcmp(buffer, "enable") == 0) {
		enable_irq(ilitek_data->client->irq);
		tp_log_info("irq enable\n");
	} else if (strcmp(buffer, "disable") == 0) {
		disable_irq(ilitek_data->client->irq);
		tp_log_info("irq disable\n");
	} else if (strcmp(buffer, "info") == 0) {
		ilitek_read_tp_info(false);
	} else if (strcmp(buffer, "reset") == 0) {
		ilitek_reset(ilitek_data->reset_time, false);
	} else if (strcmp(buffer, "update_protocol") == 0) {
		ret = api_protocol_init_func();
	}
	else if (strncmp(buffer, "setlen,", 6) == 0) {
		set_update_len = data[1];
		tp_log_info( "set_update_len=%d\n", set_update_len);
	}

	tp_log_debug("ilitek return count = %d\n", (int32_t)size);
	kfree(data);
	return size;
}

/*
   description
   ioctl function for character device driver
   prarmeters
   inode
   file node
   filp
   file pointer
   cmd
   command
   arg
   arguments
   return
   status
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
static long ilitek_file_ioctl(struct file *filp, uint32_t cmd, unsigned long arg)
#else
static int32_t ilitek_file_ioctl(struct inode *inode, struct file *filp, uint32_t cmd, unsigned long arg)
#endif
{
	static uint8_t *buffer; //[64] = { 0 };
	static int32_t len = 0, i;
	int32_t ret = ILITEK_SUCCESS;
	// parsing ioctl command
	buffer = kmalloc(ILITEK_IOCTL_MAX_TRANSFER, GFP_KERNEL);
	memset(buffer, 0 , ILITEK_IOCTL_MAX_TRANSFER);
	switch (cmd) {
	case ILITEK_IOCTL_I2C_WRITE_DATA:
		ret = copy_from_user(buffer, (uint8_t *)arg, len);
		if (ret < 0) {
			tp_log_err("copy data from user space, failed\n");
			ret = ILITEK_FAIL;
			goto error;
		}
		if (ilitek_i2c_write_and_read(buffer, len, 0, NULL, 0) < 0) {
			tp_log_err("i2c write, failed\n");
			ret = ILITEK_FAIL;
			goto error;
		}
		break;
	case ILITEK_IOCTL_I2C_READ_DATA:
		if (ilitek_i2c_write_and_read(NULL, 0, 0, buffer, len) < 0) {
			tp_log_err("i2c read, failed\n");
			ret = ILITEK_FAIL;
			goto error;
		}
		ret = copy_to_user((uint8_t *)arg, buffer, len);
		if (ret < 0) {
			tp_log_err("copy data to user space, failed\n");
			ret = ILITEK_FAIL;
			goto error;
		}
		break;
	case ILITEK_IOCTL_I2C_WRITE_LENGTH:
	case ILITEK_IOCTL_I2C_READ_LENGTH:
		len = arg;
		break;
	case ILITEK_IOCTL_DRIVER_INFORMATION:
		for (i = 0; i < 7; i++) {
			buffer[i] = ilitek_driver_information[i];
		}
		ret = copy_to_user((uint8_t *)arg, buffer, 7);
		break;
	case ILITEK_IOCTL_I2C_UPDATE:
		break;
	case ILITEK_IOCTL_I2C_INT_FLAG:
		buffer[0] = !(gpio_get_value(ilitek_data->irq_gpio));
		ret = copy_to_user((uint8_t *)arg, buffer, 1);
		if (ret < 0) {
			tp_log_err("copy data to user space, failed\n");
			ret = ILITEK_FAIL;
			goto error;
		}
		tp_log_debug("ILITEK_IOCTL_I2C_INT_FLAG = %d.\n", buffer[0]);
		break;
	case ILITEK_IOCTL_START_READ_DATA:
		ilitek_irq_enable();
		ilitek_data->unhandle_irq = false;
		tp_log_info("ILITEK_IOCTL_START_READ_DATA do enable irq and ilitek_data->unhandle_irq = false.\n");
		break;
	case ILITEK_IOCTL_STOP_READ_DATA:
		ilitek_irq_disable();
		ilitek_data->unhandle_irq = true;
		tp_log_info("ILITEK_IOCTL_STOP_READ_DATA do disable irq and ilitek_data->unhandle_irq = true.\n");
		break;
	case ILITEK_IOCTL_RESET:
		ilitek_reset(ilitek_data->reset_time, false);
		break;
	case ILITEK_IOCTL_INT_STATUS:
		put_user(gpio_get_value(ilitek_data->irq_gpio), (int32_t *)arg);
		break;
#ifdef ILITEK_TUNING_MESSAGE
	case ILITEK_IOCTL_DEBUG_SWITCH:
		ret = copy_from_user(buffer, (uint8_t *)arg, 1);
		tp_log_info("ilitek The debug_flag = %d.\n", buffer[0]);
		if (buffer[0] == 0) {
			ilitek_debug_flag = false;
		} else if (buffer[0] == 1) {
			ilitek_debug_flag = true;
		}
		break;
#endif
	case ILITEK_IOCTL_I2C_SWITCH_IRQ:
		ret = copy_from_user(buffer, (uint8_t *)arg, 1);
		if (buffer[0] == 0) {
			tp_log_info("ilitek_i2c_irq_disable .\n");
			ilitek_irq_disable();
		} else {
			tp_log_info("ilitek_i2c_irq_enable.\n");
			ilitek_irq_enable();
		}
		break;
	case ILITEK_IOCTL_UPDATE_FLAG:
		ilitek_data->operation_protection = arg;
		tp_log_info("operation_protection = %d\n", ilitek_data->operation_protection);
		break;
	case ILITEK_IOCTL_I2C_UPDATE_FW:
		ret = copy_from_user(buffer, (uint8_t *)arg, 35);
		if (ret < 0) {
			tp_log_err("copy data from user space, failed\n");
			ret = ILITEK_FAIL;
			goto error;
		}
		if (ilitek_i2c_write_and_read(buffer, buffer[34], 0, NULL, 0) < 0) {
			tp_log_err("i2c write, failed\n");
			ret = ILITEK_FAIL;
			goto error;
		}
		break;

	default:
			ret = ILITEK_FAIL;
			goto error;
	}
error:
	kfree(buffer);
	return 0;
}

/*
   description
   read function for character device driver
   prarmeters
   filp
   file pointer
   buf
   buffer
   count
   buffer length
   f_pos
   offset
   return
   status
 */
static ssize_t ilitek_file_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	uint8_t *tmp;
	int32_t ret;
	long rc;

	//tp_log_info("%s enter count = %d\n", __func__, count);

	if (count > 8192)
		count = 8192;

	tmp = kmalloc(count, GFP_KERNEL);
	if (tmp == NULL)
		return -ENOMEM;

	ret = ilitek_i2c_read(tmp, count);
	if (ret < 0) {
		tp_log_err("i2c read error, ret %d,addr %x\n", ret, ilitek_data->client->addr);
	}
	rc = copy_to_user(buf, tmp, count);

	kfree(tmp);
	tmp = NULL;
	return ret > 0 ? count : ret;
}

/*
   description
   close function
   prarmeters
   inode
   inode
   filp
   file pointer
   return
   status
 */
static int32_t ilitek_file_close(struct inode *inode, struct file *filp)
{
	ilitek_data->operation_protection = false;
	tp_log_info("operation_protection = %d\n", ilitek_data->operation_protection);
	return 0;
}

// declare file operations
static struct file_operations ilitek_fops = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	.unlocked_ioctl = ilitek_file_ioctl,
#else
	.ioctl = ilitek_file_ioctl,
#endif
	.read = ilitek_file_read,
	.write = ilitek_file_write,
	.open = ilitek_file_open,
	.release = ilitek_file_close,
};

static ssize_t ilitek_irq_enable_read(struct file *pFile, char __user *buf, size_t nCount, loff_t *pPos)
{
	int32_t ret = 0;
	uint8_t tmpbuf[128] = { 0 };
	tp_log_info("\n");
	if (*pPos != 0) {
		return 0;
	}
	tp_log_info("irq trigger count %d, irq handle count %d\n", ilitek_data->irq_trigger_count, ilitek_data->irq_handle_count);
	nCount = scnprintf(tmpbuf, PAGE_SIZE, "irq trigger count %d, irq handle count %d\n", ilitek_data->irq_trigger_count, ilitek_data->irq_handle_count);
	ret = copy_to_user(buf, tmpbuf, nCount);
	*pPos += nCount;

#ifdef MTK_UNDTS
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#else
	enable_irq(ilitek_data->client->irq);
#endif
	return nCount;
}

static struct file_operations ilitek_irq_enable_fops = {
	.read = ilitek_irq_enable_read,
	.write = NULL,
};

#ifdef ILITEK_TUNING_NODE
static ssize_t ilitek_debug_switch_read(struct file *pFile, char __user *buf, size_t nCount, loff_t *pPos)
{
	int32_t ret = 0;
	uint8_t tmpbuf[128] = { 0 };
	tp_log_info("\n");
	if (*pPos != 0) {
		return 0;
	}
	ilitek_data->debug_node_open = !ilitek_data->debug_node_open;
	tp_log_info(" %s debug_flag message(%X).\n", ilitek_data->debug_node_open ? "Enabled" : "Disabled", ilitek_data->debug_node_open);
	nCount = scnprintf(tmpbuf, PAGE_SIZE, "ilitek_data->debug_node_open %s\n", ilitek_data->debug_node_open ? "Enabled" : "Disabled");
	ret = copy_to_user(buf, tmpbuf, nCount);
	*pPos += nCount;
	return nCount;
}

static struct file_operations ilitek_debug_fops_switch = {
	.read = ilitek_debug_switch_read,
	.write = NULL,
};

static int32_t ilitek_debug_file_open(struct inode *inode, struct file *filp)
{
	tp_log_info("\n");
	return 0;
}

static ssize_t ilitek_debug_file_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	uint8_t buffer[512] = { 0 };
	int32_t ret = 0;
	tp_log_debug("\n");
	// check the buffer size whether it exceeds the local buffer size or not
	if (count > 512) {
		tp_log_err("buffer exceed 512 bytes\n");
		//return -1;
	}
	// copy data from user space
	ret = copy_from_user(buffer, buf, count - 1);
	if (ret < 0) {
		tp_log_err("copy data from user space, failed");
		return -1;
	}
	if (strcmp(buffer, "dbg_flag") == 0) {
		ilitek_data->debug_node_open = !ilitek_data->debug_node_open;
		tp_log_info(" %s debug_flag message(%X).\n", ilitek_data->debug_node_open ? "Enabled" : "Disabled", ilitek_data->debug_node_open);
	}
	return count;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
static long ilitek_file_debug_ioctl(struct file *filp, uint32_t cmd, unsigned long arg)
#else
static int32_t ilitek_file_debug_ioctl(struct inode *inode, struct file *filp, uint32_t cmd, unsigned long arg)
#endif
{
	static uint8_t buffer[64] = { 0 };
	int32_t ret;
	// parsing ioctl command
	switch (cmd) {
	case ILITEK_IOCTL_DEBUG_NODE_SWITCH:
		ret = copy_from_user(buffer, (uint8_t *)arg, 1);
		if (ret < 0) {
			tp_log_err("copy data from user space, failed");
		} else {
			tp_log_info("ilitek The debug_flag = %d.\n", buffer[0]);
			if (buffer[0] == 0) {
				ilitek_data->debug_node_open = false;
			} else if (buffer[0] == 1) {
				ilitek_data->debug_node_open = true;
			}
		}
		break;
	default:
		return -1;
	}
	return 0;
}

static uint32_t ilitek_debug_file_poll(struct file *filp, poll_table *wait)
{
	uint32_t mask = 0;

	poll_wait(filp, &ilitek_data->inq, wait);

	if (ilitek_data->debug_data_frame > 0) {
		mask |= POLLIN | POLLRDNORM;	/* readable */
	}
	return mask;
}

static loff_t ilitek_debug_file_llseek(struct file *filp, loff_t offset, int32_t whence)
{
	loff_t newpos = -EINVAL;;
	tp_log_debug("\n");
	switch (whence) {
	case 0:		/* SEEK_SET */
		newpos = offset;
		break;

	case 1:		/* SEEK_CUR */
		newpos = filp->f_pos + offset;
		break;

	case 2:		/* SEEK_END */
		//newpos = MEMDEV_SIZE -1 + offset;
		break;

	default:		/* can't happen */
		return -EINVAL;
	}
	//if ((newpos<0) || (newpos>MEMDEV_SIZE))
	//    return -EINVAL;

	filp->f_pos = newpos;
	return newpos;

}

static ssize_t ilitek_debug_file_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
	unsigned long p = *ppos;
	uint32_t count = size;
	uint8_t tmpbuf[64] = { 0 };
	int32_t i = 0;
	size_t send_data_len = 0;
	size_t ret = 0;
	//tp_log_info("size = %zu, p = %lu\n", size, p);
	if (*ppos != 0) {
		//return 0;
	}
	while (ilitek_data->debug_data_frame <= 0) {
		if (filp->f_flags & O_NONBLOCK) {
			return -EAGAIN;
		}
		wait_event_interruptible(ilitek_data->inq, ilitek_data->debug_data_frame > 0);
	}
	mutex_lock(&ilitek_data->ilitek_debug_mutex);
	ilitek_data->debug_data_frame--;
	if (ilitek_data->debug_data_frame > 0) {
		tp_log_info("ilitek_data->debug_data_frame = %d\n", ilitek_data->debug_data_frame);
	}
	if (ilitek_data->debug_buf[0][0] == 0xDB && ilitek_data->debug_buf[0][1] >= 2) {
		ilitek_data->debug_buf[0][ilitek_data->debug_buf[0][1]] = '\0';
		scnprintf(tmpbuf, PAGE_SIZE, "%s", &ilitek_data->debug_buf[0][2]);
	}
	send_data_len = ilitek_data->debug_buf[0][1] - 2;
	if (copy_to_user(buf, tmpbuf, 64)) {
		ret = -EFAULT;
	} else {
		*ppos += count;
		ret = count;

		tp_log_debug("read %d bytes(s) from %ld\n", count, p);
	}
	for (i = 1; i <= ilitek_data->debug_data_frame; i++) {
		memcpy(ilitek_data->debug_buf[i - 1], ilitek_data->debug_buf[i], 64);
	}
	mutex_unlock(&ilitek_data->ilitek_debug_mutex);
	return send_data_len;
}

/*
   description
   close function
   prarmeters
   inode
   inode
   filp
   file pointer
   return
   status
 */
static int32_t ilitek_debug_file_close(struct inode *inode, struct file *filp)
{
	tp_log_info("\n");
	return 0;
}

// declare file operations
static struct file_operations ilitek_debug_fops = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	.unlocked_ioctl = ilitek_file_debug_ioctl,
#else
	.ioctl = ilitek_file_debug_ioctl,
#endif
	.read = ilitek_debug_file_read,
	.write = ilitek_debug_file_write,
	.open = ilitek_debug_file_open,
	.release = ilitek_debug_file_close,
	.poll = ilitek_debug_file_poll,
	.llseek = ilitek_debug_file_llseek,
};
#endif


int32_t short_test_result;
int32_t open_test_result;
int32_t mopen_test_result;
int32_t allnode_test_result;
int32_t ilitek_short_threshold;
int32_t ilitek_open_threshold;
int32_t ilitek_allnode_max_threshold;
int32_t ilitek_allnode_min_threshold;
int32_t ilitek_open_txdeltathrehold;
int32_t ilitek_open_rxdeltathrehold;
int32_t ilitek_win1_failpoint;
int32_t ilitek_win2_failpoint;
int32_t ilitek_allnodetestw1;
int32_t ilitek_allnodetestw2;
int32_t ilitek_allnodemodule;
int32_t ilitek_allnodetx;
int32_t ilitek_printsensortestdata;
int32_t ilitek_Dump1_Value;
int32_t ilitek_Dump2_Value;
int32_t noisefre_start;
int32_t noisefre_end;
int32_t noisefre_step;
int32_t ilitek_short_Vref;
int32_t ilitek_posidle_L;
int32_t ilitek_posidle_H;

char sensor_test_data_path[256] = "/data/local/tmp/";
char sensor_test_data_path_tmp[256] = "/data/local/tmp/";
char noisefre_data_path[256] = "/data/local/tmp/";
char noisefre_data_path_tmp[256] = "/data/local/tmp/";

int32_t ilitek_check_busy(int32_t count, int32_t delay, int32_t type)
{
	int32_t i;
	uint8_t buf[2];

	for (i = 0; i < count; i++) {
		buf[0] = ILITEK_TP_CMD_GET_SYSTEM_BUSY;
		if (ilitek_i2c_write_and_read(buf, 1, 1, buf, 1) < 0) {
			return ILITEK_I2C_TRANSFER_ERR;
		}
		if ((buf[0] & (ILITEK_TP_SYSTEM_READY + type)) == ILITEK_TP_SYSTEM_READY) {
			//tp_log_info("check_busy i = %d\n", i);
			return ILITEK_SUCCESS;
		}
		msleep(delay);
	}
	tp_log_info("check_busy is busy,0x%x\n", buf[0]);
	return ILITEK_FAIL;
}



static int32_t ilitek_sensortest_lego_series(int32_t *short_xdata1, int32_t *short_ydata1, int32_t *open_data, int32_t *open_tx_avg, 
						int32_t *open_rx_diff, int32_t *allnode_data, int32_t *allnode_win1, int32_t *allnode_win2) {
	int32_t ret = ILITEK_SUCCESS;

	ret = api_protocol_set_testmode(true);
	if(ilitek_data->mp.Short) {
	tp_log_info("start Short test \n");
		ret = ilitek_short_test_v6(short_xdata1, short_ydata1);
		if (ret < 0) {
			tp_log_err("short test fail ret = %d\n", ret);
		}
	}
	else 
		tp_log_info("Short test is disable\n");
	if(ilitek_data->mp.open) {
		tp_log_info("start open test \n");
		ret = ilitek_open_test_v6(open_data, open_tx_avg, open_rx_diff);
		if (ret < 0) {
			tp_log_err("open test fail ret = %d\n", ret);
		}
	}
	else 
		tp_log_info("open test is disable\n");
	if(ilitek_data->mp.m_open) {
		tp_log_info("start Mirco open test \n");
		ret = ilitek_mopen_test_v6(open_data, open_tx_avg, open_rx_diff);
		if (ret < 0) {
			tp_log_err("Mirco open test fail ret = %d\n", ret);
		}
	}
	else 
		tp_log_info("Mirco open test is disable\n");
	if(ilitek_data->mp.unifo) {
		tp_log_info("start Uniformity test \n");
		allnode_test_result = ilitek_uniformity_test_v6(allnode_data , allnode_win1 , allnode_win2);
		if (ret < 0) {
			tp_log_err("allnode test fail ret = %d\n", ret);
		}
	}
	else 
		tp_log_info("Uniformity test is disable\n");
	ret = api_protocol_set_testmode(false);
	
	return ILITEK_SUCCESS;
}

static int32_t ilitek_sensortest_bigger_size_ic(int32_t *short_xdata1, int32_t *short_xdata2, int32_t *short_ydata1, int32_t *short_ydata2, int32_t *open_data, int32_t *allnode_data)
{
	int32_t ret = 0;
	uint8_t cmd[2] = { 0 };
	tp_log_info("\n");
	if (NULL == short_xdata1 || NULL == short_xdata2 || NULL == short_ydata1 || NULL == short_ydata2 || NULL == open_data || NULL == allnode_data) {
		tp_log_err("save data buf is NULL\n");
		return -ENOMEM;
	}
	cmd[0] = 0xF4;
	cmd[1] = 0x51;
	ret = ilitek_i2c_write(cmd, 2);
	if (ret < 0) {
		tp_log_err("ilitek_i2c_write err,f4 51 err ret %d\n", ret);
		//return ret;
	}
	mdelay(10);
	ret = api_protocol_set_testmode(true);
	if (ret < 0) {
		tp_log_err("into test mode err ret = %d\n", ret);
		return ret;
	}
	ret = ilitek_short_test(short_xdata1, short_xdata2, short_ydata1, short_ydata2);
	if (ret < 0) {
		tp_log_err("short test fail ret = %d\n", ret);
	}
	ret = api_protocol_set_testmode(true);
	if (ret < 0) {
		tp_log_err("into test mode err ret = %d\n", ret);
		//return ret;
	}
	ret = ilitek_open_test(open_data);
	if (ret < 0) {
		tp_log_err("open test fail ret = %d\n", ret);
	}
	ret = api_protocol_set_testmode(true);
	if (ret < 0) {
		tp_log_err("into test mode err ret = %d\n", ret);
		//return ret;
	}
	ret = ilitek_allnode_test(allnode_data);
	if (ret < 0) {
		tp_log_err("allnode test fail ret = %d\n", ret);
	}
	ret = api_protocol_set_testmode(false);
	if (ret < 0) {
		tp_log_err("into test mode err ret = %d\n", ret);
		//return ret;
	}
	cmd[0] = 0xF4;
	cmd[1] = 0x50;
	ret = ilitek_i2c_write(cmd, 2);
	if (ret < 0) {
		tp_log_err("ilitek_i2c_write err,f4 50 err ret %d\n", ret);
		//return ret;
	}
	return 0;
}

static int32_t ilitek_sensortest_proc_show(struct seq_file *m, void *v)
{
	int32_t ret = 0;
	int32_t *short_xdata1 = NULL;
	int32_t *short_xdata2 = NULL;
	int32_t *short_ydata1 = NULL;
	int32_t *short_ydata2 = NULL;
	int32_t *open_data = NULL;
	int32_t *open_tx_avg = NULL;
	int32_t *open_rx_diff = NULL;
	int32_t *allnode_data = NULL;
	int32_t *allnode_win1 = NULL;
	int32_t *allnode_win2 = NULL;
	tp_log_info("m->size = %d  m->count = %d\n", (int32_t)m->size, (int32_t)m->count);
	if (m->size <= (4096 * 2)) {
		m->count = m->size;
		return 0;
	}
	short_xdata1 = (int32_t *)vmalloc(sizeof(int32_t) * (ilitek_data->x_ch));
	short_xdata2 = (int32_t *)vmalloc(sizeof(int32_t) * (ilitek_data->x_ch));
	short_ydata1 = (int32_t *)vmalloc(sizeof(int32_t) * (ilitek_data->y_ch));
	short_ydata2 = (int32_t *)vmalloc(sizeof(int32_t) * (ilitek_data->y_ch));
	open_data = (int32_t *)vmalloc(sizeof(int32_t) * (ilitek_data->y_ch * ilitek_data->x_ch));
	open_tx_avg = (int32_t *)vmalloc(sizeof(int32_t) * ilitek_data->y_ch);
	open_rx_diff = (int32_t *)vmalloc(sizeof(int32_t) * ((ilitek_data->y_ch - 1) * ilitek_data->x_ch));
	allnode_data = (int32_t *)vmalloc(sizeof(int32_t) * (ilitek_data->y_ch * ilitek_data->x_ch));
	allnode_win1 = (int32_t *)vmalloc(sizeof(int32_t) * ((ilitek_data->y_ch - 1) * ilitek_data->x_ch));
	allnode_win2 = (int32_t *)vmalloc(sizeof(int32_t) * ((ilitek_data->y_ch - 1) * (ilitek_data->x_ch - 1)));
	if (NULL == short_xdata1 || NULL == short_xdata2 || NULL == short_ydata1 || NULL == short_ydata2 || NULL == open_data
	|| NULL == open_tx_avg || NULL == open_tx_avg || NULL == allnode_data || NULL == allnode_win1 || NULL == allnode_win2) {
		tp_log_err("kzalloc ERR NULL\n");
		ret = -ENOMEM;
		goto out;
	}
	ilitek_irq_disable();
	ilitek_data->operation_protection = true;
	if(ilitek_data->ptl.ver_major == 0x3) {
		ret = ilitek_sensortest_bigger_size_ic(short_xdata1, short_xdata2, short_ydata1, short_ydata2, open_data, allnode_data);
	}
	else if(ilitek_data->ptl.ver_major == 0x6) {
		ret = ilitek_sensortest_lego_series(short_xdata1, short_ydata1, open_data, open_tx_avg, open_rx_diff, allnode_data, allnode_win1, allnode_win2);
	}
	else {
		tp_log_err("The is protocol not support, protocol:0x%06x\n", ilitek_data->ptl.ver);
	}

	tp_log_info("ilitek_allnode_max_threshold = %d\n",ilitek_allnode_max_threshold);
	ilitek_reset(ilitek_data->reset_time, false);
	ilitek_irq_enable();
	ilitek_data->operation_protection = false;
	
	if(ilitek_data->ptl.ver_major == 0x3) {
		ilitek_printf_sensortest_data(short_xdata1, short_xdata2, short_ydata1, short_ydata2, open_data, allnode_data, m);
	}
	else if(ilitek_data->ptl.ver_major == 0x6) {
		ilitek_printf_sensortest_data_v6(short_xdata1, short_ydata1, open_data, open_tx_avg, open_rx_diff, allnode_data, allnode_win1, allnode_win2, m);
	}
	else {
		tp_log_err("The is protocol not support, protocol:0x%06x\n", ilitek_data->ptl.ver);
	}
out:
	if (short_xdata1) {
		vfree(short_xdata1);
		short_xdata1 = NULL;
	}
	if (short_xdata2) {
		vfree(short_xdata2);
		short_xdata2 = NULL;
	}
	if (short_ydata1) {
		vfree(short_ydata1);
		short_ydata1 = NULL;
	}
	if (short_ydata2) {
		vfree(short_ydata2);
		short_ydata2 = NULL;
	}
	if (open_data) {
		vfree(open_data);
		open_data = NULL;
	}
	if (open_tx_avg) {
		vfree(open_tx_avg);
		open_data = NULL;
	}
	if (open_rx_diff) {
		vfree(open_rx_diff);
		open_data = NULL;
	}
	if (allnode_data) {
		vfree(allnode_data);
		allnode_data = NULL;
	}
	if (allnode_win1) {
		vfree(allnode_win1);
		allnode_win1 = NULL;
	}
	if (allnode_win2) {
		vfree(allnode_win2);
		allnode_win2 = NULL;
	}
	return 0;
}

static int32_t ilitek_proc_open_sensortest(struct inode *inode, struct file *file)
{
	tp_log_info("\n");
	return single_open(file, ilitek_sensortest_proc_show, NULL);
}


static ssize_t ilitek_sensortest_write(struct file *pFile, const char __user *buf, size_t size, loff_t *pPos)
{
	int32_t ret = 0;
	uint8_t buffer[512] = { 0 };
	uint8_t ini_path[512] = { 0 }; 

	tp_log_info("\n");
	ret = copy_from_user(buffer, buf, size);
	if (strncmp(buffer, "ini", 3) == 0) {
		ret = sscanf(buffer, "ini%s%s", ini_path, sensor_test_data_path);
		ret = parser_path(ini_path);
		if(ret < 0) {
			tp_log_err("call parser_path fail, ret = %d\n", ret);
		}
		ilitek_short_threshold = ilitek_data->mp.short_max_thr;
		ilitek_Dump1_Value = ilitek_data->mp.dump1;
		ilitek_Dump2_Value = ilitek_data->mp.dump2;
		ilitek_posidle_L = ilitek_data->mp.posidleL;
		ilitek_posidle_H = ilitek_data->mp.posidleH;
		ilitek_short_Vref = ilitek_data->mp.vref_s;
		ilitek_open_threshold = ilitek_data->mp.open_min_thr;
		ilitek_allnode_max_threshold = ilitek_data->mp.uni.max_thr;
		ilitek_allnode_min_threshold = ilitek_data->mp.uni.min_thr;
		ilitek_allnodetestw1 = ilitek_data->mp.uni.win1_thr;
		ilitek_win1_failpoint = ilitek_data->mp.uni.win1_fail;
		ilitek_allnodetestw2 = ilitek_data->mp.uni.win2_thr;
		ilitek_win2_failpoint = ilitek_data->mp.uni.win2_fail;
		ilitek_printsensortestdata = 1;
		tp_log_info("ilitek_short_threshold = %d\n", ilitek_short_threshold);
		tp_log_info("ilitek_Dump1_Value = %d\n", ilitek_Dump1_Value);
		tp_log_info("ilitek_Dump2_Value = %d\n", ilitek_Dump2_Value);
		tp_log_info("ilitek_posidle_L = %d\n", ilitek_posidle_L);
		tp_log_info("ilitek_posidle_H = %d\n", ilitek_posidle_H);
		tp_log_info("ilitek_short_Vref = %d\n", ilitek_short_Vref);
		tp_log_info("ilitek_open_threshold = %d\n", ilitek_open_threshold);
		tp_log_info("ilitek_allnode_max_threshold = %d\n", ilitek_allnode_max_threshold);
		tp_log_info("ilitek_allnode_min_threshold = %d\n", ilitek_allnode_min_threshold);
		tp_log_info("ilitek_allnodetestw1 = %d\n", ilitek_allnodetestw1);
		tp_log_info("ilitek_win1_failpoint = %d\n", ilitek_win1_failpoint);
		tp_log_info("ilitek_allnodetestw2 = %d\n", ilitek_allnodetestw2);
		tp_log_info("ilitek_win1_failpoint = %d\n", ilitek_win2_failpoint);
		tp_log_info("ilitek_printsensortestdata = %d\n", ilitek_printsensortestdata);
		tp_log_info("sensor_test_data_path = %s\n", sensor_test_data_path);
	}
	else {
		if(ilitek_data->ptl.ver_major == 0x3)
		{
			ret = sscanf(buffer, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s", 
						&ilitek_short_threshold, 
						&ilitek_open_threshold, 
						&ilitek_open_txdeltathrehold,
						&ilitek_open_rxdeltathrehold, 
						&ilitek_allnode_max_threshold, 
						&ilitek_allnode_min_threshold,
						&ilitek_allnodetestw1, 
						&ilitek_allnodetestw2, 
						&ilitek_allnodemodule, 
						&ilitek_allnodetx, 
						&ilitek_printsensortestdata,
					sensor_test_data_path);
			if (ret != 12) {
				tp_log_err("sscanf get value fail\n");
			}
			tp_log_info("ilitek_short_threshold = %d\n", ilitek_short_threshold);
			tp_log_info("ilitek_open_threshold = %d\n", ilitek_open_threshold);
			tp_log_info("ilitek_open_txdeltathrehold = %d\n", ilitek_open_txdeltathrehold);
			tp_log_info("ilitek_open_rxdeltathrehold = %d\n", ilitek_open_rxdeltathrehold);
			tp_log_info("ilitek_allnode_max_threshold = %d\n", ilitek_allnode_max_threshold);
			tp_log_info("ilitek_allnode_min_threshold = %d\n", ilitek_allnode_min_threshold);
			tp_log_info("ilitek_allnodetestw1 = %d\n", ilitek_allnodetestw1);
			tp_log_info("ilitek_allnodetestw2 = %d\n", ilitek_allnodetestw2);
			tp_log_info("ilitek_allnodemodule = %d\n", ilitek_allnodemodule);
			tp_log_info("ilitek_allnodetx = %d\n", ilitek_allnodetx);
			tp_log_info("ilitek_printsensortestdata = %d\n", ilitek_printsensortestdata);
			tp_log_info("sensor_test_data_path = %s\n", sensor_test_data_path);
		}
		else if(ilitek_data->ptl.ver_major == 0x6)
		{

			ret = sscanf(buffer, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s", 
						&ilitek_short_threshold,
						&ilitek_Dump1_Value,
						&ilitek_Dump2_Value,
						&ilitek_open_threshold, 
						&ilitek_allnode_max_threshold, 
						&ilitek_allnode_min_threshold,
						&ilitek_allnodetestw1,
						&ilitek_win1_failpoint,
						&ilitek_allnodetestw2, 
						&ilitek_win2_failpoint,  
						&ilitek_printsensortestdata,
						sensor_test_data_path);
			if (ret != 12) {
				tp_log_err("sscanf get value fail\n");
			}
			tp_log_info("ilitek_short_threshold = %d\n", ilitek_short_threshold);
			tp_log_info("ilitek_Dump1_Value = %d\n", ilitek_Dump1_Value);
			tp_log_info("ilitek_Dump2_Value = %d\n", ilitek_Dump2_Value);
			tp_log_info("ilitek_open_threshold = %d\n", ilitek_open_threshold);
			tp_log_info("ilitek_allnode_max_threshold = %d\n", ilitek_allnode_max_threshold);
			tp_log_info("ilitek_allnode_min_threshold = %d\n", ilitek_allnode_min_threshold);
			tp_log_info("ilitek_allnodetestw1 = %d\n", ilitek_allnodetestw1);
			tp_log_info("ilitek_win1_failpoint = %d\n", ilitek_win1_failpoint);
			tp_log_info("ilitek_allnodetestw2 = %d\n", ilitek_allnodetestw2);
			tp_log_info("ilitek_win1_failpoint = %d\n", ilitek_win2_failpoint);
			tp_log_info("ilitek_printsensortestdata = %d\n", ilitek_printsensortestdata);
			tp_log_info("sensor_test_data_path = %s\n", sensor_test_data_path);
			ilitek_data->mp.open = true;
			ilitek_data->mp.Short = true;
			ilitek_data->mp.unifo = true;
		}
		else
			tp_log_err("Ilitek protocol version fail\n");
	}
	return size;
}

static struct file_operations ilitek_proc_fops_sensortest = {
	.open = ilitek_proc_open_sensortest,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = ilitek_sensortest_write,
	.release = single_release,
};

static void ilitek_printf_noisefre_data(uint8_t *noisefre_data, struct seq_file *m)
{
	int32_t i = 0, j = 0, len = 0, loop_10 = 0;
	int32_t read_noisefre_data_len = 0;
	struct file *filp;
	mm_segment_t fs;
	uint8_t buf[128];
	struct timeval time_now;
	struct rtc_time tm;
	do_gettimeofday(&time_now);
	rtc_time_to_tm(time_now.tv_sec, &tm);
	tp_log_info("%d_%d_%d_%d_%d_%d\n", (tm.tm_year + 1900), tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	len = scnprintf(buf, PAGE_SIZE, "ilitek_noisefre_%d%02d%02d%02d%02d%02d_pass.csv", (tm.tm_year + 1900), tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	for (j = 0; j < 256; j++) {
		noisefre_data_path_tmp[j] = noisefre_data_path[j];
	}
	strcat(noisefre_data_path, buf);
	tp_log_info("noisefre_data_path = %s\n", noisefre_data_path);

	read_noisefre_data_len = ((noisefre_step != 0) ?
				  (((noisefre_end - noisefre_start) * 10) / noisefre_step + 1) : (((noisefre_end - noisefre_start) * 10) * 2));
	loop_10 = read_noisefre_data_len / 20;
	if (read_noisefre_data_len % 20) {
		loop_10 += 1;
	}
	for (i = 0; i < loop_10; i++) {
		if ((read_noisefre_data_len % 20) && (i == (loop_10 - 1))) {
			for (j = 0; j < (read_noisefre_data_len % 20); j++) {
				seq_printf(m, "%04d,", (noisefre_start * 10) + ((i * 20 + j) * noisefre_step));
			}
			seq_printf(m, "\n");
			for (j = 0; j < (read_noisefre_data_len % 20); j++) {
				seq_printf(m, "%04d,", noisefre_data[(i * 20 + j)]);
			}
			seq_printf(m, "\n");
		} else {
			for (j = 0; j < 20; j++) {
				seq_printf(m, "%04d,", (noisefre_start * 10) + ((i * 20 + j) * noisefre_step));
			}
			seq_printf(m, "\n");
			for (j = 0; j < 20; j++) {
				seq_printf(m, "%04d,", noisefre_data[(i * 20 + j)]);
			}
			seq_printf(m, "\n");
			seq_printf(m, "\n");
		}
	}
	tp_log_info("m->size = %d  m->count = %d\n", (int32_t)m->size, (int32_t)m->count);

	filp = filp_open(noisefre_data_path, O_CREAT | O_WRONLY, 0777);
	if (IS_ERR(filp)) {
		tp_log_err("save noisefre data  File Open Error path = %s\n", noisefre_data_path);
	} else {
		fs = get_fs();
		set_fs(KERNEL_DS);

		for (j = (noisefre_start * 10); j < (noisefre_end * 10); j += noisefre_step) {
			len = scnprintf(buf, PAGE_SIZE, "%03d,", j);
			printk("%03d,", j);
			vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		}
		len = scnprintf(buf, PAGE_SIZE, "\n");
		printk("\n");
		vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		for (j = 0; j < read_noisefre_data_len; j++) {
			len = scnprintf(buf, PAGE_SIZE, "%03d,", noisefre_data[j]);
			printk("%03d,", noisefre_data[j]);
			vfs_write(filp, (__force const char __user *)buf, len, &(filp->f_pos));
		}
		printk("\n");
		set_fs(fs);
	}
	for (j = 0; j < 256; j++) {
		noisefre_data_path[j] = noisefre_data_path_tmp[j];
	}
	return;
}

static int32_t ilitek_noisefre_proc_show(struct seq_file *m, void *v)
{
	int32_t ret = 0, newMaxSize = 32, i = 0, j = 0, index = 0;
	uint8_t cmd[8] = { 0 };
	int32_t read_noisefre_data_len = 0;
	int32_t test_32 = 0;
	uint8_t *buf_recv = NULL;
	tp_log_info("m->size = %d  m->count = %d\n", (int32_t)m->size, (int32_t)m->count);
	if (m->size <= (4096)) {
		m->count = m->size;
		return 0;
	}
	read_noisefre_data_len = ((noisefre_step != 0) ?
				  (((noisefre_end - noisefre_start) * 10) / noisefre_step + 1) : (((noisefre_end - noisefre_start) * 10) * 2));
	test_32 = (read_noisefre_data_len) / (newMaxSize - 2);
	if (read_noisefre_data_len % (newMaxSize - 2) != 0) {
		test_32 += 1;
	}
	tp_log_info("kzalloc  test_32 = %d\n", test_32);
	buf_recv = (uint8_t *) vmalloc(sizeof(uint8_t) * (read_noisefre_data_len + test_32 * 2 + 32));
	if (NULL == buf_recv) {
		tp_log_err("buf_recv NULL\n");
		return -ENOMEM;
	}
	ilitek_irq_disable();
	ret = api_protocol_set_testmode(true);
	if (ret < 0) {
		tp_log_err("into test mode err ret = %d\n", ret);
		return ret;
	}
	cmd[0] = 0xF3;
	cmd[1] = 0x0F;
	cmd[2] = noisefre_start;
	cmd[3] = noisefre_end;
	cmd[4] = noisefre_step;
	ret = ilitek_i2c_write(cmd, 5);
	if (ret < 0) {
		tp_log_err("ilitek_i2c_write err,ilitek_into_testmode err ret %d\n", ret);
		return ret;
	}
	ret = ilitek_check_busy(1000, 10, ILITEK_TP_SYSTEM_BUSY);
	if (ret < 0) {
		tp_log_err("allnode test  check busy err ret = %d\n", ret);
	}

	cmd[0] = 0xE4;
	ret = ilitek_i2c_write(cmd, 1);
	if (ret < 0) {
		tp_log_err("ilitek_i2c_write err,ilitek_into_testmode err ret %d\n", ret);
		return ret;
	}
	for (i = 0; i < test_32; i++) {
		if (read_noisefre_data_len % (newMaxSize - 2) != 0 && i == test_32 - 1) {
			ret = ilitek_i2c_read(buf_recv + newMaxSize * i, read_noisefre_data_len % (newMaxSize - 2) + 2);
		} else {
			ret = ilitek_i2c_read(buf_recv + newMaxSize * i, newMaxSize);
		}
		if (ret < 0) {
			tp_log_err("err,i2c read error ret %d\n", ret);
		}
	}
	j = 0;
	for (i = 0; i < test_32; i++) {
		if (j == read_noisefre_data_len) {
			break;
		}
		for (index = 2; index < newMaxSize; index++) {
			buf_recv[j] = (buf_recv[i * newMaxSize + index]);
			j++;
			if (j == read_noisefre_data_len) {
				break;
			}
		}
	}
	ilitek_reset(ilitek_data->reset_time, false);
	ilitek_irq_enable();
	ilitek_printf_noisefre_data(buf_recv, m);
	if (buf_recv) {
		vfree(buf_recv);
		buf_recv = NULL;
	}
	return 0;
}

static int32_t ilitek_proc_open_noisefre(struct inode *inode, struct file *file)
{
	tp_log_info("\n");
	return single_open(file, ilitek_noisefre_proc_show, NULL);
}

static ssize_t ilitek_noisefre_write(struct file *pFile, const char __user *buf, size_t size, loff_t *pPos)
{
	int32_t ret = 0;
	uint8_t buffer[512] = { 0 };
	tp_log_info("\n");
	ret = copy_from_user(buffer, buf, size);
	ret = sscanf(buffer, "%d,%d,%d,%s", &noisefre_start, &noisefre_end, &noisefre_step, noisefre_data_path);
	if (ret != 4) {
		tp_log_err("sscanf get value fail\n");
	}
	tp_log_info("noisefre_start = %d\n", noisefre_start);
	tp_log_info("noisefre_end = %d\n", noisefre_end);
	tp_log_info("noisefre_step = %d\n", noisefre_step);
	tp_log_info("noisefre_data_path = %s\n", noisefre_data_path);
	return size;
}

static struct file_operations ilitek_proc_fops_noisefre = {
	.open = ilitek_proc_open_noisefre,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = ilitek_noisefre_write,
	.release = single_release,
};

static uint32_t ilitek_hex_2_dec(uint8_t *hex, int32_t len)
{
	uint32_t ret = 0, temp = 0;
	int32_t i, shift = (len - 1) * 4;
	for (i = 0; i < len; shift -= 4, i++) {
		if ((hex[i] >= '0') && (hex[i] <= '9')) {
			temp = hex[i] - '0';
		} else if ((hex[i] >= 'a') && (hex[i] <= 'z')) {
			temp = (hex[i] - 'a') + 10;
		} else {
			temp = (hex[i] - 'A') + 10;
		}
		ret |= (temp << shift);
	}
	return ret;
}

int hex_mapping_convert(uint32_t addr,uint8_t *buffer) {
	//uint32_t hex_fwaddr_start = 0;
	uint32_t hex_dfaddr_start = 0;
	uint32_t hex_icaddr_start = 0;
	uint32_t start = 0, count = 0, index = 0;
	//get fw version
	start = addr + HEX_FWVERSION_ADDRESS;
	for(count = start, index = HEX_FWVERSION_SIZE - 1; count < start + HEX_FWVERSION_SIZE; count++, --index)
	{
			ilitek_data->upg.hex_fw_ver[index] = buffer[count];
	}
	tp_log_info("Hex FW version:");
	for(count = 0; count < HEX_FWVERSION_SIZE; count++)
			printk("0x%x,", ilitek_data->upg.hex_fw_ver[count]);
	//get df start address
	tp_log_info("\nhex df start address:");
	hex_dfaddr_start = addr + HEX_DATA_FLASH_ADDRESS;
	for(count = hex_dfaddr_start, index = HEX_DATA_FLASH_SIZE - 1; count < hex_dfaddr_start + HEX_DATA_FLASH_SIZE; count++, --index)
	{
			printk("0x%x,", buffer[count]);
	}
	tp_log_info("\nhex ic type:");
	hex_icaddr_start = addr + HEX_KERNEL_VERSION_ADDRESS;
	tp_log_info("\nhex End address:0x%x\n", sizeof(buffer)); 
	for(count = hex_icaddr_start, index = 0; count < hex_icaddr_start + HEX_KERNEL_VERSION_SIZE; count++,index++)
	{
			printk("0x%x,", buffer[count]);
			ilitek_data->upg.hex_ic_type[index] = buffer[count];
	}
	//get memony mapping version
	start = addr + HEX_MEMONY_MAPPING_VERSION_ADDRESS;
	for(count = start, index = 0; count < start + HEX_MEMONY_MAPPING_VERSION_SIZE; count++, index++)
	{
			ilitek_data->upg.map_ver += buffer[count] << (index*8);
	}
	tp_log_info("Hex Mapping Version: 0x%x\n", ilitek_data->upg.map_ver);
	if(ilitek_data->upg.map_ver >= 0x10000)
	{
			//get flash block number
			ilitek_data->upg.blk_num = buffer[addr + HEX_FLASH_BLOCK_NUMMER_ADDRESS];
			tp_log_info("------------Hex Block information------------\n");
			tp_log_info("Hex flash block number: %d\n", ilitek_data->upg.blk_num);
			ilitek_data->blk = (struct ilitek_block_info*)kmalloc(ilitek_data->upg.blk_num*sizeof(struct ilitek_block_info), GFP_KERNEL);
			for(count = 0; count < ilitek_data->upg.blk_num; count++)
			{
				start = addr + HEX_FLASH_BLOCK_INFO_ADDRESS + HEX_FLASH_BLOCK_INFO_SIZE * count;
				ilitek_data->blk[count].start = buffer[start] + (buffer[start+1] << 8) + (buffer[start+2] << 16);
				if(count == ilitek_data->upg.blk_num - 1)
				{
					addr = addr + HEX_FLASH_BLOCK_END_ADDRESS;
					ilitek_data->blk[count].end = buffer[addr] + (buffer[addr+1] << 8) + (buffer[addr+2] << 16);
				}
				else {
					ilitek_data->blk[count].end = buffer[start+3] + (buffer[start+4] << 8) + (buffer[start+5] << 16) - 1;
				}
				#ifdef ILITEK_UPDATE_FW
				if(count == 0)
					ilitek_data->blk[count].end = get_ap_endaddr(ilitek_data->blk[count].start, ilitek_data->blk[count].end, buffer);
				else if(ilitek_data->blk[count].start > CTPM_FW_size - 32)
					;
				else
					ilitek_data->blk[count].end = get_block_endaddr(ilitek_data->blk[count].start, ilitek_data->blk[count].end, buffer);
				#endif
				tp_log_info("Hex Block:%d, start:0x%x end:0x%x\n", count, ilitek_data->blk[count].start,  ilitek_data->blk[count].end);
			}
	}
	return ILITEK_SUCCESS;
}

static int32_t ilitek_parse_hex_file(uint32_t *df_startaddr, uint32_t *df_endaddr, uint32_t *df_checksum,
				 uint32_t *ap_startaddr, uint32_t *ap_endaddr, uint32_t *ap_checksum, int32_t hex_len,
				 uint8_t *CTPM_FW, uint8_t *save_hex_content)
{
	int32_t i = 0, j = 0, k = 0, count = 0;
	uint32_t checksum = 0, check = 0;
	uint32_t len = 0, addr = 0, type = 0, exaddr = 0;
	int32_t hex_end = 0;
	int32_t offset;
	bool read_mapping = false;

	tp_log_info("\n");
	if (save_hex_content == NULL) {
		tp_log_err("save_hex_content is null\n");
		return -2;
	}
	if (CTPM_FW == NULL) {
		tp_log_err("CTPM_FW is null\n");
		return -2;
	}
	ilitek_data->upgrade_FW_info_addr = -1;
	for (i = 0; i < hex_len;) {
		len = ilitek_hex_2_dec(&save_hex_content[i + 1], 2);
		addr = ilitek_hex_2_dec(&save_hex_content[i + 3], 4);
		type = ilitek_hex_2_dec(&save_hex_content[i + 7], 2);
		if (type == 1) {
			hex_end = 1;
		}
		if (type == 0x02) {
			exaddr = ilitek_hex_2_dec(&save_hex_content[i + 9], 4) << 4;
			tp_log_info("exaddr = %x\n", (int32_t)exaddr);

		}
		if (type == 0x04) {
			exaddr = ilitek_hex_2_dec(&save_hex_content[i + 9], 4) << 16;
			tp_log_info("exaddr = %x\n", (int32_t)exaddr);
		}
		addr = addr + exaddr;
		//calculate checksum
		checksum = 0;
		for (j = 8; j < (2 + 4 + 2 + (len * 2)); j += 2) {
			if (type == 0x00) {
				check = check + ilitek_hex_2_dec(&save_hex_content[i + 1 + j], 2);
				if (addr + (j - 8) / 2 < *df_startaddr) {
					*ap_checksum = *ap_checksum + ilitek_hex_2_dec(&save_hex_content[i + 1 + j], 2);
				} else {
					*df_checksum = *df_checksum + ilitek_hex_2_dec(&save_hex_content[i + 1 + j], 2);
				}
			} else {
				checksum = 0;
			}
		}
		if (save_hex_content[i + 1 + j + 2] == 0x0D) {
			offset = 2;
		} else {
			offset = 1;
		}
		if (addr < *df_startaddr) {
			*ap_checksum = *ap_checksum + checksum;
		} else {
			*df_checksum = *df_checksum + checksum;
		}
		if(addr >= ilitek_data->upgrade_FW_info_addr + HEX_MEMONY_MAPPING_FLASH_SIZE
		&& ilitek_data->upg.hex_info_flag && read_mapping == false)
		{
				read_mapping = true;
				hex_mapping_convert(ilitek_data->upgrade_FW_info_addr, CTPM_FW+32);
		}
		if (type == 0x00) {
			if (addr < *ap_startaddr) {
				*ap_startaddr = addr;
			}
			if ((addr + len) > *ap_endaddr && (addr < *df_startaddr)) {
				*ap_endaddr = addr + len - 1;
				if (*ap_endaddr > *df_startaddr) {
					*ap_endaddr = *df_startaddr - 1;
				}
			}
			if ((addr + len) > *df_endaddr && (addr >= *df_startaddr)) {
				*df_endaddr = addr + len;
			}
			//fill data
			for (j = 0, k = 0; j < (len * 2); j += 2, k++) {
				CTPM_FW[32 + addr + k] = ilitek_hex_2_dec(&save_hex_content[i + 9 + j], 2);
			}
			//for BL protocol 1.8
			for(count = 0; count < ilitek_data->upg.blk_num; count++) {
				if(addr + len - 1 > ilitek_data->blk[count].start && addr + len - 1 < ilitek_data->blk[count+1].start)
						ilitek_data->blk[count].end = addr + len - 1;
				else if(addr + len - 1 > ilitek_data->blk[count].start && count == ilitek_data->upg.blk_num - 1){
                    ilitek_data->blk[count].end = addr + len - 1;
                }
			}
		}
		if (type == 0xAC) {
			ilitek_data->upgrade_FW_info_addr = 0;
			for (j = 0; j < len; j++) {
				ilitek_data->upgrade_FW_info_addr += ilitek_hex_2_dec(&save_hex_content[i + 9 + j * 2], 2) << (8 * (len - j - 1));
				ilitek_data->upg.hex_info_flag = true;
			}
			tp_log_info("upgrade_FW_info_addr = 0x%X\n", ilitek_data->upgrade_FW_info_addr);
		}
		if (type == 0xAD) {
			*df_startaddr = ilitek_hex_2_dec(&save_hex_content[i + 9], 6);
			tp_log_info("Hex df_startaddr = 0x%X\n", *df_startaddr);
		}
		i += 1 + 2 + 4 + 2 + (len * 2) + 2 + offset;
	}
	if (hex_end == 0) {
		tp_log_err("hex file is invalid\n");
		return -1;
	}
	return 0;
}

static ssize_t ilitek_update_with_hex_read(struct file *pFile, char __user *buf, size_t nCount, loff_t *pPos)
{
	int32_t ret = 0;
	struct file *filp;
	struct inode *inode;
	uint8_t tmpbuf[256] = { 0 }, outbuf[256] = { 0 };
	mm_segment_t fs;
	off_t fsize;
	uint32_t ap_startaddr = 0xFFFF, df_startaddr = 0xFFFF, ap_endaddr = 0, df_endaddr = 0, ap_checksum = 0, df_checksum = 0;
	uint8_t *CTPM_FW = NULL;
	uint8_t *save_hex_content = NULL;

	CTPM_FW = (uint8_t *)vmalloc(ILITEK_HEX_UPGRADE_SIZE);	/* buf size if even */
	memset(CTPM_FW, 0xFF, ILITEK_HEX_UPGRADE_SIZE);
	tp_log_info("\n");
	if (*pPos != 0) {
		return 0;
	}
	if (!(CTPM_FW)) {
		tp_log_err("alloctation CTPM_FW memory failed\n");
		nCount = scnprintf(tmpbuf, PAGE_SIZE, "alloctation CTPM_FW memory failed\n");
		goto out;
	}
	if (!(strstr(ilitek_data->upg.filename, ".hex"))) {
		tp_log_err("ilitek_data->upg.filename is invalid ilitek_data->upg.filename = %s\n", ilitek_data->upg.filename);
		nCount = scnprintf(tmpbuf, PAGE_SIZE, "ilitek_data->upg.filename is invalid ilitek_data->upg.filename = %s\n", ilitek_data->upg.filename);
		goto out;
	} else {
		filp = filp_open(ilitek_data->upg.filename, O_RDONLY, 0);
		if (IS_ERR(filp)) {
			tp_log_err("hex File Open Error ilitek_data->upg.filename = %s\n", ilitek_data->upg.filename);
			nCount = scnprintf(tmpbuf, PAGE_SIZE, "hex File Open Error ilitek_data->upg.filename = %s\n", ilitek_data->upg.filename);
			goto out;
		} else {
			tp_log_info("hex File Open Right,O_RDONLY %s\n", ilitek_data->upg.filename);
			if (!filp->f_op) {
				tp_log_err("File Operation Method Error\n");
				nCount = scnprintf(tmpbuf, PAGE_SIZE, "File Operation Method Error\n");
				goto out;
			} else {
				//inode = filp->f_path.dentry->d_inode;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 1)
				inode = filp->f_dentry->d_inode;
#else
				inode = file_inode(filp);
#endif
				fsize = inode->i_size;
				tp_log_info("File size:%d\n", (int32_t)fsize);
				save_hex_content = (uint8_t *)vmalloc((int32_t)fsize);
				//save_hex_content = kmalloc((int)fsize, GFP_ATOMIC);           /* buf size if even */
				if (!(save_hex_content)) {
					tp_log_err("alloctation save_hex_content memory failed\n");
					nCount = scnprintf(tmpbuf, PAGE_SIZE, "alloctation save_hex_content memory failed\n");
					goto out;
				}
				fs = get_fs();
				set_fs(KERNEL_DS);

				vfs_read(filp, save_hex_content, fsize, &(filp->f_pos));
				set_fs(fs);

				filp_close(filp, NULL);
			}
		}
	}
	ilitek_irq_disable();
	if (api_protocol_set_cmd(ILITEK_TP_CMD_GET_KERNEL_VERSION, NULL, outbuf) < ILITEK_SUCCESS) {
		tp_log_err("Get mcu version err error = %d\n", ret);
		nCount = scnprintf(tmpbuf, PAGE_SIZE, "Get mcu version error, ret = %d\n", ret);
		goto out;
	} else {
		tp_log_info("MCU version: 0x%x%x", ilitek_data->mcu_ver[1], ilitek_data->mcu_ver[0]);
		api_protocol_set_cmd(ILITEK_TP_CMD_GET_FIRMWARE_VERSION, NULL, outbuf);
		ret = ilitek_parse_hex_file(&df_startaddr, &df_endaddr, &df_checksum, &ap_startaddr, &ap_endaddr,
			&ap_checksum, fsize, CTPM_FW, save_hex_content);
		if (ret < 0) {
			tp_log_err("ilitek_parse_hex_file err ret = %d\n", ret);
			nCount = scnprintf(tmpbuf, PAGE_SIZE, "ilitek_parse_hex_file err ret = %d\n", ret);
			goto out;
		}
		if (ilitek_data->upgrade_FW_info_addr != -1 && (ilitek_data->mcu_ver[0] != CTPM_FW[ilitek_data->upgrade_FW_info_addr + 32 + 6] ||
								ilitek_data->mcu_ver[1] != CTPM_FW[ilitek_data->upgrade_FW_info_addr + 32 + 7])) {
			tp_log_err("upgrade file mismatch!!! ic is ILI%02X%02X, upgrade file is ILI%02X%02X\n", ilitek_data->mcu_ver[1],
					ilitek_data->mcu_ver[0], CTPM_FW[ilitek_data->upgrade_FW_info_addr + 32 + 7],
					CTPM_FW[ilitek_data->upgrade_FW_info_addr + 32 + 6]);
			nCount =
				scnprintf(tmpbuf, PAGE_SIZE, "upgrade file mismatch!!! ic is ILI%02X%02X, upgrade file is ILI%02X%02X\n", ilitek_data->mcu_ver[1],
					ilitek_data->mcu_ver[0], CTPM_FW[ilitek_data->upgrade_FW_info_addr + 32 + 7],
					CTPM_FW[ilitek_data->upgrade_FW_info_addr + 32 + 6]);
			goto out;
		} else if ((ilitek_data->upgrade_FW_info_addr == -1 && ilitek_data->mcu_ver[0] != 0x03 && ilitek_data->mcu_ver[0] != 0x09)) {
			tp_log_err("upgrade file  mismatch!!! ic is ILI%02X%02X, upgrade file is maybe ILI230X\n", ilitek_data->mcu_ver[1],
					ilitek_data->mcu_ver[0]);
			nCount =
				scnprintf(tmpbuf, PAGE_SIZE, "upgrade file  mismatch!!! ic is ILI%02X%02X, upgrade file is maybe ILI230X\n", ilitek_data->mcu_ver[1],
					ilitek_data->mcu_ver[0]);
			goto out;
		}
		tp_log_info("ilitek ap_startaddr=0x%X, ap_endaddr=0x%X, ap_checksum=0x%X\n", ap_startaddr, ap_endaddr, ap_checksum);
		tp_log_info("ilitek df_startaddr=0x%X, df_endaddr=0x%X, df_checksum=0x%X\n", df_startaddr, df_endaddr, df_checksum);
		ilitek_data->firmware_updating = true;
		ilitek_data->operation_protection = true;
		ret = ilitek_upgrade_frimware(df_startaddr, df_endaddr, df_checksum, ap_startaddr, ap_endaddr, ap_checksum, CTPM_FW);
		ilitek_data->operation_protection = false;
		ilitek_data->firmware_updating = false;
		if (ret < 0) {
			tp_log_err("upgrade fail ret = %d\n", ret);
			nCount = scnprintf(tmpbuf, PAGE_SIZE, "upgrade fail ret = %d\n", ret);
			goto out;
		}
	}

	ret = ilitek_read_tp_info(false);
	nCount =
	    scnprintf(tmpbuf, PAGE_SIZE, "upgrade successfull ilitek firmware version is %d.%d.%d.%d.%d.%d.%d.%d\n", ilitek_data->firmware_ver[0],
		    ilitek_data->firmware_ver[1], ilitek_data->firmware_ver[2], ilitek_data->firmware_ver[3], ilitek_data->firmware_ver[4],
		    ilitek_data->firmware_ver[5], ilitek_data->firmware_ver[6], ilitek_data->firmware_ver[7]);
out:
	ret = copy_to_user(buf, tmpbuf, nCount);
	ilitek_irq_enable();
	if (CTPM_FW) {
		vfree(CTPM_FW);
		CTPM_FW = NULL;
	}
	if (save_hex_content) {
		vfree(save_hex_content);
		save_hex_content = NULL;
	}
	*pPos += nCount;
	return nCount;
}

static ssize_t ilitek_update_with_hex_write(struct file *pFile, const char __user *buf, size_t size, loff_t *pPos)
{
	int32_t i = 0, ret = 0;
	uint8_t buffer[512] = { 0 };
	tp_log_info("size = %d\n", (int32_t)size);
	ret = copy_from_user(buffer, buf, size);
	if (size > 256) {
		tp_log_err("size > 256 not support size = %d\n", (int32_t)size);
	} else {
		for (i = 0; i < (size - 1); i++) {
			tp_log_info("%c\n", buffer[i]);
			ilitek_data->upg.filename[i] = buffer[i];
		}
		ilitek_data->upg.filename[size - 1] = '\0';
		tp_log_info("ilitek_data->upg.filename = %s\n", ilitek_data->upg.filename);
	}
	return size;
}

static struct file_operations ilitek_proc_fops_fwupdate = {
	.read = ilitek_update_with_hex_read,
	.write = ilitek_update_with_hex_write,
};

static ssize_t ilitek_firmware_version_read(struct file *pFile, char __user *buf, size_t nCount, loff_t *pPos)
{
	int32_t ret = 0;
	uint8_t tmpbuf[256] = { 0 };
	tp_log_info("\n");
	if (*pPos != 0) {
		return 0;
	}
	ilitek_irq_disable();
	ret = ilitek_read_tp_info(false);
	ilitek_irq_enable();
	if (ret < 0) {
		tp_log_err("ilitek_read_tp_info err ret = %d\n", ret);
		nCount = scnprintf(tmpbuf, PAGE_SIZE, "ilitek firmware version read error ret = %d\n", ret);

	} else {
		nCount = scnprintf(tmpbuf, PAGE_SIZE, "ilitek firmware version is %d.%d.%d.%d.%d.%d.%d.%d\n", ilitek_data->firmware_ver[0], ilitek_data->firmware_ver[1],
				 ilitek_data->firmware_ver[2], ilitek_data->firmware_ver[3], ilitek_data->firmware_ver[4], ilitek_data->firmware_ver[5],
				 ilitek_data->firmware_ver[6], ilitek_data->firmware_ver[7]);
	}
	*pPos += nCount;
	ret = copy_to_user(buf, tmpbuf, nCount);
	return nCount;
}

static struct file_operations ilitek_proc_fops_fwversion = {
	.read = ilitek_firmware_version_read,
	.write = NULL,
};

static ssize_t ilitek_setmode_0(struct file *pFile, char __user *buf, size_t nCount, loff_t *pPos)
{
	int ret = ILITEK_SUCCESS;
	disable_irq(ilitek_data->client->irq);
	ret = api_protocol_set_testmode(true);
	ret = api_protocol_set_funcmode(ILITEK_TP_SET_MODE_0);
	ret = api_protocol_set_testmode(false);
	enable_irq(ilitek_data->client->irq);
	return 0;
}
static struct file_operations ilitek_proc_fops_setmode_0 = {
	.read = ilitek_setmode_0,
	.write = NULL,
};

static ssize_t ilitek_setmode_1(struct file *pFile, char __user *buf, size_t nCount, loff_t *pPos)
{
	int ret = ILITEK_SUCCESS;
	disable_irq(ilitek_data->client->irq);
	ret = api_protocol_set_testmode(true);
	ret = api_protocol_set_funcmode(ILITEK_TP_SET_MODE_1);
	ret = api_protocol_set_testmode(false);
	enable_irq(ilitek_data->client->irq);
	return 0;
}
static struct file_operations ilitek_proc_fops_setmode_1 = {
	.read = ilitek_setmode_1,
	.write = NULL,
};

static ssize_t ilitek_setmode_2(struct file *pFile, char __user *buf, size_t nCount, loff_t *pPos)
{
	int ret = ILITEK_SUCCESS;
	disable_irq(ilitek_data->client->irq);
	ret = api_protocol_set_testmode(true);
	ret = api_protocol_set_funcmode(ILITEK_TP_SET_MODE_2);
	ret = api_protocol_set_testmode(false);
	enable_irq(ilitek_data->client->irq);
	return 0;
}
static struct file_operations ilitek_proc_fops_setmode_2 = {
	.read = ilitek_setmode_2,
	.write = NULL,
};

#ifdef ILITEK_GESTURE
static ssize_t ilitek_gesture_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (ilitek_data->enable_gesture) {
		return scnprintf(buf, PAGE_SIZE, "gesture: on\n");
	} else {
		return scnprintf(buf, PAGE_SIZE, "gesture: off\n");
	}
}

static ssize_t ilitek_gesture_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	if (buf[0])
		ilitek_data->enable_gesture = true;
	else
		ilitek_data->enable_gesture = false;
	return size;
}

static DEVICE_ATTR(gesture, 0664, ilitek_gesture_show, ilitek_gesture_store);
#endif

static ssize_t ilitek_firmware_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	tp_log_info("\n");
	ilitek_irq_disable();
	ret = ilitek_read_tp_info(false);
	ilitek_irq_enable();
	if (ret < 0) {
		tp_log_err("ilitek_read_tp_info err ret = %d\n", ret);
		return scnprintf(buf, PAGE_SIZE, "ilitek firmware version read error ret = %d\n", ret);
	} else {
#ifdef CHROME_OS
		return scnprintf(buf, PAGE_SIZE, "%d.%d\n",
			       ilitek_data->firmware_ver[2], ilitek_data->firmware_ver[3]);
#else
		return scnprintf(buf, PAGE_SIZE, "ilitek firmware version is %d.%d.%d.%d.%d.%d.%d.%d\n", ilitek_data->firmware_ver[0], ilitek_data->firmware_ver[1],
			       ilitek_data->firmware_ver[2], ilitek_data->firmware_ver[3], ilitek_data->firmware_ver[4], ilitek_data->firmware_ver[5],
			       ilitek_data->firmware_ver[6], ilitek_data->firmware_ver[7]);
#endif
	}
}

static DEVICE_ATTR(firmware_version, 0664, ilitek_firmware_version_show, NULL);

static ssize_t ilitek_module_name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	uint8_t tmpbuf[64];

	tp_log_info("\n");
	ilitek_irq_disable();
	ret = api_protocol_set_cmd(ILITEK_TP_CMD_GET_MODULE_NAME, NULL, tmpbuf);
	ilitek_irq_enable();
	return scnprintf(buf, PAGE_SIZE, "%s\n", tmpbuf);
}

static DEVICE_ATTR(product_id, 0664, ilitek_module_name_show, NULL);

static ssize_t ilitek_update_fw_show(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0, i = 0;
	struct device *tmp = dev_get_drvdata(dev);

	tp_log_info("path :%s, size=%zu\n", buf, size);
	if (size > 256) {
		tp_log_err("size > 256 not support size = %d\n", (int32_t)size);
	} else {
		for (i = 0; i < (size - 1); i++) {
			tp_log_info("%c\n", buf[i]);
			ilitek_data->upg.filename[i] = buf[i];
		}
		ilitek_data->upg.filename[size - 1] = '\0';
		tp_log_info("ilitek_data->upg.filename = %s\n", ilitek_data->upg.filename);
		ilitek_irq_disable();
		if(((ilitek_data->ptl.ver & 0xFFFF00) == BL_V1_8) || ilitek_data->ptl.ver >= PROTOCOL_V6) {
			ret = ilitek_bin_upgrade_6XX(tmp, ilitek_data->upg.filename);

		} else {
			ret = ilitek_bin_upgrade_3XX(tmp, ilitek_data->upg.filename);
		}
		if (ret < 0) {
			tp_log_err("ilitek_upgrade err ret = %d\n", ret);
			return size;
		}
		ilitek_irq_enable();
		tp_log_info("\n");
	}
	tp_log_info("\n");
	if (ret) {
		tp_log_err("The firmware update failed\n");
	} else {
		tp_log_info("The firmware update succeeded\n");
	}
	return size;
}

static DEVICE_ATTR(update_fw, 0664, NULL, ilitek_update_fw_show);

static struct attribute *ilitek_sysfs_attrs_ctrl[] = {
	&dev_attr_firmware_version.attr,
	&dev_attr_product_id.attr,
#ifdef ILITEK_GESTURE
	&dev_attr_gesture.attr,
#endif
	&dev_attr_update_fw.attr,
	NULL
};

static struct attribute_group ilitek_attribute_group[] = {
	{.attrs = ilitek_sysfs_attrs_ctrl},
};

int ilitek_create_sysfsnode(void)
{
	int ret = 0;
	struct i2c_client *client = ilitek_data->client;

	ilitek_data->ilitek_func_kobj = kobject_create_and_add("touchscreen", NULL);
	if (ilitek_data->ilitek_func_kobj == NULL) {
		tp_log_err("kobject_create_and_add failed\n");
	} else {
		ret = sysfs_create_group(ilitek_data->ilitek_func_kobj, ilitek_attribute_group);
		if (ret < 0) {
			tp_log_err("sysfs_create_group failed\n");
			kobject_put(ilitek_data->ilitek_func_kobj);
		}
	}
	ret = sysfs_create_group(&client->dev.kobj, ilitek_attribute_group);
		if (ret < 0) {
			tp_log_err("sysfs_create_group failed\n");
			kobject_put(&client->dev.kobj);
		}
	return ret;
}


int32_t ilitek_create_tool_node(void)
{
	int32_t ret = 0;
	// allocate character device driver buffer
	ret = alloc_chrdev_region(&ilitek_dev.devno, 0, 1, "ilitek_file");
	if (ret) {
		tp_log_err("can't allocate chrdev\n");
		//return ret;
	} else {
		tp_log_info("register chrdev(%d, %d)\n", MAJOR(ilitek_dev.devno), MINOR(ilitek_dev.devno));

		// initialize character device driver
		cdev_init(&ilitek_dev.cdev, &ilitek_fops);
		ilitek_dev.cdev.owner = THIS_MODULE;
		ret = cdev_add(&ilitek_dev.cdev, ilitek_dev.devno, 1);
		if (ret < 0) {
			tp_log_err("add character device error, ret %d\n", ret);
			//return ret;
		} else {
			ilitek_dev.class = class_create(THIS_MODULE, "ilitek_file");
			if (IS_ERR(ilitek_dev.class)) {
				tp_log_err("create class, error\n");
				//return ret;
			}
			device_create(ilitek_dev.class, NULL, ilitek_dev.devno, NULL, "ilitek_ctrl");
		}
	}
	ilitek_proc = proc_create("ilitek_ctrl", ILITEK_DEVICE_NODE_PERMISSON, NULL, &ilitek_fops);
	if (ilitek_proc == NULL) {
		tp_log_err("proc_create(ilitek_ctrl, ILITEK_DEVICE_NODE_PERMISSON, NULL, &ilitek_fops) fail\n");
	}
	ilitek_proc_irq_enable = proc_create("ilitek_irq_enable", ILITEK_DEVICE_NODE_PERMISSON, NULL, &ilitek_irq_enable_fops);
	if (ilitek_proc_irq_enable == NULL) {
		tp_log_err("proc_create(ilitek_irq_enable, ILITEK_DEVICE_NODE_PERMISSON, NULL, &ilitek_irq_enable_fops) fail\n");
	}
#ifdef ILITEK_TUNING_NODE
	ilitek_proc_debug = proc_create("ilitek_debug", ILITEK_DEVICE_NODE_PERMISSON, NULL, &ilitek_debug_fops);
	if (ilitek_proc_debug == NULL) {
		tp_log_err("proc_create(ilitek_debug, ILITEK_DEVICE_NODE_PERMISSON, NULL, &ilitek_debug_fops) fail\n");
	}
	ilitek_proc_debug_switch = proc_create("ilitek_debug_switch", ILITEK_DEVICE_NODE_PERMISSON, NULL, &ilitek_debug_fops_switch);
	if (ilitek_proc_debug_switch == NULL) {
		tp_log_err("proc_create(ilitek_debug, ILITEK_DEVICE_NODE_PERMISSON, NULL, &ilitek_debug_fops) fail\n");
	}
	init_waitqueue_head(&(ilitek_data->inq));
	ilitek_data->debug_data_frame = 0;
#endif
    ilitek_sensortest_init();
	ilitek_proc_entry = proc_mkdir("ilitek", NULL);
	if (!ilitek_proc_entry) {
		tp_log_err("Error, failed to creat procfs.\n");
	} else {
		if (!proc_create("firmware_version", ILITEK_DEVICE_NODE_PERMISSON, ilitek_proc_entry, &ilitek_proc_fops_fwversion)) {
			tp_log_err("Error, failed to creat procfs firmware_version.\n");
			remove_proc_entry("firmware_version", ilitek_proc_entry);
		}
		if (!proc_create("update_firmware", ILITEK_DEVICE_NODE_PERMISSON, ilitek_proc_entry, &ilitek_proc_fops_fwupdate)) {
			tp_log_err("Error, failed to creat procfs update_firmware.\n");
			remove_proc_entry("update_firmware", ilitek_proc_entry);
		}
		if (!proc_create("sensor_test_data", ILITEK_DEVICE_NODE_PERMISSON, ilitek_proc_entry, &ilitek_proc_fops_sensortest)) {
			tp_log_err("Error, failed to creat procfs sensor_test.\n");
			remove_proc_entry("sensor_test_data", ilitek_proc_entry);
		}
		if (!proc_create("noisefre_data", ILITEK_DEVICE_NODE_PERMISSON, ilitek_proc_entry, &ilitek_proc_fops_noisefre)) {
			tp_log_err("Error, failed to creat procfs noisefre_data.\n");
			remove_proc_entry("noisefre_data", ilitek_proc_entry);
		}
		if (!proc_create("setmode_0", ILITEK_DEVICE_NODE_PERMISSON, ilitek_proc_entry, &ilitek_proc_fops_setmode_0)) {
			tp_log_err("Error, failed to creat procfs setmode_0.\n");
			remove_proc_entry("setmode_0", ilitek_proc_entry);
		}
		if (!proc_create("setmode_1", ILITEK_DEVICE_NODE_PERMISSON, ilitek_proc_entry, &ilitek_proc_fops_setmode_1)) {
			tp_log_err("Error, failed to creat procfs setmode_1.\n");
			remove_proc_entry("setmode_1", ilitek_proc_entry);
		}
		if (!proc_create("setmode_2", ILITEK_DEVICE_NODE_PERMISSON, ilitek_proc_entry, &ilitek_proc_fops_setmode_2)) {
			tp_log_err("Error, failed to creat procfs setmode_1.\n");
			remove_proc_entry("setmode_2", ilitek_proc_entry);
		}
	}
	return 0;
}

void ilitek_remove_sys_node(void) {
	if (ilitek_data->ilitek_func_kobj) {
		sysfs_remove_group(ilitek_data->ilitek_func_kobj, ilitek_attribute_group);
		kobject_put(ilitek_data->ilitek_func_kobj);
		ilitek_data->ilitek_func_kobj = NULL;
	}
	if (&ilitek_data->client->dev.kobj) {
		sysfs_remove_group(&ilitek_data->client->dev.kobj, ilitek_attribute_group);
		kobject_put(&ilitek_data->client->dev.kobj);
		//&ilitek_data->client->dev.kobj = NULL;
	}
}

int32_t ilitek_remove_tool_node(void)
{
	cdev_del(&ilitek_dev.cdev);
	unregister_chrdev_region(ilitek_dev.devno, 1);
	device_destroy(ilitek_dev.class, ilitek_dev.devno);
	class_destroy(ilitek_dev.class);
	if (ilitek_proc) {
		tp_log_info("remove procfs ilitek_ctrl.\n");
		remove_proc_entry("ilitek_ctrl", NULL);
		ilitek_proc = NULL;
	}
	if (ilitek_proc_irq_enable) {
		tp_log_info("remove procfs ilitek_irq_enable.\n");
		remove_proc_entry("ilitek_irq_enable", NULL);
		ilitek_proc_irq_enable = NULL;
	}
#ifdef ILITEK_TUNING_NODE
	if (ilitek_proc_debug) {
		tp_log_info("remove procfs ilitek_ctrl.\n");
		remove_proc_entry("ilitek_debug", NULL);
		ilitek_proc_debug = NULL;
	}
	if (ilitek_proc_debug_switch) {
		tp_log_info("remove procfs ilitek_ctrl.\n");
		remove_proc_entry("ilitek_debug_switch", NULL);
		ilitek_proc_debug_switch = NULL;
	}
#endif
	if (ilitek_proc_entry) {
		tp_log_info("remove procfs firmware_version.\n");
		remove_proc_entry("firmware_version", ilitek_proc_entry);
		tp_log_info("remove procfs update_firmware.\n");
		remove_proc_entry("update_firmware", ilitek_proc_entry);
		tp_log_info("remove procfs sensor_test_data.\n");
		remove_proc_entry("sensor_test_data", ilitek_proc_entry);
		tp_log_info("remove procfs noisefre_data.\n");
		remove_proc_entry("noisefre_data", ilitek_proc_entry);
		tp_log_info("remove procfs ilitek.\n");
		remove_proc_entry("setmode_2", ilitek_proc_entry);
		tp_log_info("remove setmode_2 ilitek.\n");
		remove_proc_entry("setmode_1", ilitek_proc_entry);
		tp_log_info("remove setmode_1 ilitek.\n");
		remove_proc_entry("setmode_0", ilitek_proc_entry);
		tp_log_info("remove setmode_0 ilitek.\n");
		remove_proc_entry("ilitek", NULL);
		ilitek_proc_entry = NULL;
	}
	return 0;
}
#endif
