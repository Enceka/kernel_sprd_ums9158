# 2026-09-09 排查记录:今天的刷机测试到底发生了什么

背景:反馈是"刷入内核无法进入,也没办法获得 pstore"。设备当时仍可 adb 连接(自动回退
到了 stock 内核那一侧)。本文记录纯设备取证(未刷机、未重启设备)得到的结论,以及一个
新发现的、**不需要串口**就能拿到早期内核日志的通道。

## 0. 结论先行

- 今天(09-09)一共 **7 次真实开机尝试**,LK **每一次都成功跳进了内核**——AVB/LK 从来
  不是卡点。
- 今天成功归档到 `blackbox/ylog` 的 **4 次完整启动全部是原厂 5.15.119 内核**。**没有任何
  证据表明自编内核(5.15.211)今天完整跑完过一次开机流程。** `git log` 里"已经定位到
  vendor_boot 模块版本不匹配"那条结论,目前设备上的证据既没有证实也没有证伪。
- 找到一个新的、完全不需要串口的日志通道(`common_rs1_<slot>`,见第 3 节),可以在下次
  测试时直接拿到失败那次开机的完整 kmsg——这是目前唯一能继续往下查的路。
- `build_vendor_boot_e5.py` 里加了一道边界检查(commit `200fc7935`),但根据分区实测数据
  (第 4 节),它大概率**不是**今天这两次 AVB footer 复现的真正原因,只是顺手堵上的一个
  真实但边际的坑。

## 1. 时间线怎么拼出来的

三个数据源交叉核对:

| 来源 | 能看到什么 | 看不到什么 |
|---|---|---|
| `/dev/block/by-name/uboot_log`(16MB,非线性轮转写入,新旧记录混杂) | LK 阶段:AVB 校验结果、是否跳进内核(`start_linux`) | 跳进内核之后发生了什么;`repack_boot_e5.sh` 为了绕过更严格校验,故意保留原厂 AVB footer/vbmeta 字节,所以这里的 vbmeta digest **永远是原厂的**,没法区分这次跳的是原厂内核还是自编内核 |
| `/sys/fs/pstore/console-ramoops-0` | 崩溃/重启前最后一小段 console 输出 | 单槽位环形缓冲区,**下一次成功启动会直接覆盖**;而且 `pstore_register()` 实际挂在 `arch_initcall_sync`(`of_platform_default_populate_init`),不是想象中的 `postcore_initcall`,能捕捉到的窗口比预期更窄 |
| `/blackbox/ylog/<n>/log_<n>.tar.gz` | 完整 `kernel.log`(含 `Linux version` 版本行) | **只在完整启动成功后才归档**,失败的那几次启动全都没有对应的 ylog |

方法:把 `uboot_log` 里今天(`LK time is 2026-09-09_*`)的记录按真实时间排序(不能按文件内
偏移排序,这个分区是轮转写入的,今天的记录和 2024-11、2026-03/04 的旧记录物理上混在一
起),再和同一时间段的 `blackbox/ylog` 归档、`console-ramoops-0` 对齐。

## 2. 今天的完整时间线

真实开机尝试(LK 都跳进了内核):

| 时间 | AVB 结果 | 后续是否有 ylog 归档 |
|---|---|---|
| 07:09:44 → 07:09:52 跳转 | 正常(result 3) | ✅ `log_128.tar.gz`(07:10),**stock 5.15.119** |
| 15:56:04 → 15:56:12 跳转 | **result 6 / ERROR_INVALID_METADATA / slot_data[0]=0x0** | ❌ 无归档,启动失败 |
| 15:57:11 → 15:57:17 跳转 | 正常(result 3) | ❌ 无归档,启动失败 |
| 15:58:08 → 15:58:16 跳转 | 正常(result 3) | ✅ `log_129.tar.gz`(15:59),**stock 5.15.119** |
| 16:25:10 → 16:25:18 跳转 | **result 6 / ERROR_INVALID_METADATA / slot_data[0]=0x0** | ❌ 无归档,启动失败 |
| 16:25:38 → 16:25:46 跳转 | 正常(result 3) | ✅ `log_130.tar.gz`(16:26),**stock 5.15.119** |
| 16:42:07 → 16:42:15 跳转 | 正常(result 3) | ❌ 无归档,启动失败(?) |
| 16:42:45 → 16:42:53 跳转 | 正常(result 3) | ✅ `log_131.tar.gz`(16:44),**stock 5.15.119**——这就是当前 adb 连着的这次启动 |

四次归档的 `kernel.log` 第一行版本号全部核对过:

```
Linux version 5.15.119-android13-8-00018-ga78ea39db117-ab10710420 ...
```

没有一次是 `5.15.211`。因为 LK 日志分不清跳进去的是原厂还是自编内核(vbmeta digest 始终
是原厂的,见上表),**无法从 LK 侧确认哪几次尝试用的是自编内核**,但可以确认:今天所有
"成功走完全程"的启动,清一色是 stock。

`console-ramoops-0` 里唯一的一行:

```
[    1.059840][    T1] reboot: Restarting system with command 'bootloader'
```

uptime 仅 1.06 秒,远早于此前调研记录的"约 16 秒后 `InitFatalReboot`"(vendor_boot 模块版
本不匹配那个理论对应的失败时间点)。这行本身大概率就是上表里某次 `result 6`(AVB footer
问题)尝试的残留——`androidboot.vbmeta.digest` 等 cmdline 参数因为 `slot_data[0]=0x0` 整
体丢失,第一阶段 init 大概率因此在极早期主动调用了 `reboot(bootloader)`。**但这只是推测,
没有更早的日志能确认。**

## 3. 新发现:不需要串口的日志通道

内核里 `drivers/unisoc_platform/sysdump/last_kmsg.c` 注册了一个 **reboot notifier**——不
管是不是 panic,只要内核走到 `reboot(2)` 系统调用(`kernel_restart()` →
`kernel_restart_prepare()` → 通知链,发生在打印 "Restarting system..." **之前**),就会把
kmsg 环形缓冲区 + 上一次的 android logcat 整个写到一个固定分区:

```
/dev/block/by-name/common_rs1_a   (对应 ro.boot.slot_suffix=_a;_b 槽位对应 common_rs1_b)
  offset 0x0        : "bootloader\n" (16 字节的 reboot 目标字符串头) + kmsg
  offset 0x100000    : 上一次的 android logcat
分区大小 8MB (0x800000)
```

已经实测验证过(读出了一次真实的 kmsg,内容是一次正常关机重启,`init: Reboot ending,
jumping to kernel`)。**唯一要注意的是它是单槽位,会被下一次 `reboot()` 覆盖**——今天两
次 AVB footer 失败的快照,都已经被后来的正常重启冲掉了,这次没捞到。

### 下次测试时的操作步骤

1. 该怎么恢复能进系统就怎么恢复(自动回退也好,手动 `fastboot flash boot_a stock.img`
   也好)——**这一步不会碰 `common_rs1_a`**,它是完全独立的分区,只有"内核真正执行过一次
   `reboot()`"才会覆盖它。
2. **adb 一连上,立刻执行,不要做任何别的事(尤其不要再重启)**:
   ```sh
   adb pull /dev/block/by-name/common_rs1_a
   dd if=common_rs1_a bs=1 skip=16 count=1048560 of=kmsg.txt   # kmsg
   dd if=common_rs1_a bs=1M skip=1 of=lastlog.logcat           # 上次 logcat
   ```
3. 更稳妥:如果 fastboot/LK 支持双镜像 `fastboot boot`,可以用
   `fastboot boot stock_boot.img [stock_vendor_boot.img]` 做纯内存临时启动,不写 flash——
   这样 boot_a/vendor_boot_a 里失败的那次配置原封不动保留着,读完日志还能直接复现,不用
   重新刷一遍自编内核。

`last_kmsg` 只在内核真正调用了 `reboot(2)` 时触发(`register_reboot_notifier`)。如果某次
失败是**硬件看门狗直接复位**(没有走 reboot 系统调用),这个通道会跟 pstore 一样抓不到。
这种情况下的备用方案:给 cmdline 加
`ramoops.mem_address=0xfff80000 ramoops.mem_size=0x40000 ramoops.record_size=0x8000
ramoops.console_size=0x8000 ramoops.pmsg_size=0x8000`,可以让 ramoops 走 dummy-device 路
径在 `postcore_initcall`(比当前实际生效的 `arch_initcall_sync` 更早几个初始化阶段)就完成
注册,多抓一点最早期的窗口——**这个改动没有实测过,刷之前建议先确认不会因为 cmdline 解
析顺序等问题引入新问题**。

## 4. vendor_boot AVB footer 的边界检查(已提交,但可能不是今天这个 bug 的真正原因)

`build_vendor_boot_e5.py` 把 vbmeta blob 搬到新 payload 末尾(`new_end`)、footer 重新指向
它时,原来完全没有校验 `new_end + vbmeta_size` 是否还在 footer(分区最后 64 字节)之前。
如果自编模块集合体积把 payload 推过这条线,会**静默**把 footer/分区尾部写坏——已经加了断
言,越界直接报错而不是刷出一个损坏的镜像(commit `200fc7935`)。

但从设备上实测的 stock `vendor_boot_a`(100MB 分区)反推:

```
当前 payload 结束 (vbmeta_offset+vbmeta_size) : 0x4d188c0
footer 位置                                    : 0x63fffc0
headroom                                       : 0x16e7700 字节 ≈ 22.9 MB
```

23MB 的余量,自编模块集合(几十到一百多个 .ko)不太可能把 payload 撑爆这么多。**所以今天
两次 `ERROR_INVALID_METADATA` 复现,更可能是别的原因**(比如某次 fastboot 传输/写入过程
中的偶发问题,或者别的逻辑 bug),这道边界检查值得保留,但不要把它当成"已解决"——下次
复现时用第 3 节的方法直接抓 `common_rs1_a`,应该能看到 LK 报告的具体 footer 数值,跟这次
写入的镜像做字节级比对。

## 5. 新发现:boot_a 内核 Image 的可用空间几乎是满的

`repack_boot_e5.sh` 把内核 Image 原地换入 boot.img 的固定区域(从 0x1000 到原厂 ramdisk 的
起始偏移),超出就会在打包阶段直接 `assert` 失败。用同样的算法(向后扫描 lz4 魔数定位
ramdisk)量了一下**当前设备上的 boot_a**:

```
内核可用区域 : 0x2c97000 字节 (44.6 MB)
当前(stock)内核占用 : 0x2c96a00 字节 (44.6 MB)
余量 : 仅 0x600 字节 (1.5 KB, 0.0%)
```

注:这是量的**设备当前 boot_a**,不是 `repack_boot_e5.sh` 实际使用的那个 Magisk-patched 基
准镜像,两者余量未必完全一致,但量级应该接近。

这个余量非常紧张,而最近几个 commit 的趋势是往内核里塞更多东西:UMP9620/9621 regulator
一度改 built-in、`DRM_SPRD_DPU0`/`DRM_SPRD_DSI`、`PWM_SPRD`、`sprd_systimer` 强制 `=y`、
"build boot-critical Unisoc platform drivers into the kernel",以及 `DEBUG_INFO_BTF=y`(会
给 vmlinux 嵌入 BTF 调试信息,通常会让最终 Image 明显变大)。**如果下一次构建的 Image 比
现在这个 stock 基准大哪怕几百字节,`repack_boot_e5.sh` 就会在打包这一步直接失败**(这是好
事,是显式报错而不是刷出坏镜像;但如果构建/打包流程里有人加了 `|| true` 之类的兜底跳过这
个 assert,就会变成静默截断)。

建议:下次构建后,先跑一下上面这段大小对比逻辑(或者直接看 `ls -la out_e5/arch/arm64/boot/
Image` 的字节数),确认没有踩线,再进入 `repack_boot_e5.sh`。如果确实超了,`DEBUG_INFO_BTF`
是第一个该怀疑的配置项。

## 6. 另一个不相关的问题:昨天(09-08 16:47)system_server 崩溃循环

`/blackbox/unisoc-stability/rescueParty/1/rescueparty.logcat` 记录了 09-08 16:47 前后
system_server 反复重启(`SystemServerTiming: StartWatchdog` 每隔 ~4 秒用一个新 pid 重新出
现,`PackageWatchdog` 反复 "Syncing state, reason: added new observer"),触发了 Android
自带的 RescueParty 保护机制。伴随出现 `Cam3Factory: overrideCameraIdIfNeeded: fallback to
0 since no module info found` 的反复告警。**这是用户态(Zygote/system_server)崩溃循环,
和今天的内核早期启动失败是两个独立问题**,没有继续深挖,记在这里供以后需要时参考(对应
的 `rescueparty.kmsg` 是空文件,没有内核侧信息)。

## 7. 关于 git status 里那 13 个"改动"文件

`net/netfilter/xt_DSCP.c`、`xt_MARK.h` 等文件在这台 Mac 上一直显示 `modified`,但**不是真
实差异**:APFS 默认大小写不敏感,`xt_DSCP.c`/`xt_dscp.c`(以及 `xt_MARK.h`/`xt_mark.h` 等
成对文件)在磁盘上被折叠成了同一个 inode,谁后写入谁留在磁盘上,git 因此一直误报。GitHub
上两个文件都完整存在。**不需要处理,也不要往这几个文件里提交任何改动**——除非以后换到大
小写敏感的文件系统/Linux 机器上核实是真实差异。

## 8. 下一步

1. 下次刷机测试,严格按第 3 节的步骤,在能连上 adb 的第一时间抓 `common_rs1_a`——这是目
   前唯一可能看到"内核跳进去之后、reboot 之前"发生了什么的办法。
2. 如果抓到的 kmsg 里能看到 `Linux version 5.15.211...`,说明自编内核确实跑起来过一段,
   再看后面卡在哪一步(大概率还是 vendor_boot 模块版本不匹配,或者其他探测失败);如果连
   `Linux version` 都看不到,说明连内核自己的早期 init 都没走完,是完全不同的问题,需要
   往回查 `earlycon`/`DEBUG_INFO_BTF`/BTF 相关的构建配置。
3. 顺手确认一下 `out_e5/arch/arm64/boot/Image` 的实际大小,别被"打包环节静默失败"坑。
