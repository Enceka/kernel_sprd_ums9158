# E5 (kernel_ts305_ums9158) 诊断记录 — 2026-09-11

本轮修掉了挡住开机的最后一个问题（GSP capability 的 ioctl ABI 错位），并顺带发现
两个此前一直被掩盖的系统性问题：**全量编译早就失败**、**vendor_dlkm 的模块几乎全都
装不进去**。

## 结论速览

| 目标 | 状态 | 依据 |
|---|---|---|
| 显示链路 probe 全 0 | ✅ | ylog 174：`sprd-gsp`/`sprd-drm`/`dpu`/`dsi` 均 `returned 0`，不再 -517，见 §1 |
| GSP capability | ✅ 修复并在设备上验证 | `version='init' size=92` → `version='R9P0' size=208`，无 copy error |
| HWC HAL | ✅ 不再段错误 | android.log：`SprdDrm:: Init success`、注册 `IComposer/default` |
| 全量编译 | ✅ 通过（此前长期失败）| 见 §2 |
| vendor_dlkm 模块 | ⚠️ 已定位修复，镜像待刷 | 128/131 模块拒载，见 §3 |
| 电池 / USB(adb) | ⚠️ 见 §3 | 与模块拒载现象一致 |
| 触摸 | ✅ 已启用并编译通过 | 4 处 API 断裂已修，见 §5 |
| WCN 三驱动 | ✅ 编译通过 | 2 处断裂已修，见 §5b |

---

## 1. GSP capability：ioctl 结构体 ABI 错位（已修复并验证）

### 根因

`include/uapi/drm/sprd_drm_gsp.h` 的两个结构体缺了 `char version[32]`：

| 布局 | gsp_id | size | version | cap | sizeof |
|---|---|---|---|---|---|
| 修复前 | 0 | 4 | — | **8** | 16 |
| 修复后 / 原厂 | 0 | 4 | 8..39 | **40** | 48 |

`gsp_id`(0) 和 `size`(4) 在两种布局下**偏移相同**，这正是它隐蔽的原因 —— 设备号对、
size 对，驱动一路走到最后一个 copy 才出错。HAL 把 `cap` 写在 offset 40，我们从
offset 8 读，而 offset 8 恰好是 HAL 填的版本字符串（`"R9P0"`），于是变成
`copy_to_user(0x30503952, ...)` → -EFAULT → HAL 段错误启动循环。

### 证据（四条独立印证）

1. **原厂反汇编**：`sprd_gsp_get_capability_ioctl` 用 `ldr x20, [x20, #0x28]` 取
   `cap`，0x28 = 40；
2. **原厂 `sprd_gsp_trigger_ioctl`** 用 `[x22, #0x30]`（48）取 `config`，对应
   `split`(12) + `version[32]`(13..44) + 对齐；
3. **同平台 OPPO UMS9230 树**（`bsp/modules/kernel5.15/display/`）仍保留
   `strcpy(version, drm_capa->version)` 和 `"frist get capality"` /
   `"board version is: %s"` 字符串，与原厂内核日志逐字对应；
4. **设备侧 ylog 174**：

```
sprd-gsp 30130000.sprd-gsp: cap req: gsp_id=0 size=92  version='init' cap=b400007d85d4a23c
sprd-gsp 30130000.sprd-gsp: io_cnt:7, core_cnt:1 ,size:92, cap->size:208
sprd-gsp 30130000.sprd-gsp: cap req: gsp_id=0 size=208 version='R9P0' cap=b400007d95d52a70
sprd-gsp 30130000.sprd-gsp: io_cnt:7, core_cnt:1 ,size:208, cap->size:208
```

`cap` 是合法用户态指针（不再是 `0x30503952`），无任何 `copy error`。这与原厂驱动的
校验分支完全一致：首次查询 `size==GSP_CAPA && version=="init"`，随后
`size==R9P0_CAPA && version=="R9P0"`。

### 提交

`90ee37c48`（uapi 结构体）、`39a6c6865`（驱动侧读取 + 诊断 + gsp_id 边界检查）、
`9043122e7`（32 位 compat 结构对齐）。

---

## 2. 全量编译长期失败：charger-manager 的 .c / .h 错配

### 现象

`./build_e5.sh` 在 `drivers/power/supply/charger-manager.c` 处中断：

```
error: no member named 'num_charger_regulators' in 'struct charger_desc'
```

### 定位

| 文件 | 来源 |
|---|---|
| `include/linux/power/charger-manager.h` | Unisoc 版（与 `motorola-...-uoa34` **逐字节相同**，796 行差异 vs Google 基线）|
| `drivers/power/supply/charger-manager.c` | Google 基线（仅多一个 `cm_notify_event`）|

`.c` 引用了 Unisoc 头文件里**不存在**的 `desc->num_charger_regulators` /
`desc->charger_regulators`，两者的配对关系是错的。

**触发点**：提交 `f7f566d7e e5: enable CHARGER_MANAGER` 打开了
`CONFIG_CHARGER_MANAGER=m`，把这个文件拉进了编译。在此之前它从未被编译过
（`out_e5/drivers/power/supply/` 下一直没有 `charger-manager.o`）。

### 修复

用头文件同源的那棵树（`motorola-sprd-andriod-14-release-uoa34`）的 `.c`（9060 行，
是超集，已含 `cm_notify_event`）。提交 `c03d3f498`。

### 后果（重要）

**在这之前的所有刷机，用的都是陈旧模块。** `sprd-drm.ko` / `sprd-gsp.ko` 的时间戳
在 2026-09-10 16:05 之前就没动过，`strings` 检查证实它们**不含任何此前加的诊断
串**（`fails access_ok` 0 匹配）—— 即之前几轮测试看到的都还不是打过补丁的驱动。

---

## 3. vendor_dlkm 模块拒载：内核版本变了，镜像里的模块没跟着变

### 现象

设备可开机、显示正常，但**电池读不到、USB 不工作（没有 adb）**。

### 根因

内核因 rebase 从 `g7ed1875c73a3` 变成 `f74c891fa5c3`，而 `vendor_dlkm` 里
131 个模块只有 3 个（当轮手动替换的显示三件套）是新版：

| 模块数 | vermagic | 能否加载 |
|---|---|---|
| 3 | `f74c891fa5c3`（新内核）| ✅ |
| 53 | `g7ed1875c73a3`（旧构建）| ❌ |
| 75 | `5.15.119`（原厂）| ❌ |

设备侧 ylog 174 印证：**169 次 `Loading module` 中 218 条**
`disagrees about version of symbol module_layout` / `Exec format error`，
最终只有 90 个（全部来自 vendor_boot ramdisk 里我们新编的模块）加载成功。

其中 `sprd-charger-manager.ko`（`modules.load` 第 72 行）正是电池与 USB 的提供者，
缺失后两个症状同时出现。

### 打包流程的陷阱

1. **vendor_dlkm 重打包只替换「同名」模块** —— 新启用的 `=m` 配置项编出来的 `.ko`
   永远不会被放进去。本轮 `f7f566d7e` 把 `CHARGER_MANAGER` 改成 `=m`，就正好踩到。
2. **替换时不能用 `find out_e5 -name`** —— `out_e5` 里有历代遗留的 stale `.ko`，
   会命中旧文件。必须按 `out_e5/modules.order` 逐条对路径替换。
3. **依赖图要用 `depmod` 重建** —— 各模块 `.modinfo` 的 `depends=` 与符号级依赖
   未必一致（`sprd-gsp.ko` 的 `depends=` 是空的，但符号上依赖 `apsys-dvfs` /
   `unisoc-iommu`）。用 `depmod` 重新生成后与原厂 `modules.dep` 逐字一致。
4. 新模块必须同时登记进 `fs_config` 和 `file_contexts`（后者是**逐文件**规则，
   没有通配符）。

### 修复

按 `modules.order` 替换 56 个同名模块、`depmod` 重建依赖、登记新文件，
重打包 `stock-img/vendor_dlkm_e5.img`（60.8 MB，146 条目）。

---

## 4. 内置 / 模块配置清点（为恢复 `=m` 做准备）

此前把一批驱动改成 built-in，多半是因为模块加载不了。现按**原厂镜像里的
`modules.load` 与 `.ko` 文件**判定原厂原本怎么配：

**可恢复为 `=m`（36 项，原厂均为模块）**

| 分组 | 配置项 |
|---|---|
| 存储/时钟/PMIC | `SCSI_UFS_SPRD` `MMC_SDHCI_SPRD` `MMC_SWCQ` `MMC_HSQ` `SPRD_COMMON_CLK` `SPRD_UMS9621_CLK` `SPRD_PMIC_SYSCON` `MFD_SC27XX_PMIC` `SPRD_EFUSE` `SC27XX_EFUSE` `POWER_RESET_SC27XX` `SPRD_SYSTIMER` `PWM_SPRD` |
| 总线/引脚/GPIO | `I2C_SPRD` `I2C_SPRD_HW_V2` `SPI_SPRD` `SPI_SPRD_ADI` `PINCTRL_SPRD` `PINCTRL_SPRD_QOGIRN6LITE` `GPIO_EIC_SPRD` `GPIO_PMIC_EIC_SPRD` `GPIO_SPRD` `SPRD_COMMONPHY` |
| DVFS/IOMMU/GPU | `DVFS_APSYS_SPRD` `SPRD_DMC_DRV` `DEVFREQ_SPRD_DDR_DVFS` `UNISOC_IOMMU` `UNISOC_GPU_COOLING_DEVICE` `MALI_MIDGARD` `LEDS_SC27XX_BLTC` |
| 热/安全/标识 | `SPRD_THERMAL_R5P0` `SPRD_SHELL_THERMAL` `SPRD_UMP96XX_TSENSOR` `UNISOC_THERMAL_CTL` `TRUSTY` `SPRD_UID` `SPRD_SOCID` |

**保持内置（4 项，原厂连 `.ko` 文件都没有）**

`SPRD_THERMAL`（框架）· `SPRD_SKIN_THERMAL` · `DEVFREQ_GOV_SPRD_VOTE` · `DRM_PANFROST`

> 风险提示：恢复 `=m` 后，UFS/eMMC/PMIC/时钟/pinctrl 都要靠 vendor_boot ramdisk
> 加载。顺序错一步就是「秒重启 + 无日志」（见 2026-09-10 文档 §4）。我们的
> vendor_boot 打包脚本本就沿用原厂 83 条的加载顺序，模块存在即会自动排到正确位置。

---

## 5. 触摸：驱动从未编译

E5 的触摸 IC 是 `sitronix_touch@29` / `tlsc6x@2e`（`i2c@2270000`），我们的树里
**没有任何触摸驱动源码**，`out_e5` 也从未产出 `sitronix*` / `tlsc6x*` 模块。

源码位置已找到：

```
android_kernel_zte_ums9620_mifi_u30air/drivers/vendor/common/touchscreen_v2/
    sitronix_incell/     sitronix_ts.c
    sitronix_incell_1/
    tlsc6x_v3/           tlsc6x.c
```

注意设备用的是**自己的 dtbo 分区**，触摸节点本来就存在，所以缺的只是驱动。

### 进展（本轮）

芯片型号已**在运行中的设备上实测确证**是 tlsc6x（此前只是 DT + 原厂 `.ko` 文件名推断）：
`/proc/bus/input/devices` 里注册的是 `Name="tlsc6x_touch"`，
`/sys/bus/i2c/devices/3-002e/` 的 `name` / `compatible` 分别是
`tlsc6x_ts` / `tlsc6x,tlsc6x_ts`，并带有该驱动自建的 `tlsc_*` sysfs 节点。
**sitronix 那颗确实没在用**，可以不管。

移植侧三项改造已落地（详见 `touch_port_plan.md`）：

- `b486a2cde` —— DT 用命名属性 `tlsc6x,{reset,irq}-gpio`，而驱动用的是下标式
  `of_get_gpio(np, 0/1)`，会返回 `-ENOENT` 且是致命分支（`goto fail`），
  **不改则必然 probe 失败**。已改为 `of_get_named_gpio()`。
- `8d706a2fa` —— 新建 `firmware_config/e5/` 并把 Kconfig 默认值设为 `"e5"`。
  过程中发现上游四个项目代号目录（chestnut/dates/pitaya/plum）**内容逐字节完全相同**，
  所以"选哪个更接近 E5"本是个伪问题。
- wakelock 一项经复核**属早前误判**，无需改动：调用点都在
  `CONFIG_PM_WAKELOCKS` + `LINUX_VERSION_CODE >= 4.19` 分支内，
  走的已是 5.15 现存 API；引用已删除头文件的是另一条不会被编译的 `#else` 分支。
  （反过来说：**`CONFIG_PM_WAKELOCKS` 不能关掉**，否则会落到那条分支上编译失败。）

### 编译验证（已完成）

**更正**：此前写的「本会话无构建环境」是错的 —— Homebrew clang 可以直接交叉编译
aarch64，内核构建系统本身也能在 macOS 上跑起来。已固化成
`artifacts_e5/macos_compile_check.sh`（`7628b5527`），限制写在脚本头部。

驱动已在 defconfig 启用（`8f3d0ba8d`），产出 `zte_tpd.o` + `tlsc6x_ts.o`，
零错误零告警。过程中修掉 3 处 5.4→5.15 断裂：

- `78c0a57ce` —— **25 个错误全是同一个**：5.6 起 `proc_create()` 要求
  `const struct proc_ops *`，不再接受 `file_operations`。28 个 procfs 节点
  （ztp_core.c 27 个 + tlsc6x-debug）全部转换：`.read/.write` →
  `.proc_read/.proc_write`，并删掉 `.owner` —— `struct proc_ops` 没有这个成员，
  模块生命周期由 procfs 自己的 `pde_users` 引用计数处理（这正是上游拆分两个结构体时
  去掉该字段的原因）。
- `452768baa` —— 两处**只在 E5 板级配置下才暴露**的问题（上游四个 board config
  都开着这两个宏，从没编译过这条分支）：
  - `find_3535last_valid_burn_cfg()` 定义在 `#ifdef TLSC_AUTO_UPGRADE` 内，
    调用点 `tlsx6x_3535find_lastvaild_ver()` 却没有守卫；加上同样的守卫后退化为
    「没找到已烧录配置」，正是调用方本来就要处理的状态。
  - `test_val` 仅在 `#ifdef TLSC_TPD_PROXIMITY` 内使用 → `-Werror=unused-variable`。

另外发现一个 Kconfig 陷阱（已在 `8f3d0ba8d` 里规避）：`TOUCHSCREEN_BOARD_NAME`
的 `default "e5" if TOUCHSCREEN_TLSC6X_V3` **只在该符号尚无取值时生效**。若先在
TLSC6X 关闭的状态下跑过一次 `olddefconfig`，`.config` 会记下
`BOARD_NAME=""`，之后再打开 TLSC6X 也不会重算 —— 空串会让
`firmware_config/<name>/` 的 include 路径指向不存在的目录。所以 defconfig 里把它
显式写死，不依赖 default。

---

## 5b. WCN（WiFi / BT / FM / GNSS）：芯片对应关系已完整验证

导入三个 WCN 驱动（`5f465ad54` FM、`dd251524b` BT、`49a844cb6` WLAN）并启用
（`b74489a84`）后，逐环核对了「defconfig 选的芯片」是否真的等于「设备上的芯片」。
**结论：完全对应，无需改动。** 证据链如下（每一环都是实据，非推断）：

| 环节 | 证据 |
|---|---|
| 设备属性 | `ro.vendor.wcn.hardware.product = marlin3_lite`、`ro.vendor.gnsschip = marlin3lite` |
| 设备 DT | `/sys/firmware/devicetree/base/sprd-marlin3/compatible` = `unisoc,marlin3lite_sdio` |
| 驱动匹配 | `sprd_wcn.c:148` `{ .compatible = "unisoc,marlin3lite_sdio", .data = &g_marlin3lite_sdio_data }` |
| 匹配数据 | `sprd_wcn.c:92` `g_marlin3lite_sdio_data = { .unisoc_wcn_sdio, .unisoc_wcn_slp, .unisoc_wcn_m3lite = true }` |
| 型号换算 | `sprd_wcn.h:29` `bool unisoc_wcn_m3lite; //UMW2652`；`umw2652_glb.h:24` `/* UMW2652 is the lite of sc2355 */` |
| defconfig | `CONFIG_SC23XX=y`(721) + `CONFIG_UMW2652=y`(722) |

即 **marlin3lite ≡ m3lite ≡ UMW2652**，defconfig 选的正是这颗。

三个上层驱动也与 DT 子节点一一对上 —— `sprd-marlin3` 节点下正好有
`wlan` / `sprd-mtty`(BT) / `sprd-fm` 三个子节点，对应
`UNISOC_WLAN_COMBO` / `UNISOC_WCN_BT` / `UNISOC_WCN_FM`；
原厂运行中实际加载的也正是这四个：

```
sprd_fm  sprdbt_tty  sprd_wlan_combo  wcn_bsp(被前三者共同依赖)
```

### 排查过程中虚惊两次（记录下来避免重复怀疑）

1. **`CONFIG_UMW2652` 依赖 `SC23XX`（`default n`），会不会被 `olddefconfig` 静默丢弃？**
   —— 不会。`CONFIG_SC23XX=y` 就在 defconfig 第 721 行（紧邻 722 行的 `UMW2652`）。
   第一次没发现是因为 grep 模式（`WCN|WLAN|GNSS|...`）不匹配 `SC23XX` 这个名字。
2. **defconfig 里完全没有 `CONFIG_WLAN`，WiFi 会不会起不来？** —— 不影响。
   `UNISOC_WLAN_COMBO` 只 `depends on CFG80211`(=m，已设) 和 `UNISOC_WCN_BSP`(=m，已设)，
   与 `CONFIG_WLAN` 无关；后者是 `drivers/net/wireless` 那棵树的总开关，默认 `y`，
   `savedefconfig` 按惯例会省略默认值，所以不出现在文件里是正常的。

### 编译验证（已完成）

`sprd_fm.o` / `sprdbt_tty.o` 直接零错误零告警通过。`sprd_wlan_combo.o`（46 个
目标文件）修掉两处后通过：

- `7f6025b3a` —— **`NL80211_WAPI_VERSION_1` 在 GKI 头文件里不存在**。WAPI
  （GB 15629.11）是 Unisoc 给 `enum nl80211_wpa_versions` 加的扩展，他们改过 UAPI
  头文件，而本树基于的 android13-5.15 GKI 没有。

  这个取值是驱动与用户态之间的 wire format（设备上原厂
  `/vendor/bin/hw/wpa_supplicant` 确实带 WAPI，43 处相关字符串），**不能猜**，
  所以从原厂 `/vendor/lib/modules/sprd_wlan_combo.ko` 里读出来：
  `sprd_convert_wpa_version()` 的 switch 被编译成 `.rodata+0x1c870` 处、以
  `(值 - 1)` 为下标的跳转表：

  | nl80211 值 | 映射到 | 反推 |
  |---|---|---|
  | 1 | `0x1` `SPRD_WPA_VERSION_1` | `WPA_VERSION_1 = 1<<0` |
  | 2 | `0x2` `SPRD_WPA_VERSION_2` | `WPA_VERSION_2 = 1<<1` |
  | 4 | `0x8` `SPRD_WPA_VERSION_3` | `WPA_VERSION_3 = 1<<2`（同上游）|
  | 8 | `0x4` `SPRD_WAPI_VERSION_1` | **`WAPI_VERSION_1 = 1<<3`** |

  即 Unisoc 保留了上游的 WPA3 取值，把 WAPI 追加在 `1<<3`。

  **注意这与驱动里 `SPRD_*` 宏的位序相反**（那边 WAPI 是 `BIT(2)`、WPA3 是
  `BIT(3)`）。按 `SPRD_*` 的位序去推 nl80211 的取值会得到错的答案，并且会同时把
  WPA3 和 WAPI 都映射错 —— 这一步中途确实推错过一次，是靠反汇编纠正的。

  定义写在驱动自己的头文件里并加 `#ifndef`，没有去改 GKI 的 UAPI 头文件，
  这样在原厂 BSP 内核上会自动让位。

- `649c88cff` —— 4 处 K&R 风格的 `()` 定义（`get_project_name` ×2、
  `get_rfboard_id` ×2）触发 `-Werror=strict-prototypes`。

把验证范围扩到本轮之外的两个旧模块时，**发现本会话自己引入的一个回归**
（`c871cdd8c` 已修）：`e1d32e45d` 同步共享头文件时，把 OPPO 树里的
`extern void gnss_hold_cpu(void);` 一并带进了 `include/misc/wcn_bus.h`，
而本树 `sprdwcn/platform/gnss_dump.c` 里这个函数是 `static` 的 →
`static declaration follows non-static declaration`，`wcn_bsp` 直接编不过。
本树内除 `gnss_dump.c` 自身外无人引用它，已删掉该声明。
**教训**：同步上游共享头文件时，新增的 `extern` 声明必须逐条对照本树里是否已有
`static` 的同名定义 —— 这类冲突不会在被导入的新驱动里暴露，只会打断旧模块。

`wcn_bsp` / `gnss_common_ctl_all` / `gnss_pmnotify_ctl` / `gnss_dbg` 随后均编译
链接通过，且产出名与设备上原厂 `.ko` 逐一对应。

### 我们导入的源码 vs 原厂 `.ko`：实测差异

**先更正一处此前写错的结论。** 我一度根据原厂 `.ko` 的 DWARF 源码路径
（`.../SPRD_A13_5G/bsp/modules/kernel5.4/wcn/wlan/wlan_combo/`，53268 处引用，
`kernel5.15` 0 处）判断「原厂跑的是 5.4 版驱动」——**这个说法是错的**。
同一份 DWARF 里：

| 字段 | 值 |
|---|---|
| 源码路径 | `bsp/modules/**kernel5.4**/wcn/wlan/wlan_combo/` |
| `DW_AT_comp_dir` | `out_abi/android13-5.15/**kernel5.15**` |
| `DW_AT_producer` | `clang version 14.0.7`（android13-5.15 参考工具链）|
| `vermagic` | `5.15.119-android13-8-gf0c1c2c751e6-dirty` |

即：**目录名是 Unisoc BSP 的布局遗留，这份代码本来就是为 5.15.119 编的。**
（驱动源码里本身带 `#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0)` 分支，
一份源码跨内核版本，所以目录名不能当版本依据。）

真正的差异要比对文件集合。从 DWARF 抽出原厂实际编进去的 134 个源文件，
与我们导入的 95 个对比：

**原厂有、我们完全没有：**
- 整个 `merlion/` 子目录（58 个文件：自带 `cfg80211.c` / `cmdevt.c` /
  `core_sc2355.c` / `main.c` / `txrx.c` / `sprdwl.h` …，是一份**并行的、
  另一代**驱动实现，符号前缀 `sprdwl_`）。原厂 `.ko` 里 `sprdwl_*` 符号 **644** 个、
  `sprd_*` 符号 227 个 —— merlion 那半其实是更大的一半。
  **这个目录在我们树里和 OPPO 那棵树里都不存在。**
- `sc2355/nan.c`（Wi-Fi Aware）—— 已确认**不是导入时漏了**，OPPO 的 kernel5.15
  分支里也没有这个文件。

**我们编、原厂那次构建没编：** `common/apf.o`、`common/chr.o`、
`common/wifi_config.o`、`sc2355/cpu_performance.o`、`sc2355/hw_sipc_param.o`、
`sc2355/sipc.o`、`sc2355/sipc_buf.o`、`sc2332/sdio.o`。
注意这一侧证据较弱：DWARF 只反映**实际编进去的**文件，分不清「原厂源码里没有」
和「原厂源码里有但被 Kbuild 条件排除了」。

**但真正给这台设备绑定的那条路径是同一份代码**，这点可以确证：
- 原厂 `.ko` 里存在 `sprd_wlan_driver`、`wlan_global_match_table` 这两个符号 ——
  正是我们 `common/sprd_wlan.c` 里的那张表；
- 两边声明的 of compatible **完全一致**：`sprd,sc2332-sipc-wifi`、
  `sprd,sc2355-pcie-wifi`、`sprd,sc2355-sdio-wifi`、`sprd,sc2355-sipc-wifi`
  （我们源码里的 `sc2332-sdio` 在 `#if 0` 内，不进表，原厂 alias 里同样没有）；
- 设备上实际绑定的是 platform driver `wlan` ←→ `sprd-marlin3:wlan`，
  节点 compatible `sprd,sc2355-sdio-wifi`，落在上面这张表里。

所以 merlion 那半对这颗芯片是不参与匹配的旁路代码。**结论：功能集合有差异
（我们多了 apf/chr/wifi_config 等，少了 NAN 和 merlion），但绑定路径一致。**

---

## 6. 下一步

1. **刷 `vendor_dlkm_e5.img`**（只刷这一个，boot / vendor_boot 自上轮未变且配套）
   → 确认电池与 USB 恢复、adb 回来；
2. 恢复 §4 的 36 项为 `=m`，重编译重打包，验证开机；
3. ~~移植触摸驱动~~ → ~~编译~~ **均已完成**（§5）：已启用并编译通过；
   真机行为（probe、坐标、`tlsc_*` sysfs 节点缺失的影响）仍待刷机验证；
4. ~~WCN 编译验证~~ **已完成**（§5b）：`sprd_fm` / `sprdbt_tty` /
   `sprd_wlan_combo` / `wcn_bsp` / `gnss_common_ctl_all` / `gnss_pmnotify_ctl` /
   `gnss_dbg` 全部编译链接通过；
5. 把「按 `modules.order` 全量替换 + depmod + 登记 fs_config/file_contexts」
   固化成一个 `repack_vendor_dlkm.sh`，避免 §3 的坑再犯；
6. ~~defconfig 里 `CONFIG_SPRD_MEMDISK` 被赋值两次~~ **已确认并清理**
   （`8b02466bc`）：设备侧实测**完全没用上** —— `/sys/firmware/devicetree/base`
   下 381 个 compatible 节点全扫，memdisk 命中 0（控制组 marlin3lite 命中 1），
   `/proc/devices`、`/proc/partitions`、`/vendor/lib/modules` 里也都没有；
   我们的 dts 树里同样没有 `sprd,memdisk` 节点，而驱动 `of_find_compatible_node()`
   拿不到节点就直接 `-ENODEV`。Unisoc 自己的 8 个 `sprd_gki_*.fragment`
   （含本 SoC 家族的 `qogirn6l`）也都是 `is not set`。
   原来那行 `=m` 来自最初的 defconfig（`412169660`），`c3c841a0c` 是在文件末尾
   追加一行来关掉它、没改原行，删掉陈旧的 `=m` 即可，生成的 `.config` 逐字节不变。

   顺带一个与本机无关但值得记的观察：原厂跑的是 **GKI** 内核，
   `/proc/config.gz` 里 `# CONFIG_ARCH_SPRD is not set` —— 平台代码全在 vendor
   模块里，所以 `SPRD_MEMDISK`（`depends on ARCH_SPRD`）在原厂配置下压根不可选。
   这也提醒：**不能用 `/proc/config.gz` 去核对我们这棵树里任何 `ARCH_SPRD`
   下的符号**，那份 config 只描述 GKI 那一半。

## 附：本轮产生的镜像

| 文件 | 说明 |
|---|---|
| `boot-e5-new.img` | 内核 `5.15.211-gf74c891fa5c3-dirty` |
| `stock-img/vendor_boot_e5.img` | first-stage 模块（130 个新编 + 原厂顺序）|
| `stock-img/vendor_dlkm_e5.img` | 56 个同名模块替换 + `ocp2131.ko` 新增 + depmod 依赖 |

回滚：`stock-img/{boot_a.img, vendor_boot_a.img, vendor_dlkm.img}`（原厂）；
`/tmp/vendor_dlkm_e5.img.prev`、`/tmp/vendor_boot_e5.img.prev`（上一版）。
