# vendor_boot 重打包：E5 开机调试研究记录

目标机型：荣悦 E5 / Unisoc UMS9621 / qogirn6lite / `ums9158_1h10_cmcc`，Android 13。
内核：自编译 `5.15.211`（原厂为 `5.15.119-android13-8`）。

本目录保存 vendor_boot 镜像的重打包工具，以及"为什么镜像会开不了机"的完整排查结论。

```
build_vendor_boot_e5.py   重打包脚本（保留原厂 header/DTB/AVB，只换模块）
modules.load.stock        原厂第一阶段模块加载清单（83 行，顺序不可改）
```

---

## 1. 为什么要动 vendor_boot

内核跑起来是没问题的（`Run /init as init process` 在 16.29s，无 panic）。真正的问题是
**原厂 vendor ramdisk 里的内核模块全部拒载**：

```
Loading module /lib/modules/printk_cpuid.ko
printk_cpuid: disagrees about version of symbol module_layout
```

原因：原厂模块是 5.15.119 编的，带 `MODVERSIONS`，CRC 与我们的 5.15.211 不匹配。

first stage init 从 vendor ramdisk 的 `/lib/modules` 加载存储相关模块，模块起不来 →
eMMC 上没有任何块设备 →

```
partition(s) not found in /sys, waiting for their uevent(s):
    boot_a, dtbo_a, init_boot_a, metadata, super, vendor_boot_a ...
Failed to mount required partitions early
InitFatalReboot   → 重启
```

所以必须把 vendor ramdisk 里的模块换成我们自己编出来的。

## 2. vendor_boot 镜像结构（boot header v4）

| 字段 | 偏移 |
|---|---|
| magic `VNDRBOOT` | 0 |
| vendor ramdisk size | `0x18` |
| vendor dtb size | `0x834` |
| vendor ramdisk table size | `0x840` |
| vendor bootconfig size | `0x84c` |

布局（每段按 `0x1000` 页对齐依次排列）：

```
0x0000     header
0x1000     vendor ramdisk (lz4 legacy 压缩)
           vendor dtb
           vendor ramdisk table
           vendor bootconfig
           [vbmeta 镜像]        ← 位于 original_image_size 处
...
分区末尾   AVB 页脚 (64 bytes)
```

LK 启动时会把各段偏移打出来，可用于确认刷进去的是不是预期版本：

```
vendorbootimage: vendor ramdisk size is 67275055
vendorbootimage: vendor dt offset is 0x402a000
```

## 3. 坑一：AVB 页脚（魔数是 `AVBf`，不是 `AVB0`）

**页脚魔数是 `AVBf`**，vbmeta 镜像的魔数才是 `AVB0`。只搜 `AVB0` 会完全漏掉页脚。

页脚位置：**分区最后 64 字节**（100 MB 分区即 `0x63FFFC0`），字段为大端：

```
+0   magic "AVBf"
+4   version_major, version_minor (u32 x2)
+12  original_image_size (u64)
+20  vbmeta_offset      (u64)
+28  vbmeta_size        (u64)
```

原厂值：`original_image_size = 0x4D18000`、`vbmeta_offset = 0x4D18000`、`vbmeta_size = 0x8C0`。

**改了 ramdisk 大小后，payload 末尾会移动，必须同时做两件事**：

1. 把 vbmeta 镜像原封不动搬到新的 payload 末尾；
2. 把页脚的 `original_image_size` / `vbmeta_offset` 重指向新位置。

只搬 vbmeta 不改页脚（第一版犯的错）的后果：

```
Loading vbmeta struct in footer from partition vendor_boot_a.
Magic is incorrect.
avb_vbmeta_image_verify() return 2
vendor_boot_a: Error verifying vbmeta image: invalid vbmeta header
load_and_verify_vbmeta() return error should not continue.
avb_slot_verify result is 6 (ERROR_INVALID_METADATA)
slot_data[0] is 0x0.                    ← 空
```

连带后果更致命：`slot_data` 变空 → 合并出的 cmdline 丢失
`androidboot.veritymode=disabled`（退化成 `enforcing`），`vbmeta.size` 从 48064 变成 19392。

校验方法：

```sh
avbtool info_image --image vendor_boot_e5.img
# Original image size 应与 VBMeta offset 相同，VBMeta size = 2240
```

> 注：vbmeta 里带的是原厂签名，payload 换了之后 hash 必然不匹配，返回
> `HASH_MISMATCH`（return 4）。但设备处于 unlocked 状态
> （`avb allow verification error in UNLOCK status`），会被放行，
> `slot_data` 仍然有效 —— 与 `boot_a` 的处理方式一致，那里也是保留原厂
> footer+vbmeta 字节不动，因此 `return 0`。

## 4. 坑二（致命）：第一阶段模块加载顺序

**原厂有 158 个 `.ko`，但 `modules.load` 只列 83 行** —— first stage init 只加载这 83 个，
而且顺序是手工排的（节选）：

```
 1  printk_cpuid.ko          11  clk-sprd.ko
 2  timer-sprd.ko            12  ums9621-clk.ko
 4  regmap-hook.ko           15  spi-sprd-adi.ko        ← PMIC 的 ADI 总线
 5  sprd_systimer.ko         16  sprd-pmic-spi.ko       ← PMIC regmap 提供者
10  sprd_wdt_fiq.ko          17  rtc-sc27xx.ko
                             18  ump9620-regulator.ko   ← 第 18 位
                             19  ump9621-regulator.ko
                             31  sdhci-sprd.ko
```

第一版脚本是**按字母序**生成的 139 行，于是 `ump9620-regulator.ko` 排到了**第 1 行**。

该驱动依赖父设备的 regmap：

```
U dev_get_regmap
U regmap_write
```

父设备（ADI/PMIC）还没 probe → `dev_get_regmap()` 返回 NULL → 随后空指针解引用。

**崩溃发生在 first stage init，远早于 pstore 初始化（15.4s），所以 pstore 完全空白**，
表现为"秒重启 + 一点日志都没有"，极难定位。

同时，字母序那版还多加载了 92 个**原厂第一阶段从不加载**的模块：
`panfrost`、`wcn_bsp`、`sprd_coresight*`×8、`zram`、`zsmalloc`、`gnss_*`、`sipa-sys`、
各种 touch IC 驱动……此时供电和时钟都还没就绪。

修复：`modules.load` 改为直接沿用原厂 83 行的顺序，只保留我们构建产物里真实存在的
条目，得到 **47 行**：

```
first-stage load list: 47 modules (stock asks for 83)
  stock-only, built into our kernel (=y), skipped: 36
  ours but never first-stage on stock, dropped: 92
```

`ump9620-regulator.ko` 因此落到第 8 位（时钟同步 / 看门狗 / 调度 / RTC 之后），
PMIC 底层已就绪。

## 5. 这些驱动在我们内核里是内置的（=y），不需要模块

```
CONFIG_MMC_SDHCI_SPRD=y      CONFIG_SPI_SPRD_ADI=y
CONFIG_PINCTRL_SPRD=y        CONFIG_GPIO_SPRD=y
CONFIG_GPIO_PMIC_EIC_SPRD=y  CONFIG_MFD_SC27XX_PMIC=y
CONFIG_SPRD_COMMON_CLK=y     CONFIG_SPRD_UMS9621_CLK=y
CONFIG_SPRD_EFUSE=y          CONFIG_SC27XX_EFUSE=y
CONFIG_MMC_HSQ=y             CONFIG_MMC_SWCQ=y
CONFIG_SPRD_PMIC_SYSCON=y
```

只有这两个是模块：`CONFIG_REGULATOR_UMP9620=m`、`CONFIG_REGULATOR_UMP9621=m`。

**结论：eMMC 主机控制器一直是存在的**，日志里
`sdhci_sprd_r11 22220000.sdio probe returned -517 (EPROBE_DEFER)`
纯粹是在等 regulator 供电，不是驱动缺失。

## 6. 日志怎么拿（三种途径的可靠性）

| 途径 | 位置 | 可靠性 |
|---|---|---|
| LK / uboot log | 保留内存 + miscdata，可回读 | 可靠，但只有 bootloader 阶段 |
| pstore | `/sys/fs/pstore/console-ramoops-0` | **不可靠**：`pstore_init` 是 `device_initcall`，**15.4s 才可用**，早期崩溃抓不到；且是循环 buffer，成功启动一次就把失败那次覆盖掉 |
| blackbox / ylog | `/blackbox/ylog/<n>/log_<n>.tar.gz`（含 `kernel.log`、`android.log`） | 只在**系统成功启动后**由服务归档，失败启动没有 |

所以"秒重启 + pstore 空"不能说明没日志，只能说明**崩在 15.4s 之前**。
下一步若还需要早期日志，可把 `pstore_init` 从 `device_initcall` 提前到 `core_initcall`。

## 7. 用法

```sh
cd artifacts_e5/vendor_boot

# 默认：stock 镜像自动查找；输出 ./vendor_boot_e5.img；
#       模块取自 <kernel tree>/out_e5
python3 build_vendor_boot_e5.py

# 也可显式指定
E5_STOCK_VENDOR_BOOT=/path/to/vendor_boot_a.img \
E5_BUILD_DIR=/path/to/out_e5 \
E5_OUT_VENDOR_BOOT=/path/to/vendor_boot_e5.img \
    python3 build_vendor_boot_e5.py
```

模块来源为 `out_e5/modules.order`（只取当前构建真正产出的模块），
`out_e5` 里历代遗留的 3000+ 个 stale `.ko` 会被自动忽略。

刷入：

```sh
fastboot flash vendor_boot_a vendor_boot_e5.img
fastboot reboot
```

## 8. 刷机后从 LK log 确认（搜索特征）

```sh
grep -nE 'vendor ramdisk size|vendor dt offset|avb_vbmeta_image_verify\(\) return|Error verifying vbmeta|avb_slot_verify result|slot_data\[0\]|start linux' uboot.log
grep -oE 'androidboot\.veritymode[^ ]*' uboot.log
```

期望看到：

```
Loading vbmeta struct in footer from partition vendor_boot_a.
avb_vbmeta_image_verify() return 0           # 或 4 (HASH_MISMATCH，unlocked 放行)
load_and_verify_vbmeta() process success.
avb_slot_verify result is 3 (ERROR_VERIFICATION)   # 与原厂一致
slot_data[0] is 0xbf......                   # 非 0
androidboot.veritymode=disabled              # 关键
androidboot.vbmeta.size=48064
```

若出现 `return 2` / `invalid vbmeta header` / `ERROR_INVALID_METADATA` /
`slot_data[0] is 0x0` → 页脚没重指向（见第 3 节）。
