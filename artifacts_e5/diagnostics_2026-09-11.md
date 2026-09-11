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
| 触摸 | ❌ 驱动从未被编译 | 源码位置已定位，见 §5 |

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

---

## 6. 下一步

1. **刷 `vendor_dlkm_e5.img`**（只刷这一个，boot / vendor_boot 自上轮未变且配套）
   → 确认电池与 USB 恢复、adb 回来；
2. 恢复 §4 的 36 项为 `=m`，重编译重打包，验证开机；
3. 移植触摸驱动（§5）；
4. 把「按 `modules.order` 全量替换 + depmod + 登记 fs_config/file_contexts」
   固化成一个 `repack_vendor_dlkm.sh`，避免 §3 的坑再犯。

## 附：本轮产生的镜像

| 文件 | 说明 |
|---|---|
| `boot-e5-new.img` | 内核 `5.15.211-gf74c891fa5c3-dirty` |
| `stock-img/vendor_boot_e5.img` | first-stage 模块（130 个新编 + 原厂顺序）|
| `stock-img/vendor_dlkm_e5.img` | 56 个同名模块替换 + `ocp2131.ko` 新增 + depmod 依赖 |

回滚：`stock-img/{boot_a.img, vendor_boot_a.img, vendor_dlkm.img}`（原厂）；
`/tmp/vendor_dlkm_e5.img.prev`、`/tmp/vendor_boot_e5.img.prev`（上一版）。
