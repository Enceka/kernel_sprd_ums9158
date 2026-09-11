# E5 驱动缺口清单与补齐 — 2026-09-12

新克隆了三棵完整内核树：`kernel-sprd`(5.15.149, motorola andriod-14-release-uoa34)、
`android_kernel_zte_ums9620_mifi_u30air`(5.4.254)、
`android_kernel_oppo_ums9230`(5.15.189, 基于 UNISOC android13-5.15-2025-09_r8)。
本轮用它们 + 已有的 `android_kernel_modules_and_devicetree_oppo_ums9230`(模块仓库)
系统性地盘了一次驱动缺口，并补齐了其中可补的部分。

## 1. 缺口是怎么定出来的（方法）

不靠猜，四步过滤：

1. **基准**：设备上 `/vendor/lib/modules/` 的 **130** 个 `.ko`。
2. **对照**：扫我们树里所有 `Makefile`/`Kbuild`，收集能产出的模块名
   （`obj-*` 里的 `.o`、`<name>-objs/-y/-m` 复合模块名），
   **按 `-`/`_` 归一化**后比对 → 缺 51 个。
   > 归一化这一步不能省：第一次没做，`sprd_wdf`/`sprd_gpu_cooling`/
   > `unisoc_dump_info`/`sprd-vpu-pw-domain` 都被误报成"缺失"，其实早就在树里。
3. **相关性**：`/vendor/lib/modules` 是通用 Unisoc 镜像，`modules.load` 是全量加载，
   **加载 ≠ 用得上**。真正的判据是有没有绑定成功的设备 ——
   遍历 `/sys/module/<m>/drivers/*/` 下的设备符号链接：
   - **34 个**缺失模块绑定了真实设备 → 真缺口
   - 15 个已加载但绑定 0 个设备 → 与本机硬件无关
   - 2 个未加载

   这一步直接排掉了 focaltech / novatek / synaptics / sitronix 四家触摸面板驱动
   （本机是 tlsc6x）、`snd-soc-fsa4480`、`sprd_gpu_cooling`、`sprd_wdf`/`sprd_wdh`、
   `unisoc_dump_info` 等。
4. **硬件复核**：
   - 音频**真实存在**：`/proc/asound/cards` 有 `sprdphonesc2730`，
     `/proc/asound/pcm` 里 normal/fast/voice/VoIP/FM/loop 通道齐全 → 音频栈值得补。
   - 摄像头**不成立**：DT 里虽有 dcam 节点，但 `/dev/video*`、`/dev/media*`
     **都不存在** → 整个 camera 组（`sprd_camera`/`sprd_cpp`/`sprd_sensor`/
     `sprd_flash_drv`/`sprd_camsys_pw_domain`/`flash_ic_aw3641`/`mmdvfs`）降到最低优先级。

## 2. 本轮补齐了什么

| 模块 | 数量 | 来源 | commit |
|---|---|---|---|
| 音频 DSP 底层 | 10 | oppo 模块仓 `audio/sprd_audio` | `6431cb01d` |
| vendor ASoC 栈 | 13 | oppo 模块仓 `audio/sprd` + `vender/audio/fsa4480` | `903f6b7cc` |
| VSP/VPU | 1 | oppo 模块仓 `video/sprd-vpu` | `6cfbe7870` |
| tlsc6x 模块改名 | — | 本树已有源码 | `0d8edea97` |

全部编译通过（aarch64 clang，零错误零告警），**尚未在 defconfig 启用**。

### 2.1 关键：Kbuild 的条件分支不能拍平

vendor 的 Kbuild 按 `BSP_KERNEL_BUILD_CONFIG` 分支定义 `-D` 宏。
直接 grep 出所有 `ccflags-y += -D...` 会得到**各平台的并集**，用上去就错了。
本 SoC 对应的是 `build.config.gki.aarch64.ums9621_` 分支，只取这一支：

| 模块 | 本 SoC 实际需要的宏 |
|---|---|
| `snd-soc-sprd-card` | `CODEC_UMP9620`（不是 sc2721/sc2730）|
| `snd-soc-sprd-codec-ump9620` | `CODEC_UMP9620` / `CODEC_DNS_N6L` / `HEADSET_FSA4480` |
| `snd-soc-sprd-tdm` | `UNISOC_TDM` |
| `snd-soc-sprd-vbc-fe` | `MCDT_R2P0` / `MCDT_N6L` / `DNS_N6L` |
| `snd-soc-sprd-vbc-v4`、`sprd-compr-2stage-dma` | `MCDT_R2P0` |

顺带确认：`CONFIG_UNISOC_AUDIO_MCDT`（不带 R2P0 的那个）在导入的源码里**零引用**，
并集里出现它纯属其他平台分支的噪声。

### 2.2 两处板级选择用原厂二进制定案，而不是推断

沿用之前从原厂 `.ko` 的 DWARF 反推的办法：

- **外置 PA**：`snd-soc-sprd-card.ko` 是从 `sprd-asoc-card-utils-hook.c` /
  `-legacy.c` 编的，**不是 `_14c10` 变体** → 原厂用的是
  `BSP_BOARD_AUDIO_EXTPA=false` 分支。
  值得注意：这与"设备上 `snd-soc-sprd-pa-aw87xxx` 绑定了 1 个设备"看起来矛盾，
  但机器码不会骗人，按 hook/legacy 走。
- **FSA4480**：`snd-soc-sprd-codec-ump9620.ko` 的 DWARF 里确实有
  `vender/audio/fsa4480/fsa4480-i2c.h` → `CONFIG_SND_SOC_HEADSET_FSA4480` 是开的，
  所以把 fsa4480 一并导入了。

### 2.3 修掉的上游缺陷

- `sprd-headset-ump9620.c` 自定义 `MAX()`，与本树 `include/linux/minmax.h:315`
  的 `MAX` 冲突。全驱动**只定义、零使用**，直接删（相邻的 `ABS()` 有 3 处使用，保留）。
- `impd_get_impd_val()` 定义成 `()`，而**它自己的头文件**声明的是 `(void)` ——
  `-Werror=strict-prototypes`。按头文件对齐。
- `sprd_vpu.c` 里 `#include <sprd_camsys_domain.h>` 无守卫，而该头文件在四个仓库里
  都不存在。文件内需要它的两个函数只在 `#if defined(PROJ_PIKE2)`（另一颗 SoC）
  里调用，所以删掉这个 include，而不是去凑那个头文件。
- `audio_dsp_dump.c` 用到 `DSPLOG_CMD_SET_RD_TIMEOUT`，本树
  `include/uapi/sound/sprd_audiodsp_ioctl.h` 没有。从
  `android_kernel_oppo_ums9230` 逐字节补了这一行（命令号 12），
  那是两份头文件**唯一**的差异。

### 2.4 tlsc6x 模块名

我们建的是 `tlsc6x_ts.ko`，原厂是 `tlsc6x.ko`，而 `modules.dep` 第 5 行引用的是
后者。vendor_dlkm 重打包是**按名字**替换原厂模块的，名字不对要么被丢掉、
要么留下装不进去的原厂旧模块。已改名对齐。

## 3. 有源码但本轮没做

| 模块 | 来源 | 说明 |
|---|---|---|
| `sprd_power_stat` | zte `drivers/soc/sprd/power/power_stat` | 绑定 1 个设备，功耗统计，优先级低 |
| `microarray_fp` | oppo 模块仓 `microarray` | 指纹；绑定 1 个设备，但 MiFi 上大概率只是 DT 节点 |
| `ion_ipc_trusty` | `kernel-sprd` `drivers/dma-buf/heaps` | 绑定 0 个设备；09-11 文档 §5 那条"未移植 ION secure heap"现在**有源码了** |
| `kprobe_block`/`kprobe_iowait` | oppo 内核 `drivers/unisoc_platform/io` | 未加载 |

## 4. 确认无源码（四个仓库全搜过）

- **`aw322xx_charger`** —— 仍然是那个真正卡电池的缺口。三棵新树里只有
  `aw32257_charger.c`，没有 `aw322xx`。与 09-10 文档的结论一致。
- **camera 组**：`sprd_camera` / `sprd_cpp` / `sprd_sensor` / `sprd_flash_drv` /
  `sprd_camsys_pw_domain` / `flash_ic_aw3641` / `mmdvfs`。
- `snd-soc-sprd-pa-aw87xxx`（oppo 模块仓里有个 `snd-soc-aw87xxx`，**模块名不同**，
  未核实是否同一驱动）。

`sprd_camsys_pw_domain` 缺失有一个具体后果：**`sprd-jpg` 没法导入**。
`sprd_jpg.c` 在每次 open/release 都调 `sprd_glb_mm_pw_on_cfg()`/`_off_cfg()`
（只有 SHARKL3 例外，我们不是），而设备 `/proc/kallsyms` 显示这两个符号的属主正是
`[sprd_camsys_pw_domain]`。硬凑只能把电源时序 stub 掉，那会让多媒体域不上电，
所以选择不导入并在 Kconfig 里写明原因。

## 5. 顺带发现：我们的 dts 里没有触摸节点

`arch/arm64/boot/dts/sprd/` 全树 grep `tlsc6x` **零命中**，而设备实际节点在
`/soc/ap-ahb/i2c@2270000/tlsc6x@2e`。

目前不影响：`repack_boot_e5.sh` **不替换 dtb**，运行时用的是原厂 DT，
所以驱动能正常匹配。但这是个潜在坑 —— 一旦改用 `make_e5_dtb.sh` 产出的
`e5-rongyue.dtb`，触摸会直接消失。板级 overlay 本身是有内容的（343 行，
含 `&cm` 充电节点），只是缺这一块。

## 6. 下一步

1. 决定是否在 `e5_rongyue_defconfig` 启用本轮导入的 24 个模块
   （音频 23 + VPU 1）—— 一次性打开较多，建议和一次可刷机的验证一起做；
2. `CONFIG_UNISOC_AUDIO_MCDT_R2P0` 与既有的 mainline `CONFIG_SND_SOC_SPRD_MCDT`
   是同一块硬件的两个驱动。原厂只有 `mcdt_hw_r2p0.ko`、没有 `sprd-mcdt.ko`，
   **两者不应同时开**。当前 defconfig 开的是 mainline 那个，未处理；
3. 把 `tlsc6x@2e` 节点补进 `e5-rongyue-overlay.dts`（见 §5）；
4. `ion_ipc_trusty` 现在有源码了，可以重新评估 09-11 文档 §5 里那条遗留项。
