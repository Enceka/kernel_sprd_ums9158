# SPDX-License-Identifier: GPL-2.0
# sprd_dvfs_vsp.h lives with the apsys devfreq drivers.
ccflags-y += -I$(srctree)/drivers/devfreq/apsys
obj-$(CONFIG_UNISOC_VPU) += vpu.o
vpu-y := sprd_vpu.o common_drv.o vpu_r1p0.o \
	 vsp_hevc_lite.o vsp_1080p_r1p0.o vsp_hevc_lite_r1p0.o
