# E5 触摸驱动移植 — 前期准备记录

状态：**移植改造完成，尚未在 defconfig 启用**（Kconfig 全部 `default n`，defconfig 未加条目，
不影响当前编译）。§4 的三项改造已全部落地或确认无需改动，§5 的未决问题已全部解决 —— 详见
各节的「✅ 已完成」标注。下一步只剩 §6 的启用与编译验证。

> 2026-09-11 补充：芯片型号已在**运行中的设备上实测确证**（不再只是 DT 推断），
> 见 §1。

## 1. 目标芯片：tlsc6x（已确证）

设备 DT（`kernel_probe/_dev.dts`）里 i2c@2270000 下有两个触摸节点，实际使用的是 tlsc6x：

```dts
tlsc6x@2e {
    TP_MAX_X = <0x140>;          /* 320 */
    TP_MAX_Y = <0x1e0>;          /* 480，与面板 320x480 一致 */
    have-virtualkey = <0x00>;
    tlsc6x,irq-gpio   = <0x9e 0x0d 0x00>;
    tlsc6x,reset-gpio = <0x9e 0x0e 0x00>;
    reg = <0x2e>;
    compatible = "tlsc6x,tlsc6x_ts";
};

sitronix_touch@29 {
    compatible = "sitronix,st1633i", "sitronix,cf1216", "sitronix,cf1133";
};
```

### ✅ 设备实测确证（2026-09-11，原厂系统运行中）

此前的判断依据是 DT + 原厂 `.ko` 文件名（属于推断）。现已在设备上直接读到运行时证据，
**确认实际工作的是 tlsc6x，sitronix 那颗并未被使用**：

```
# /proc/bus/input/devices
N: Name="tlsc6x_touch"          ← 实际注册的 input 设备
S: Sysfs=/devices/virtual/input/input40
H: Handlers=event1 sysdump

# /sys/bus/i2c/devices/3-002e/
name                 -> tlsc6x_ts
of_node/compatible   -> tlsc6x,tlsc6x_ts
modalias             -> of:Ntlsc6xT(null)Ctlsc6x,tlsc6x_ts
supplier             -> platform:64170000.gpio
tlsc_tp_info / tlsc_version / tlsc_esd_ctl / tlsc_reset_ctl ... ← 驱动自建的 sysfs 节点
```

注意实际总线是 **i2c-3**（`3-002e`），而非 plan 早期依据 DT 写的 `i2c@2270000` 编号；
地址 `0x2e` 与 DT 的 `reg = <0x2e>` 一致。`tlsc_version` 读出 `Boot version = 0000`、
`Vender id = 0`，说明原厂固件未写入版本信息，这属正常（该驱动允许固件无版本号）。

其余依据（仍然成立）：
- 原厂 `vendor_dlkm` 里同时有 `tlsc6x.ko`(1.09MB) 和 `sitronix_touch.ko`(2.28MB)；
- `tlsc6x_v3/tlsc6x_main.c` 的 `TS_NAME` 是 `"tlsc6x_ts"`，`of_device_id` 是
  `{.compatible = "tlsc6x,tlsc6x_ts"}` —— 与 DT 完全对应；
- sitronix 那颗对应的 `st1633i/cf1216/cf1133` 驱动**全工作区都没有源码**，ZTE 树里的
  sitronix 是 `st7123`（不同芯片）。故 sitronix 暂不处理。

## 2. 已搬运的文件

```
drivers/vendor/common/touchscreen_v2/           <- ZTE 框架（共享 core）
    ztp_core.c  ztp_core.h  ztp_common.h
    ztp_report_algo.c  ztp_state_change.c
    ztp_ufp.c  ztp_ufp.h  lcd_state_notify.c
    tlsc6x_v3/                                  <- 驱动
        tlsc6x_main.c  tlsc6x_main.h  tlsc6x_comp.c  tlsc6x_common_interface.c
        firmware_config/{chestnut,dates,pitaya,plum}/{comp_upd_bin.h,tlsc6x_config.h}
        tlsc_chip3535/  tlsc_chip3536/
include/vendor/common/                          <- 框架头文件
    zte_tpd.h  zte_lcd_notifier.h  zte_misc.h  vendor_cfg_helper.h
include/vendor/comdef/
    zlog_common_base.h
```

来源：`android_kernel_zte_ums9620_mifi_u30air`（5.4.254 UMS9620 SDK）。

框架规模约 140 KB C 代码 + 22 KB 头文件，`ztp_core.c` 的 include 全是标准内核头文件，
`zlog_common_base.h` 只依赖 `<linux/miscdevice.h>` —— 没有更深的外部依赖。

## 3. 已完成的接线（均默认关闭）

| 文件 | 内容 |
|---|---|
| `drivers/vendor/Kconfig` / `Makefile` | 新建，转发到 touchscreen_v2 |
| `drivers/Kconfig` | 加 `source "drivers/vendor/Kconfig"` |
| `drivers/Makefile` | 加 `obj-y += vendor/` |
| `touchscreen_v2/Kconfig` | 精简版（原版 source 了 ~25 个无关芯片驱动）|
| `touchscreen_v2/Makefile` | `zte_tpd.o` = ztp_core + ztp_report_algo + ztp_state_change（+ 可选 ufp/lcd_notify）|
| `tlsc6x_v3/Kconfig` | `bool` → **`tristate`**，加 `depends on TOUCHSCREEN_VENDOR_V2` |
| `tlsc6x_v3/Makefile` | 修正 include 路径（原版指向不存在的 `touchscreen/`）|

## 4. 移植时必须改的地方（重要）

### 4.1 GPIO 获取方式 —— 不改就 probe 不起来 ★ ✅ 已修复（`b486a2cde`）

驱动用的是**下标式**接口，顺序还是 reset → irq：

```c
/* tlsc6x_main.c:966 / 972 */
pdata->reset_gpio_number = of_get_gpio(np, 0);
pdata->irq_gpio_number   = of_get_gpio(np, 1);
```

但设备 DT 用的是**命名属性** `tlsc6x,reset-gpio` / `tlsc6x,irq-gpio`，
没有 `gpios` 属性 —— `of_get_gpio()` 会返回 -ENOENT，驱动直接退出。

必须改成：

```c
pdata->reset_gpio_number = of_get_named_gpio(np, "tlsc6x,reset-gpio", 0);
pdata->irq_gpio_number   = of_get_named_gpio(np, "tlsc6x,irq-gpio", 0);
```

### 4.2 5.15 内核 API 变更

| 位置 | 原写法 | 需改为 | 状态 |
|---|---|---|---|
| `tlsc6x_main.c`、`tlsc6x_comp.c` | `#include <asm/uaccess.h>` | `<linux/uaccess.h>` | ✅ 已改 |
| `tlsc6x_main.c` | `#include <linux/wakelock.h>` | 无需改动，见下 | ✅ 确认无需处理 |

**更正**：早前登记的「wakelock 待改」是**误判**（只看了 `grep wake_lock` 的命中行，
没看外层 `#ifdef`）。实际上这些调用点全部包在版本分支里，5.15 走的是现代 API 分支：

```c
/* 声明处 tlsc6x_main.c:91-99 */
#ifdef CONFIG_PM_WAKELOCKS
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
static struct wakeup_source *tlsc6x_wakelock;      ← 我们走这条
#else
static struct wakeup_source tlsc6x_wakelock;
#endif
#else
static struct wake_lock tlsc6x_wakelock;           ← 只有这条才用旧 API
#endif
```

`init`（:1957）用 `wakeup_source_register(NULL, ...)`、持锁（:1048、:1465）用
`__pm_wakeup_event(tlsc6x_wakelock, 2000)`，均为 5.15 现存 API，签名也对得上
（`include/linux/pm_wakeup.h:102/205`）。

前提条件已满足：defconfig 有 `CONFIG_PM_WAKELOCKS=y`（第 76 行），
且 `LINUX_VERSION_CODE`(5.15) ≥ 4.19，因此走的是第一条分支，
`<linux/wakelock.h>`（5.15 已删除）所在的 `#else` 分支**根本不会被编译**。

> 附带结论：**`CONFIG_PM_WAKELOCKS` 不能关**。一旦关掉，就会落到最外层 `#else`
> 去引用已不存在的 `<linux/wakelock.h>` 和 `struct wake_lock`，直接编译失败。

### 4.3 遗留配置宏

代码里出现 `CONFIG_HAS_EARLYSUSPEND`、`CONFIG_PM_WAKELOCKS`、
`CONFIG_TOUCHSCREEN_UFP_MAC`、`_POINT_REPORT_CHECK`、`_PSENSOR_REPORT_CHECK`、
`_KNUCKLE`、`CONFIG_CREATE_TPD_SYS_INTERFACE`、`CONFIG_VENDOR_ZTE_LOG_EXCEPTION`。
前两个是 3.x/4.x 时代的，需要确认对应的 `#ifdef` 分支在 5.15 下能走通（多半会走 else 分支，
需逐个检查）。其余已在简化版 Kconfig 里给出开关，默认 n。

### 4.4 DT 缺 `vdd_name`

驱动读 `vdd_name` 取 regulator，设备 DT 里没有这个属性。代码里是
`WARN(IS_ERR(reg_vdd), ...)` 的形式（`tlsc6x_main.c:929`），**不致命**，
但要确认后续 `regulator_enable()` 的失败分支不会 return 掉整个 probe。

## 5. 未决问题

1. ~~**`CONFIG_TOUCHSCREEN_BOARD_NAME` 取哪个值？**~~ ✅ **已解决（`8d706a2fa`）**

   比对结果出人意料：上游四个项目代号目录 `chestnut` / `dates` / `pitaya` / `plum`
   **内容完全相同**（两个文件的 md5 都一模一样）：

   ```
   tlsc6x_config.h   4 个目录 md5 均为 3911a929...（各 26 行，逐字节相同）
   comp_upd_bin.h    4 个目录 md5 均为 f77928a5...（各 139954 字节，逐字节相同）
   ```

   也就是说，这四个"板级配置"其实从未真正分化过，选哪个在编译结果上没有任何区别 ——
   原先担心的"哪个更接近 E5"是个伪问题。

   处理方式：新建 `firmware_config/e5/`，`comp_upd_bin.h` 沿用同一份固件，
   `tlsc6x_config.h` 按 E5 实际情况裁剪（关掉 proximity 等本机没有的特性），
   并把 Kconfig 默认值设为 `default "e5" if TOUCHSCREEN_TLSC6X_V3`，
   这样不必在 defconfig 里显式写 `CONFIG_TOUCHSCREEN_BOARD_NAME` 也不会指向空目录。
2. **uart/misc 相关**：`CONFIG_TOUCHSCREEN_LCD_NOTIFY` 要不要开？
   设备是 MIPI 屏，`lcd_state_notify.c` 用于屏状态联动 TP 挂起/唤醒，
   建议先关（default n）跑通基本触摸再说。
3. **是否保留 ZTE 的 proc/sysfs 调试接口**（`/proc/tlsc6x-debug`、`tpd_attributes`）——
   保留无害，但会多出一些 proc 节点。

## 6. 启用步骤（等 USB/电池确认后再做）

```sh
# 1. defconfig 加（先只开最小集合）
#    BOARD_NAME 不必写，Kconfig 已 default "e5"；PM_WAKELOCKS 已是 y，见 §4.2
CONFIG_TOUCHSCREEN_VENDOR_V2=m
CONFIG_TOUCHSCREEN_TLSC6X_V3=m

# 2. 编译，按报错逐个解决 4.2 / 4.3 / 4.4
./build_e5.sh

# 3. 确认产出 tlsc6x_ts.ko 和 zte_tpd.ko

# 4. 打包时需要：把两个 .ko 加进 vendor_dlkm，并登记 fs_config + file_contexts，
#    在 modules.load 中排在 i2c-sprd / pinctrl-sprd-qogirn6lite 之后
```

## 7. 备份

原文保留在 `android_kernel_zte_ums9620_mifi_u30air/drivers/vendor/common/touchscreen_v2/`，
本次拷贝未做任何修改（除 4.2 待改项）。
