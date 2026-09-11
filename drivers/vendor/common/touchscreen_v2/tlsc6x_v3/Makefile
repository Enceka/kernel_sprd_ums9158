# SPDX-License-Identifier: GPL-2.0
#
# TLSC6X touchscreen driver.
#
# Upstream ccflags-y pointed at drivers/vendor/common/touchscreen/ (no _v2) and
# at tlsc6x_v3-relative firmware/chip dirs; only the latter are correct in this
# tree, the former does not exist.

ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen_v2/
ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen_v2/tlsc6x_v3/
ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen_v2/tlsc6x_v3/firmware_config/$(CONFIG_TOUCHSCREEN_BOARD_NAME)/
ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen_v2/tlsc6x_v3/tlsc_chip3535/
ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen_v2/tlsc6x_v3/tlsc_chip3536/

obj-$(CONFIG_TOUCHSCREEN_TLSC6X_V3)  += tlsc6x_ts.o
tlsc6x_ts-objs := tlsc6x_main.o
tlsc6x_ts-objs += tlsc6x_comp.o
tlsc6x_ts-objs += tlsc6x_common_interface.o
