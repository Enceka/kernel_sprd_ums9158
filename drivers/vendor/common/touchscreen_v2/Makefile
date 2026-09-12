# SPDX-License-Identifier: GPL-2.0
#
# ZTE touchscreen framework + tlsc6x, ported from
# android_kernel_zte_ums9620_mifi_u30air/drivers/vendor/common/touchscreen_v2.
#
# The original Makefile also links ~25 per-chip drivers into zte_tpd; we build
# only the shared core and the tlsc6x chip driver.
#
# Both halves go into ONE module named tlsc6x.ko, which is how the stock image
# ships them: /vendor/lib/modules has tlsc6x.ko and no zte_tpd.ko.  Splitting
# them in two does not work - the chip driver uses tpd_cdev, get_tp_chip_id(),
# change_tp_state(), tpd_copy_to_tp_firmware_data() and
# tpd_zlog_record_notify() from the core, and upstream exports none of them
# (upstream declares the chip driver bool, i.e. always same-module).
#
# NOTE: the upstream tree adds -I$(srctree)/drivers/vendor/common/touchscreen/
# (no _v2) in tlsc6x_v3/Makefile - that path does not exist here, so the include
# paths below are the only ones that should be used.

ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen_v2/
ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen_v2/tlsc6x_v3/
ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen_v2/tlsc6x_v3/firmware_config/$(CONFIG_TOUCHSCREEN_BOARD_NAME)/
ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen_v2/tlsc6x_v3/tlsc_chip3535/
ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen_v2/tlsc6x_v3/tlsc_chip3536/

obj-$(CONFIG_TOUCHSCREEN_VENDOR_V2) += tlsc6x.o

tlsc6x-y := ztp_core.o ztp_report_algo.o ztp_state_change.o
tlsc6x-$(CONFIG_TOUCHSCREEN_UFP_MAC)    += ztp_ufp.o
tlsc6x-$(CONFIG_TOUCHSCREEN_LCD_NOTIFY) += lcd_state_notify.o

tlsc6x-$(CONFIG_TOUCHSCREEN_TLSC6X_V3)  += tlsc6x_v3/tlsc6x_main.o \
					   tlsc6x_v3/tlsc6x_comp.o \
					   tlsc6x_v3/tlsc6x_common_interface.o
