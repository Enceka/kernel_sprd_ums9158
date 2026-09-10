# E5 (kernel_ts305_ums9158) 诊断记录 — 2026-09-10

本轮把系统从「黑屏 + 无日志 + 启动回滚」推进到「显示链路打通 + 日志恢复」，
剩下一个 HAL 侧的崩溃点挡住了开机。

## 结论速览

| 目标 | 状态 | 依据 |
|---|---|---|
| DRM/DSI/DPU/panel 显示链路 | ✅ 打通 | 面板 `320x480@59` 模式已设，见 §1 |
| `dpu` / `dsi` / `dphy` / `gsp` 探测 | ✅ 全部 `returned 0` | 见 §2 |
| 日志系统 (`unisoc_userlog` → ylog) | ✅ 恢复 | 见 §3 |
| HWC HAL (`composer@2.4-service`) | ❌ 段错误 → 启动循环 | 见 §4 |

---

## 1. 根因一：vendor_dlkm 的厂商模块全都加载不了（已修复）

原厂模块与重建内核的 vermagic 不匹配：

| | 版本 |
|---|---|
| 原厂 vendor_dlkm 模块 | `5.15.119-android13-8-gf0c1c2c751e6-dirty` |
| 重建内核 | `5.15.211-g7ed1875c73a3-dirty` |

→ 每一个都 `disagrees about version of symbol module_layout` / `Exec format error`。
`/vendor/lib/modules/` 里载有 `unisoc_userlog.ko`（ylog 日志后端）、`sprd-vpu-pw-domain.ko`
等关键模块，全灭之后既没有日志、也没有显示。

**修复**：重打包 `vendor_dlkm.img`（见 §5），把能自建的模块换成我们的 5.15.211 版本。

---

## 2. 根因二：VPU 电源域缺 genpd provider（已修复，显示由此打通）

### 现象

启动时有 15 个平台设备 `probe of X returned -517`，其中只有 4 个**永远**不恢复：

```
30130000.sprd-gsp   31000000.dpu   31300000.dsi   31300000.dphy
```

### 定位过程

1. `really_probe()` 在调用 `probe` 之前先跑 `device_links_check_suppliers()`，
   失败即返回 `-EPROBE_DEFER`。日志里这 4 个设备都是 **3~7 微秒**返回，说明
   probe 函数根本没进去 —— 问题在供应商没就绪。
2. 在 `drivers/base/core.c` 里加 `DEFERDBG` 插桩（把原本走 `dev_dbg`
   被吞掉的供应商名提升到 `KERN_ERR`），一次启动即点名：
   `soc:mm:power-domain@0`（`compatible = "sprd,vpu-pd"`，phandle `&vpu_pd_top`）无驱动。
3. 原厂对照：`gsp/dpu/dsi` 的供应商里都有 `platform:soc:mm:power-domain@0`；
   `sprd-gsp` 更是**唯一**供应商。
4. 原厂 `/sys/bus/platform/drivers/sprd-vpu-pd/module` → `sprd_vpu_pw_domain`
   —— 即 `sprd-vpu-pw-domain.ko`，Unisoc 闭源模块，任何一棵移植树里都没有。

### 原厂时间线（stock dmesg）

```
[3.772] modprobe: Loading module /vendor/lib/modules/sprd-vpu-pw-domain.ko
[3.775] sprd-vpu-pd soc:mm:power-domain@0: vpu_pd_probe, 232
[3.776] sprd-vpu-pd: PMU_APB_PIXELPLL pw on
[3.776] sprd-vpu-pd: power-domain: vpu_pw_on OK
[3.831] probe of 31300000.dsi.0 returned 0
[3.836] probe of 31000000.dpu   returned 0
```

原厂在 0.98s **同样**出现这些 `-517`（全日志 231 处），只是 3.77s 模块加载后
`driver_bound()` 触发延迟探测重试，全部转 0。我们的内核缺这个模块，于是永久挂起。

### 修复

- 源码取自 `sprd-kernel-modules-video/sprd-vpu-power/sprd_vpu_pw_domain.c`（Unisoc, GPL-2.0），
  引入 `drivers/soc/sprd/domain/`，`CONFIG_SPRD_VPU_PW_DOMAIN=m`。
- 加进 vendor ramdisk 的第一阶段加载清单（排在 `sprd-gsp.ko` / `sprd-drm.ko` 之前）。
- 修复后 4 个设备全部 `returned 0`。

### 其余 11 个 `-517` 是正常抖动

DEFERDBG 点名的其余供应商（`64160000.efuse`、`64900000.aonapb-gate`、
`64400000.spi:pmic@0:power-controller@2000`、`spi4.0` …）与原厂一致，
后续都会退避重试成功 —— 实测 `sdio ×2 / gpu / pwm / thermal ×2 / pmic adc·gpio / sprd-uid`
在原厂和我们的内核里最终都是 `returned 0`。

---

## 3. 日志系统恢复

`unisoc_userlog.ko` 位于 vendor_dlkm 第 8 条，替换成我们的构建后：

```
unisoc_userlog: unisoc_userlog_init
unisoc_userlog: created 256K log 'userlog_point'
initcall init_module+0x0/0x28 [unisoc_userlog] returned 0
```

ylog/blackbox 因此恢复写入 —— 这也是本轮能拿到 `171` 日志的前提。

---

## 4. 现存问题：GSP capability ioctl 的 `copy_to_user` 全数失败

### 崩溃链

```
E GSPModule: gsp device capability has not been initialized
F libc: Fatal signal 11 (SIGSEGV), fault addr 0x0, pid 448 (composer@2.4-se)
F libc: Fatal signal 6 (SIGABRT), pid 479 (surfaceflinger)
        ↓ 反复重启
StartWatchdog 16:50:17 → 16:50:31 → 16:50:36 ...   (load average 11.83)
        ↓
bugreport-wdt-2026-09-10-16-51-37.txt
```

### 内核侧证据

`drivers/.../gsp/gsp_dev.c: sprd_gsp_get_capability_ioctl()`：

```c
	size = drm_capa->size;
	if (size < sizeof(*capa))                  { ... "size: %zu less than request" ; return -1; }
	ret = gsp_dev_get_capability(gsp, &capa);
	if (size > capa->capa_size)                { ... "get capability size exceed error"; return -1; }
	ret = copy_to_user(drm_capa->cap, capa, size);
	if (ret)  GSP_DEV_ERR(dev, "get capability copy error\n");     ← 我们这里必然命中
	GSP_DEV_INFO(dev, "io_cnt:%d, core_cnt:%d ,size:%zu, cap->size:%d", ...);
```

`struct drm_gsp_capability { __u8 gsp_id; __u32 size; void *cap; }`
（`include/uapi/drm/sprd_drm_gsp.h`，升级提交未改动该 UAPI）。

### 精确对照（`core_cnt:1 ,size:N` 行）

| | 原厂 155 (5.15.119) | 我们 171 (5.15.211) |
|---|---|---|
| `size=208` 的请求 | 2 | **0** |
| `size=92` 的请求 | 2 | **26** |
| `get capability copy error` | **0** | **26** |
| `composer@2.4-se` SIGSEGV 行数 | 2 | 164 |
| `SurfaceFlinger is starting` | 1 | 13 |

驱动侧本身是好的：`capa->capa_size = 208` 两家一致，`io_cnt:7, core_cnt:1` 一致，
`sizeof(struct gsp_capability)` 都 ≤ 92（否则会打 `size: 92 less than request`），
所以两道尺寸校验都通过，**失败的只有 `copy_to_user` 本身**。

HAL 的调用序是「先 92 后 208」（原厂日志里 `92,208,92,208` 交替）；
我们这边第一次 92 就失败，HAL 直接放弃，208 那条路再没走到 → capability 拿不到
→ HWC 空指针 → SurfaceFlinger 中止。

### 尚未确定

为什么同一个 `copy_to_user(drm_capa->cap, capa, 92)` 在 5.15.119 成功、在 5.15.211 失败，
**原因未定位**。已排除：

- 用户态结构体布局（`drm_gsp_capability` 升级提交未改）；
- 符号缺失（`llvm-nm -u sprd-drm.ko` 里有 `U sprd_gsp_get_capability_ioctl`；
  原厂那句 `sprd_drm: no symbol version for ...` 只是原厂模块没有 MODVERSIONS CRC，
  我们这边有 CRC 所以本来就不会打印，**不是缺符号**）；
- capability 结构体尺寸不匹配（两道校验都过）。

下一步候选：在驱动里加一行 `pr_info` 打印 `drm_capa->cap` 与 `copy_to_user` 的返回值，
或用 `access_ok()` 判定是「地址非法」还是「拷贝中途 fault」。

> **已加为诊断 patch**（未测试，等下次能刷机再验证）：`gsp_dev.c` 里
> `sprd_gsp_get_capability_ioctl()` 现在会在 `copy_to_user` 前后各打一行日志，带上
> `drm_capa->cap` 的实际值、`access_ok()` 的结果、以及 `copy_to_user` 返回的「未拷贝字节
> 数」。看下一次的日志时重点关注：
> - `access_ok` 就不过 → 传进来的 `cap` 指针本身不是合法用户地址（HAL 侧问题，或者
>   `struct drm_gsp_capability` 在用户态/内核态的实际内存布局不一致——留意 stock 内核用
>   `clang 14.0.7` 编译、我们现在用 `clang 22.1.8`，两者对同一个 C 结构体的对齐/内边距按
>   AAPCS64 应该一致，但值得作为一个可疑点排除）；
> - `access_ok` 过但 `copy_to_user` 仍失败、且 `uncopied` 约等于 `size`（几乎整段没拷进
>   去）→ 更像是地址一开始就没法访问（例如 stale/已经被回收的映射）；
> - `uncopied` 明显小于 `size`（拷了一部分才失败）→ 更像是拷贝中途跨页时撞到了一个没映
>   射的页，指向该用户 buffer 本身跨越了一个洞。

### 顺带观察到、同样待查的一项

```
watchdogd: Failed to open /dev/watchdog: No such file or directory
init: Service 'watchdogd' (pid 271) exited with status 1
```

`641e0000.watchdog` probe 是成功的，但 `/dev/watchdog` 没建出来。需要和原厂
（当前设备上）`ls -Z /dev/watchdog*` / `/sys/class/watchdog/` 对比确认是配置差异
还是驱动差异。

---

## 5. 复现步骤（vendor_dlkm 重打包）

vendor_dlkm 是 `super` 里的动态分区（设备上 `dm-6`），设备已挂载，**不需要 erofs 解包工具**：

```sh
# 1. 从设备取原厂内容（root）
adb shell su -c 'rm -rf /data/local/tmp/vdlkm && mkdir -p /data/local/tmp/vdlkm \
  && cp -a /vendor_dlkm/. /data/local/tmp/vdlkm/ && chmod -R a+rX /data/local/tmp/vdlkm'
adb pull /data/local/tmp/vdlkm /tmp/e5ref/vdlkm

# 2. 用 out_e5/modules.order 里的同名模块替换（只认当前构建，避开 out_e5 里的陈旧 .ko）
#    并 llvm-strip --strip-debug 压缩体积（unisoc_userlog 0.28MB → 0.02MB）

# 3. depmod 重生成元数据，再改写成原厂的绝对路径形式
#    /vendor/lib/modules/foo.ko: /vendor/lib/modules/bar.ko

# 4. mkfs.erofs 打包（原厂参数：LZ4 / blocksize 4096 / LZ4_0PADDING）
LD_LIBRARY_PATH=<aosp-plugged>/lib <aosp-plugged>/bin/mkfs.erofs \
  -zlz4 -T <ts> --mount-point=/vendor_dlkm --file-contexts=/tmp/e5ref/vdlkm_fc.txt \
  vendor_dlkm_e5.img /tmp/e5ref/vdlkm_new
```

file_contexts（两行，与原厂标签一致）：

```
/vendor_dlkm(/.*)?     u:object_r:vendor_file:s0
/vendor_dlkm/etc(/.*)? u:object_r:vendor_configs_file:s0
```

结果：`79200256` 字节（原厂 `111296512`），格式与原厂逐字段一致。

### 刷写 / 回滚

`boot_a` + `vendor_boot_a` + `vendor_dlkm_a` **三个必须一起刷** —— 只刷 vendor_dlkm
会让 5.15.211 的模块配原厂 5.15.119 内核，全部加载失败，比不改还差。

```sh
adb reboot fastboot
fastboot flash boot_a         kernel_ts305_ums9158/boot-e5.img
fastboot flash vendor_boot_a  stock-img/vendor_boot_e5.img
fastboot flash vendor_dlkm_a  stock-img/vendor_dlkm_e5.img
fastboot reboot
```

回滚：`stock-img/{boot_a.img, vendor_boot_a.img, vendor_dlkm.img}`。

---

## 6. 下一步

1. **GSP capability**（§4）：定位 5.15.211 上 `copy_to_user` 失败的原因 —— 这是当前
   唯一挡住开机的问题。
2. **`/dev/watchdog`**：与设备上的原厂系统对比确认。
3. **补齐仓库模块（35 个）**：`sprd-kernel-modules-{video, common-camera, audio, microarray}`
   可编出 `vpu` `jpg` `sprd_camera` `sprd_cpp` `sprd_sensor` `sprd_flash_drv`
   `flash_ic_aw3641` `sprd_camsys_pw_domain` `mmdvfs` `microarray_fp` 及 audio 全套。
4. **接线树内已有源码**：`sprd_gpu_cooling` `ion_ipc_trusty` `sprd_wdf` `sprd_wdh`
   `unisoc_dump_info` `sc27xx_fuel_gauge` `sprd-charger-manager`（源码在树里，只是没接 Kconfig）。
5. **确认无源码**：`sprd_wlan_combo` `sprdbt_tty` `sprd_fm`（WCN/WiFi）、`aw322xx-charger`、
   `snd-soc-sprd-pa-aw87xxx`、`snd-soc-fsa4480`、`sprd_power_stat`。
6. **触摸不是问题**：`focaltech_*` / `novatek_nt36528` / `sitronix_touch` / `nvt_nt36xxx` /
   `synaptics_td4320` / `tlsc6x` 都不在 vendor `modules.load` 里，原厂也不加载。

---

## 7. 本轮提交

- `import: sprd UMS9621 VPU power domain driver (sprd-vpu-pw-domain)`
- `fix: load sprd_vpu_pw_domain.ko from the vendor ramdisk`
- `debug: print the blocking supplier on fw_devlink probe deferral`（诊断用，可回退）
