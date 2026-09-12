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
| aw87xxx 功放 + hook 修正 | 1 | oppo 模块仓 `codec/sprd/aw87xxx` | `e862ef847` |
| vendor ASoC 栈 | 13 | oppo 模块仓 `audio/sprd` + `vender/audio/fsa4480` | `903f6b7cc` |
| VSP/VPU | 1 | oppo 模块仓 `video/sprd-vpu` | `6cfbe7870` |
| tlsc6x 模块改名 | — | 本树已有源码 | `0d8edea97` |

共 **25** 个模块，全部编译通过（aarch64 clang，零错误零告警）
且未定义符号均可在树内解析（见 §2.6），**尚未在 defconfig 启用**。

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
  这一度看起来与"设备上 `snd-soc-sprd-pa-aw87xxx` 绑定了 1 个设备"矛盾，
  **§2.7 解开了**：原厂没用 `_14c10` 变体，而是把 aw87xxx 的 hook 直接加进了
  普通的 `hook.c`。
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

## 2.5 追加：`sprd-kernel-modules-audio`(Motorola uoa34)的用处

这个仓库和 `kernel-sprd`(5.15.149)同源，也就是**我们树里 Unisoc 平台代码的同代版本**，
而上面导入的音频来自 OPPO 的 5.15.189。两者 68 个同名文件里 **25 个有差异**，
所以"用哪一份"不是无所谓的。

**结论：没有需要从它补的模块**（它只多一个 `snd-soc-sprd-pa-fs1815n`，是另一颗功放），
但它有决定性的**诊断价值**——把它当第三个参照后，用原厂 `.ko` 的 DWARF
（函数名 + `DW_AT_decl_line`）三方比对，发现：

| 文件 | OPPO | Motorola | 结论 |
|---|---|---|---|
| `sprd-asoc-common.c` | 偏移恒为 -4 | 同 | 两边同源 |
| `sprd-asoc-card-utils-legacy.c` | 偏移 {-4, 17} | {-4, 5, 58} | OPPO 更接近 |
| `sprd-asoc-card-utils.c` | {-1,12,53,65} | {-2,11,13} | Motorola 更接近 |
| `sprd-asoc-card-utils-hook.c` | 13 个函数只对上 **8** | 对上 **11** | 都不是原厂那一份 |

原厂 `hook.c` 是**第三个变体**：它有 Motorola 才有的 `audio_sense_*`，
**也**有 OPPO 才有的 aw87xxx 支持（原厂叫 `hook_spk_aw87xxx`）。
功能上 aw87xxx 是刚需（芯片实际在板上），所以继续用 OPPO 那一份。

## 2.6 一个 `.o` 级编译查不出来的坑：未定义符号

`macos_compile_check.sh` 只编到 `.o`，**不做模块链接**，所以"编过了"并不代表模块能加载。
本轮就踩到了：`snd-soc-sprd-card` 引用了三个外置功放驱动，其中两个根本没导入
（`sipa_audio_power_scene_set` ← `snd-soc-sipa`、`frsm_i2ca_spk_switch` ← `snd-soc-frsm-v5`），
还有一处 `soc_codec_conf_sipa` 也来自 sipa。真刷进去会直接 `insmod` 失败。

补了一道检查（做法记在这里，可复用）：把每个模块 `.o` 的未定义符号
（`llvm-nm --undefined-only`）与全树 `EXPORT_SYMBOL*` 的并集比对。
两点注意：
- 要手工补上**宏生成的导出**，否则全是误报 —— `_dev_err`/`_dev_info`/`_dev_warn`
  由 `define_dev_printk_level()` 生成，`param_ops_*` 由 `STANDARD_PARAM_DEF()` 生成，
  正则只能抓到宏的形参名；
- 跨模块引用要把本批模块自己定义的符号也算作已提供。

修完后本轮 25 个模块的未定义符号**全部可在树内解析**。

## 2.7 hook 表的顺序是和 dtb 的 ABI

`ext_hook_arr[]` 的**下标**就是设备树 `sprd,spk-ext-pa-info` 选的值，
而我们**启动用的是原厂 dtb**（`repack_boot_e5.sh` 不替换 dtb）。

实测：`/sound@0` 的 `sprd,spk-ext-pa-info = <0 1 1 0>`
→ `{ctrl_type=0(SPK), hook_sel=1, priv=1, share_gpio=0}`，即**选第 1 号 hook**；
而板上功放实测在 `/sys/bus/i2c/devices/6-0058`，名字 `aw87xxx_pa`。

OPPO 原表第 1 号是 **sia8xx**，直接用会去驱动错误的功放芯片。
去掉两个用不上的 hook 后第 1 号正好是 aw87xxx，与原厂 dtb 对齐。
**以后再动这张表必须同步确认 dtb 的取值。**

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
- ~~`snd-soc-sprd-pa-aw87xxx`~~ **已解决**：OPPO 的 `snd-soc-aw87xxx` 就是同一驱动，
  导出原厂 card 需要的 `aw87xxx_set_profile`，已按原厂名导入（见 §2.7）。

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

## 5b. 有线 adb 不工作：USB PHY 驱动重名竞态

电池在 `08b8f7587` 之后已经能读数和充电，但有线 adb 不通。诊断走的是
**对比原厂同机启动**，数据来自 `/blackbox/ylog`：session 24 是自编内核
（`5.15.211-g08b8f75879da`，即 `08b8f7587`），session 26 是原厂 —— 两份都有
完整 `kernel.log` + `android.log`，不需要串口也不需要 adb。

### 先排除掉的（两边表现一致，都是良性）

| 现象 | 判定 |
|---|---|
| `init: symlink .../ffs.adb ... failed: File exists` | **原厂同样报**。vendor rc 先组好了 gadget，Android 通用 rc 再来一次必然失败 |
| `init: write .../g1/UDC ... failed: Device or resource busy` | **原厂同样报**，同上 |
| `musb-sprd: failed to get pmu regmap!` | **原厂同样报** |
| `sprd-hsphy: No separate ID extcon device` | **原厂同样报** |
| `sprd-hsphy: failed to get refclk_cfg regmap!` | 自编独有，但代码里是 `dev_warn` + `regmap_ptr = NULL`，该 regmap 本就是可选的 |
| `sprd_eye_pattern_prepared failed, ret = -22` | 自编独有，但默认 eye pattern 已从 PHY trimming 寄存器读到；失败的只是 dtb 里不存在的板级微调属性（`hsphy-tuneeq` 等），且调用方只 `dev_warn` |

另外注意：整条 probe 链其实是能走通的 ——
`extcon-gpio`(2.117) → `typec@380`(2.121) → `hsphy`(2.126) → `musb`(2.139)
→ gadget 进 peripheral → adbd 拿到 `FUNCTIONFS_BIND` **和 `FUNCTIONFS_ENABLE`**。
所以那一次启动 USB 实际是枚举成功的，只是 BIND→ENABLE 花了 **24 秒**
（原厂 0.7 秒），而且是在一次 vbus 0→1 抖动之后才成功。

### 真正的 bug

```
[2.065] phy_sprd_qogirn6lite 加载，probe 64300000.hsphy
[2.349] Error: Driver 'sprd-hsphy' is already registered, aborting...
[2.349] init_module [phy_sprd_qogirn6pro] returned -16
[2.349] modprobe: Failed to load phy-sprd-qogirn6pro.ko: Device or resource busy
[2.349] modprobe: Loading phy-sprd-sharkl5.ko ...
```

**所有 sprd USB2 PHY 驱动都注册同一个 platform driver 名 `"sprd-hsphy"`**
（pike2 / qogirl6 / qogirn6lite / qogirn6pro / sharkl3 / sharkl5 / sharkle / ums512，
共 8 个）。厂商设计上每颗 SoC 只编一个，原厂镜像里也确实只有
`phy-sprd-qogirn6lite.ko` 一个。而我们的 defconfig 开了 **三个**。

谁先加载谁赢，顺序由重打包后 vendor_dlkm 的 `modules.load`/`modules.dep` 决定，
**不保证**。session 24 那次运气好是 n6lite 先注册；一旦换成 n6pro 或 sharkl5 先赢，
它们的 compatible 不匹配 `sprd,qogirn6lite-phy` → `64300000.hsphy` 永远不 probe
→ `64a00000.usb` 一直卡在 `-517` → **根本没有 UDC**。这与"有线 adb 时好时坏"吻合。

已修（`e6aba4003`）：只保留 `SPRD_QOGIRN6LITE_USB2_PHY`。
`SPRD_USB_DUMMY_PHY` 保留 —— 它注册的是 `sprd-dummy-phy` / 匹配
`sprd,usb-dummy-phy`，不可能冲突。

**做完之后全树扫了一遍同类问题**（"同一个 platform driver 名被本 defconfig 里
多个已启用模块占用"），结果为空。这个检查以后值得复用。

**诚实说明**：session 24 那次最终是枚举成功的，所以这修的是一个**已证实存在的竞态**，
不是一个复现出来的硬失败。要确认 adb 恢复仍需实际刷机验证。

### 顺带发现：vendor_dlkm 里还有装不进去的陈旧模块

```
modprobe: Failed to load module sprd_wdh.ko: Exec format error
modprobe: Failed to load module sprd-dmaengine-pcm.ko: Exec format error
modprobe: Failed to load module sprd-compr-2stage-dma.ko: Exec format error
```
这三个是原厂旧模块残留（我们没有产出同名替换）。其中后两个正是本轮 §2 导入的音频模块
—— 启用后重打包即可消除。

## 5c. camera / video 两个仓库：能补到哪一步

新增 `sprd-kernel-modules-common-camera`(Motorola, 2024-12-31) 与
`sprd-kernel-modules-video`(Motorola, 2024-09-24)。

### camera 仓：模块齐了，但卡在 4 个内核头文件

它确实**覆盖了全部 7 个缺失的 camera 模块**（`KO_MODULE_NAME` 逐一对上）：
`sprd_camera`(cam_sys/core)、`sprd_cpp`、`sprd_sensor`、`sprd_flash_drv`、
`flash_ic_aw3641`、`mmdvfs`、`sprd_camsys_pw_domain`。
而且 `power/kernel/` 下有 `sprd_camsys_pw_domain_qogirn6l.c` —— **qogirn6l 就是
qogirn6lite**，正是本 SoC；单个驱动 `sprd_campd`，无重名问题。

**但整棵 camera 栈依赖 4 个 `include/video/` 下的内核头文件，八个仓库里全都没有：**

| 头文件 | 谁需要 |
|---|---|
| `video/sprd_mmsys_pw_domain.h` | power、mmdvfs、sensor、cam_sys、core |
| `video/sprd_mmsys_pw_domain_qogirn6l.h` | power |
| `video/sprd_mmsys_pw_domain_qogirn6pro.h` | power |
| `video/sprd_vsp_pw_domain.h` | cpp |

这是**内核侧**头文件（Unisoc BSP 加在 `include/video/`），不在任何模块仓库里。
其余依赖都满足：`flash/` 零缺口，`sprd_camsys_domain.h` 就在 camera 仓自己的
`power/kernel/` 下，`os_adapt_common.h` 在 `os_adapt/linux/`。
（`core/` 另外还要 `mach/hardware.h`、`soc/sprd/globalregs.h` 等老 SoC 的头，
但 `core/` 是旧一代 dcam，本 SoC 用的是 `cam_sys/`，不需要。）

**所以 `sprd-jpg` 仍然进不来** —— 它需要 `sprd_glb_mm_pw_on_cfg()`，
而提供方 `sprd_camsys_pw_domain` 自己编不过去。

### video 仓：没有那些头文件，但澄清了三件事

1. **它也没有**那 4 个头文件。
2. 它的 `sprd-vpu-power/sprd_vpu_pw_domain.c` 与我们树里
   `drivers/soc/sprd/domain/` 那份 **md5 完全相同**（`dcc14f0f…`）—— 同源，无需动。
   两个 power 模块都不导出任何符号（纯 genpd provider），所以
   `sprd_vsp_pw_domain.h` 只是给 camera `cpp/` 声明那两个函数用的。
3. `sprd-vsp` / `sprd-vsp-power` **本机用不上** —— 原厂 130 个模块里没有 vsp。
4. 它的 `sprd-vpu` / `sprd-jpg` 与 OPPO 仓**文件集完全一致**（都没有 `vpu_drv.c`），
   所以换仓库不解决问题。

### 顺带核实：导入的 vpu 是新一代源码，但能正确绑定

原厂 `vpu.ko` 的 DWARF 显示它是用 **`vpu_drv.c`** 编的，而两个公开仓库都只有
拆分后的 `common_drv.c` + `vpu_r1p0.c` + `vsp_*.c` —— 又是一个"原厂是第三变体"
的情况（同 §2.5 的 `hook.c`）。

但这次可以确认不影响绑定：设备上 `sprd_vpu` 驱动绑的是
`32000000.video-codec`(`sprd,vpu-enc-core0`) 和
`32200000.video-codec`(`sprd,vpu-dec-core0`)，**两个 compatible 都在我们导入版本的
`of_match_table_vpu` 里**，且驱动名同为 `sprd_vpu`。

## 5d. 有线 adb 的真正致命点：MUSB `hops.host_start` 是 NULL

§5b 修的是"PHY 驱动重名竞态"（会让 UDC 时有时无），但它只解释"时好时坏"。
继续追之后，用 `artifacts_e5/incident/dmesg.log` 里自编内核的崩溃记录定位到一个
**必然致命**的 bug：只要切到 host 模式，内核立刻 NULL 解引用而死。

```
[31.038] Unable to handle kernel NULL pointer dereference at 0x0
[31.038] Internal error: Oops [#1] PREEMPT SMP  5.15.211-g47380a7b3b56-dirty #7
[32.233] Workqueue: k_sm_usb musb_sprd_otg_sm_work [musb_sprd]
[32.247] pc : 0x0
[32.250] lr : musb_sprd_otg_start_host+0x290/0x368 [musb_sprd]
```

时序：插入 USB → typec `source connect!` → `musb_sprd_otg_start_host: turn on host`
→ `host setup done` → 立即 Oops。`pc=0x0` 说明是**函数指针为 NULL 被调用**。

反汇编 `musb_sprd.ko` 确认：

```
134c: ldr x8, [x19, #0x2370]   ; musb->hops.host_start
1350: blr x8                    ; NULL -> 跳到地址 0
```

`0x2370` 正对应 `struct musb_host_ops`（`musb_core.h:196`）第一个成员
`host_start` 的偏移。根因：该结构体里 4 个钩子（`host_start` /
`advance_schedule` / `tx_dma_program` / `rx_dma_program`）在本树里**只被读取、
从未赋值** —— `musb_host_alloc()` 少了 wiring 那段，且本树没有
`musb_host_start()` 的实现。对照兄弟树
`refer/android_kernel_zte_ums9620_mifi_u30air/`：它会在
`musb_host.c:3272-3275` 把这 4 个全填上，并在 `musb_sprd.c` 里用
`if (musb->hops.host_start)` 保护该调用。我们两样都没有。

**已修**（`artifacts_e5/fix_musb_hops.py`，幂等，可重跑）：

1. `musb_host.c` `musb_host_alloc()`：填上本树确实存在的两个 ——
   `advance_schedule`、`tx_dma_program`；
2. `musb_sprd.c:1591`：裸调用 `musb->hops.host_start(musb);` 改为 `if (...)` 保护。
   本树无该实现，跳过是安全的：HCD 已由 `musb_host_setup()` 拉起，紧接着
   `sprd_musb_enable()` 会做 SESSION/HOST_FORCE_EN；
3. `sprd_musbhsdma.c`：两处 `hops.rx_dma_program(...)` 调用点加保护（该钩子本树
   无实现，保持 NULL）。`advance_schedule` / `tx_dma_program` 现已填上，无需保护。

影响：这条是**硬失败**（内核直接挂），比 §5b 的竞态更靠前，很可能才是"有线 adb
一直不通"的主因 —— 即便 PHY 正确 probe、UDC 出现，一旦切 host 模式照样 Oops。

**编译/链接验证已过**（fedora VM 内 `make O=out_e5 modules`，`out_e5` 里三处目标文件
19:43 重新生成）：

- `musb_sprd.ko` —— 崩溃点由原来的 `ldr x8,[x19,#0x2370]; blr x8` 变为
  `ldr x8,[x19,#0x2370]; cbz x8,0x1358; ...; blr x8`，即有了空指针判断；
- `musb_hdrc.ko` `musb_host_alloc` —— 新增两条 store：`str x8,[x19,#0x2378]`
  (`advance_schedule`)、`str x8,[x19,#0x2380]` (`tx_dma_program`)；
  `host_start`(0x2370)/`rx_dma_program`(0x2388) 仍为 0，由调用点保护。

剩下只是刷机实测 —— 09-12 晚间已用本树重新编译并打好三个镜像（内核串
`5.15.211-ge05c37adde49`，尺寸与 sha256 见 `README.md` 的 "Rebuild & repack,
2026-09-12 20:38" 一节），插线看是否还 Oops。

## 6. 下一步

1. 决定是否在 `e5_rongyue_defconfig` 启用本轮导入的 25 个模块
   （音频 24 + VPU 1）—— 一次性打开较多，建议和一次可刷机的验证一起做。
   注意 `SND_SOC_UNISOC_PA_AW87XXX` 必须与 `SND_SOC_UNISOC_CARD` 同开，
   否则 card 里的 `aw87xxx_set_profile()` 无法解析；
2. `CONFIG_UNISOC_AUDIO_MCDT_R2P0` 与既有的 mainline `CONFIG_SND_SOC_SPRD_MCDT`
   是同一块硬件的两个驱动。原厂只有 `mcdt_hw_r2p0.ko`、没有 `sprd-mcdt.ko`，
   **两者不应同时开**。当前 defconfig 开的是 mainline 那个，未处理；
3. 把 `tlsc6x@2e` 节点补进 `e5-rongyue-overlay.dts`（见 §5）；
4. `ion_ipc_trusty` 现在有源码了，可以重新评估 09-11 文档 §5 里那条遗留项；
5. 刷机验证有线 adb：**先看 §5d 的 MUSB `hops` 修复**（这是硬失败，优先级最高），
   再看 §5b 的 PHY 修复是否让 UDC 稳定出现；若都不再 Oops 但仍不稳，下一个怀疑对象
   是 BIND→ENABLE 那 24 秒延迟（原厂 0.7 秒），可从 `musb-sprd` 的
   `sm_work` 状态机与 vbus 事件时序入手。
