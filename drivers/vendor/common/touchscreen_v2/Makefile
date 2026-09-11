# SPDX-License-Identifier: GPL-2.0
#
# ZTE touchscreen framework + tlsc6x, ported from
# android_kernel_zte_ums9620_mifi_u30air/drivers/vendor/common/touchscreen_v2.
#
# The original Makefile also links ~25 per-chip drivers into zte_tpd; we build
# only the shared core and descend into tlsc6x_v3.
#
# NOTE: the upstream tree adds -I$(srctree)/drivers/vendor/common/touchscreen/
# (no _v2) in tlsc6x_v3/Makefile - that path does not exist here, so the include
# paths below are the only ones that should be used.

ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen_v2/
ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen_v2/tlsc6x_v3/
ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen_v2/tlsc6x_v3/firmware_config/$(CONFIG_TOUCHSCREEN_BOARD_NAME)/
ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen_v2/tlsc6x_v3/tlsc_chip3535/
ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen_v2/tlsc6x_v3/tlsc_chip3536/

obj-$(CONFIG_TOUCHSCREEN_VENDOR_V2) += zte_tpd.o
zte_tpd-y := ztp_core.o ztp_report_algo.o ztp_state_change.o
zte_tpd-$(CONFIG_TOUCHSCREEN_UFP_MAC) += ztp_ufp.o
zte_tpd-$(CONFIG_TOUCHSCREEN_LCD_NOTIFY) += lcd_state_notify.o

obj-$(CONFIG_TOUCHSCREEN_TLSC6X_V3) += tlsc6x_v3/
