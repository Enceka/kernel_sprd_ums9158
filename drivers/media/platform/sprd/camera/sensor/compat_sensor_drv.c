/*
 * Copyright (C) 2021-2022 UNISOC Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include "sprd_sensor.h"
#include "compat_sensor_drv.h"

long compat_sensor_k_ioctl(struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	long err = 0;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	/* Stock 32-bit HAL encodes pointer-bearing commands with compat sizes,
	 * and their payloads use 32-bit pointers. The core converts those layouts.
	 * Match the stock driver's command translation before exact dispatch.
	 */
	switch (cmd) {
	case _IOC(_IOC_WRITE, SENSOR_IOC_MAGIC, 14, 16):
		cmd = SENSOR_IO_I2C_WRITE_REGS;
		break;
	case _IOC(_IOC_WRITE, SENSOR_IOC_MAGIC, 19, 12):
		cmd = SENSOR_IO_GRC_I2C_WRITE;
		break;
	case _IOC(_IOC_READ | _IOC_WRITE, SENSOR_IOC_MAGIC, 20, 12):
		cmd = SENSOR_IO_GRC_I2C_READ;
		break;
	case _IOC(_IOC_WRITE, SENSOR_IOC_MAGIC, 23, 80):
		cmd = SENSOR_IO_MUTI_I2C_WRITE;
		break;

	default:
		break;
	}
	err = filp->f_op->unlocked_ioctl(filp, cmd,
			(unsigned long)compat_ptr(arg));

	return err;
}
