# SPDX-FileCopyrightText: 2016-2023 Unisoc (Shanghai) Technologies Co. Ltd
# SPDX-License-Identifier: GPL-2.0-only
#
# sprd_fm.ko - Unisoc WCN combo chip FM radio driver.
#
# Imported from android_kernel_modules_and_devicetree_oppo_ums9230
# (bsp/modules/kernel5.15/wcn/fm/driver/fm_515), adapted from its
# out-of-tree Kbuild to a normal in-tree obj-$(CONFIG_x) build.

ccflags-y += -I$(srctree)/include/misc

obj-$(CONFIG_UNISOC_WCN_FM) += sprd_fm.o

sprd_fm-y := \
	fm_515/fm_rf_marlin3.o \
	fm_515/fmdrv_main.o \
	fm_515/fmdrv_ops.o \
	fm_515/fmdrv_rds_parser.o
