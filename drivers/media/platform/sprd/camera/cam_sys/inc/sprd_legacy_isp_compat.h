/* SPDX-License-Identifier: GPL-2.0-only */
/* Android 13 ISP layouts reconstructed from Qogirn6L stock module DWARF. */
#ifndef SPRD_LEGACY_ISP_COMPAT_H
#define SPRD_LEGACY_ISP_COMPAT_H
#include <linux/slab.h>
#include <linux/uaccess.h>
static __maybe_unused unsigned long sprd_legacy_copy_block_with_update(void *dst,
		const void __user *src, size_t size)
{
	u32 update_flag = ~0U;

	memcpy(dst, &update_flag, sizeof(update_flag));
	return copy_from_user((u8 *)dst + sizeof(update_flag), src,
		size - sizeof(update_flag));
}
static __maybe_unused unsigned long sprd_legacy_copy_dcam_dev_3dnr_me(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 12);
	if (IS_ERR(old))
		return 12;
	memset(dst, 0, 16);
	memcpy((u8 *)dst + 4, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 4);
	memset((u8 *)dst + 0, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_dcam_dev_blc_info(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 20);
	if (IS_ERR(old))
		return 20;
	memset(dst, 0, 24);
	memcpy((u8 *)dst + 4, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 4);
	memcpy((u8 *)dst + 16, old + 12, 4);
	memcpy((u8 *)dst + 20, old + 16, 4);
	memset((u8 *)dst + 0, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_dcam_dev_hist_info(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 52);
	if (IS_ERR(old))
		return 52;
	memset(dst, 0, 56);
	memcpy((u8 *)dst + 4, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 4);
	memcpy((u8 *)dst + 16, old + 12, 4);
	memcpy((u8 *)dst + 20, old + 16, 4);
	memcpy((u8 *)dst + 24, old + 20, 4);
	memcpy((u8 *)dst + 28, old + 24, 4);
	memcpy((u8 *)dst + 32, old + 28, 4);
	memcpy((u8 *)dst + 36, old + 32, 4);
	memcpy((u8 *)dst + 40, old + 36, 4);
	memcpy((u8 *)dst + 44, old + 40, 4);
	memcpy((u8 *)dst + 48, old + 44, 4);
	memcpy((u8 *)dst + 52, old + 48, 4);
	memset((u8 *)dst + 0, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_dcam_dev_lsc_info(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 80);
	if (IS_ERR(old))
		return 80;
	memset(dst, 0, 88);
	memcpy((u8 *)dst + 4, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 4);
	memcpy((u8 *)dst + 16, old + 12, 4);
	memcpy((u8 *)dst + 20, old + 16, 4);
	memcpy((u8 *)dst + 24, old + 20, 4);
	memcpy((u8 *)dst + 28, old + 24, 4);
	memcpy((u8 *)dst + 32, old + 28, 4);
	memcpy((u8 *)dst + 36, old + 32, 4);
	memcpy((u8 *)dst + 40, old + 36, 4);
	memcpy((u8 *)dst + 48, old + 40, 8);
	memcpy((u8 *)dst + 56, old + 48, 8);
	memcpy((u8 *)dst + 64, old + 56, 8);
	memcpy((u8 *)dst + 72, old + 64, 8);
	memcpy((u8 *)dst + 80, old + 72, 4);
	memcpy((u8 *)dst + 84, old + 76, 4);
	memset((u8 *)dst + 0, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_dcam_dev_rgb_dither_info(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 56);
	if (IS_ERR(old))
		return 56;
	memset(dst, 0, 60);
	memcpy((u8 *)dst + 4, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 4);
	memcpy((u8 *)dst + 16, old + 12, 4);
	memcpy((u8 *)dst + 20, old + 16, 4);
	memcpy((u8 *)dst + 24, old + 20, 4);
	memcpy((u8 *)dst + 28, old + 24, 32);
	memset((u8 *)dst + 0, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_dcam_dev_rgb_gain_info(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 20);
	if (IS_ERR(old))
		return 20;
	memset(dst, 0, 24);
	memcpy((u8 *)dst + 4, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 4);
	memcpy((u8 *)dst + 16, old + 12, 4);
	memcpy((u8 *)dst + 20, old + 16, 4);
	memset((u8 *)dst + 0, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_dcam_dev_rgb_gtm_block_info(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 972);
	if (IS_ERR(old))
		return 972;
	memset(dst, 0, 980);
	memcpy((u8 *)dst + 4, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 4);
	memcpy((u8 *)dst + 16, old + 12, 4);
	memcpy((u8 *)dst + 20, old + 16, 12);
	memcpy((u8 *)dst + 32, old + 28, 4);
	memcpy((u8 *)dst + 36, old + 32, 4);
	memcpy((u8 *)dst + 40, old + 36, 4);
	memcpy((u8 *)dst + 44, old + 40, 4);
	memcpy((u8 *)dst + 48, old + 44, 4);
	memcpy((u8 *)dst + 52, old + 48, 4);
	memcpy((u8 *)dst + 56, old + 52, 4);
	memcpy((u8 *)dst + 60, old + 56, 4);
	memcpy((u8 *)dst + 64, old + 60, 4);
	memcpy((u8 *)dst + 68, old + 64, 4);
	memcpy((u8 *)dst + 72, old + 68, 4);
	memcpy((u8 *)dst + 76, old + 72, 4);
	memcpy((u8 *)dst + 80, old + 76, 4);
	memcpy((u8 *)dst + 84, old + 80, 4);
	memcpy((u8 *)dst + 88, old + 84, 4);
	memcpy((u8 *)dst + 92, old + 88, 4);
	memcpy((u8 *)dst + 96, old + 92, 4);
	memcpy((u8 *)dst + 100, old + 96, 4);
	memcpy((u8 *)dst + 104, old + 100, 4);
	memcpy((u8 *)dst + 108, old + 104, 4);
	memcpy((u8 *)dst + 112, old + 108, 36);
	memcpy((u8 *)dst + 148, old + 144, 4);
	memcpy((u8 *)dst + 152, old + 148, 4);
	memcpy((u8 *)dst + 156, old + 152, 4);
	memcpy((u8 *)dst + 160, old + 156, 4);
	memcpy((u8 *)dst + 164, old + 160, 4);
	memcpy((u8 *)dst + 168, old + 164, 256);
	memcpy((u8 *)dst + 424, old + 420, 20);
	memcpy((u8 *)dst + 444, old + 440, 4);
	memcpy((u8 *)dst + 448, old + 444, 4);
	memcpy((u8 *)dst + 452, old + 448, 4);
	memcpy((u8 *)dst + 456, old + 452, 4);
	memcpy((u8 *)dst + 460, old + 456, 196);
	memcpy((u8 *)dst + 656, old + 652, 76);
	memcpy((u8 *)dst + 732, old + 728, 244);
	memset((u8 *)dst + 0, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_3dlut_info(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 17504);
	if (IS_ERR(old))
		return 17504;
	memset(dst, 0, 17508);
	memcpy((u8 *)dst + 0, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 17496);
	memset((u8 *)dst + 4, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_3dnr_info_v1(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 472);
	if (IS_ERR(old))
		return 472;
	memset(dst, 0, 480);
	memcpy((u8 *)dst + 4, old + 0, 12);
	memcpy((u8 *)dst + 16, old + 12, 4);
	memcpy((u8 *)dst + 24, old + 16, 4);
	memcpy((u8 *)dst + 28, old + 20, 4);
	memcpy((u8 *)dst + 32, old + 24, 4);
	memcpy((u8 *)dst + 36, old + 28, 16);
	memcpy((u8 *)dst + 52, old + 44, 16);
	memcpy((u8 *)dst + 68, old + 60, 16);
	memcpy((u8 *)dst + 84, old + 76, 4);
	memcpy((u8 *)dst + 88, old + 80, 4);
	memcpy((u8 *)dst + 92, old + 84, 4);
	memcpy((u8 *)dst + 96, old + 88, 4);
	memcpy((u8 *)dst + 100, old + 92, 4);
	memcpy((u8 *)dst + 104, old + 96, 4);
	memcpy((u8 *)dst + 108, old + 100, 4);
	memcpy((u8 *)dst + 112, old + 104, 4);
	memcpy((u8 *)dst + 116, old + 108, 4);
	memcpy((u8 *)dst + 120, old + 112, 4);
	memcpy((u8 *)dst + 124, old + 116, 4);
	memcpy((u8 *)dst + 128, old + 120, 4);
	memcpy((u8 *)dst + 132, old + 124, 4);
	memcpy((u8 *)dst + 136, old + 128, 4);
	memcpy((u8 *)dst + 140, old + 132, 4);
	memcpy((u8 *)dst + 144, old + 136, 4);
	memcpy((u8 *)dst + 148, old + 140, 4);
	memcpy((u8 *)dst + 152, old + 144, 4);
	memcpy((u8 *)dst + 156, old + 148, 4);
	memcpy((u8 *)dst + 160, old + 152, 4);
	memcpy((u8 *)dst + 164, old + 156, 4);
	memcpy((u8 *)dst + 168, old + 160, 4);
	memcpy((u8 *)dst + 172, old + 164, 4);
	memcpy((u8 *)dst + 176, old + 168, 4);
	memcpy((u8 *)dst + 180, old + 172, 4);
	memcpy((u8 *)dst + 184, old + 176, 4);
	memcpy((u8 *)dst + 188, old + 180, 4);
	memcpy((u8 *)dst + 192, old + 184, 4);
	memcpy((u8 *)dst + 196, old + 188, 4);
	memcpy((u8 *)dst + 200, old + 192, 4);
	memcpy((u8 *)dst + 204, old + 196, 4);
	memcpy((u8 *)dst + 208, old + 200, 4);
	memcpy((u8 *)dst + 212, old + 204, 4);
	memcpy((u8 *)dst + 216, old + 208, 4);
	memcpy((u8 *)dst + 220, old + 212, 4);
	memcpy((u8 *)dst + 224, old + 216, 4);
	memcpy((u8 *)dst + 228, old + 220, 4);
	memcpy((u8 *)dst + 232, old + 224, 4);
	memcpy((u8 *)dst + 236, old + 228, 4);
	memcpy((u8 *)dst + 240, old + 232, 4);
	memcpy((u8 *)dst + 244, old + 236, 4);
	memcpy((u8 *)dst + 248, old + 240, 4);
	memcpy((u8 *)dst + 252, old + 244, 4);
	memcpy((u8 *)dst + 256, old + 248, 4);
	memcpy((u8 *)dst + 260, old + 252, 4);
	memcpy((u8 *)dst + 264, old + 256, 4);
	memcpy((u8 *)dst + 268, old + 260, 4);
	memcpy((u8 *)dst + 272, old + 264, 4);
	memcpy((u8 *)dst + 276, old + 268, 4);
	memcpy((u8 *)dst + 280, old + 272, 4);
	memcpy((u8 *)dst + 284, old + 276, 4);
	memcpy((u8 *)dst + 288, old + 280, 4);
	memcpy((u8 *)dst + 292, old + 284, 4);
	memcpy((u8 *)dst + 296, old + 288, 4);
	memcpy((u8 *)dst + 300, old + 292, 4);
	memcpy((u8 *)dst + 304, old + 296, 4);
	memcpy((u8 *)dst + 308, old + 300, 4);
	memcpy((u8 *)dst + 312, old + 304, 4);
	memcpy((u8 *)dst + 316, old + 308, 4);
	memcpy((u8 *)dst + 320, old + 312, 4);
	memcpy((u8 *)dst + 324, old + 316, 4);
	memcpy((u8 *)dst + 328, old + 320, 4);
	memcpy((u8 *)dst + 332, old + 324, 4);
	memcpy((u8 *)dst + 336, old + 328, 4);
	memcpy((u8 *)dst + 340, old + 332, 4);
	memcpy((u8 *)dst + 344, old + 336, 4);
	memcpy((u8 *)dst + 348, old + 340, 4);
	memcpy((u8 *)dst + 352, old + 344, 4);
	memcpy((u8 *)dst + 356, old + 348, 4);
	memcpy((u8 *)dst + 360, old + 352, 4);
	memcpy((u8 *)dst + 364, old + 356, 4);
	memcpy((u8 *)dst + 368, old + 360, 4);
	memcpy((u8 *)dst + 372, old + 364, 4);
	memcpy((u8 *)dst + 376, old + 368, 4);
	memcpy((u8 *)dst + 380, old + 372, 4);
	memcpy((u8 *)dst + 384, old + 376, 4);
	memcpy((u8 *)dst + 388, old + 380, 4);
	memcpy((u8 *)dst + 392, old + 384, 4);
	memcpy((u8 *)dst + 396, old + 388, 4);
	memcpy((u8 *)dst + 400, old + 392, 4);
	memcpy((u8 *)dst + 404, old + 396, 4);
	memcpy((u8 *)dst + 408, old + 400, 4);
	memcpy((u8 *)dst + 412, old + 404, 4);
	memcpy((u8 *)dst + 416, old + 408, 4);
	memcpy((u8 *)dst + 420, old + 412, 4);
	memcpy((u8 *)dst + 424, old + 416, 4);
	memcpy((u8 *)dst + 428, old + 420, 4);
	memcpy((u8 *)dst + 432, old + 424, 4);
	memcpy((u8 *)dst + 436, old + 428, 4);
	memcpy((u8 *)dst + 440, old + 432, 4);
	memcpy((u8 *)dst + 444, old + 436, 4);
	memcpy((u8 *)dst + 448, old + 440, 4);
	memcpy((u8 *)dst + 452, old + 444, 4);
	memcpy((u8 *)dst + 456, old + 448, 4);
	memcpy((u8 *)dst + 460, old + 452, 4);
	memcpy((u8 *)dst + 464, old + 456, 4);
	memcpy((u8 *)dst + 468, old + 460, 4);
	memcpy((u8 *)dst + 472, old + 464, 4);
	memcpy((u8 *)dst + 476, old + 468, 4);
	memset((u8 *)dst + 0, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_bchs_info(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 48);
	if (IS_ERR(old))
		return 48;
	memset(dst, 0, 52);
	memcpy((u8 *)dst + 0, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 4);
	memcpy((u8 *)dst + 16, old + 12, 4);
	memcpy((u8 *)dst + 20, old + 16, 4);
	memcpy((u8 *)dst + 24, old + 20, 4);
	memcpy((u8 *)dst + 28, old + 24, 4);
	memcpy((u8 *)dst + 32, old + 28, 4);
	memcpy((u8 *)dst + 36, old + 32, 4);
	memcpy((u8 *)dst + 40, old + 36, 4);
	memcpy((u8 *)dst + 44, old + 40, 4);
	memcpy((u8 *)dst + 48, old + 44, 4);
	memset((u8 *)dst + 4, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_brightness_info(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 12);
	if (IS_ERR(old))
		return 12;
	memset(dst, 0, 16);
	memcpy((u8 *)dst + 0, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 4);
	memset((u8 *)dst + 4, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_cce_info(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 32);
	if (IS_ERR(old))
		return 32;
	memset(dst, 0, 36);
	memcpy((u8 *)dst + 0, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 18);
	memcpy((u8 *)dst + 30, old + 26, 2);
	memcpy((u8 *)dst + 32, old + 28, 2);
	memcpy((u8 *)dst + 34, old + 30, 2);
	memset((u8 *)dst + 4, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_cdn_info(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 296);
	if (IS_ERR(old))
		return 296;
	memset(dst, 0, 300);
	memcpy((u8 *)dst + 0, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 4);
	memcpy((u8 *)dst + 16, old + 12, 4);
	memcpy((u8 *)dst + 20, old + 16, 4);
	memcpy((u8 *)dst + 24, old + 20, 4);
	memcpy((u8 *)dst + 28, old + 24, 4);
	memcpy((u8 *)dst + 32, old + 28, 4);
	memcpy((u8 *)dst + 36, old + 32, 4);
	memcpy((u8 *)dst + 40, old + 36, 4);
	memcpy((u8 *)dst + 44, old + 40, 4);
	memcpy((u8 *)dst + 48, old + 44, 124);
	memcpy((u8 *)dst + 172, old + 168, 124);
	memcpy((u8 *)dst + 296, old + 292, 4);
	memset((u8 *)dst + 4, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_cfa_info_v1(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 64);
	if (IS_ERR(old))
		return 64;
	memset(dst, 0, 68);
	memcpy((u8 *)dst + 4, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 4);
	memcpy((u8 *)dst + 16, old + 12, 4);
	memcpy((u8 *)dst + 20, old + 16, 4);
	memcpy((u8 *)dst + 24, old + 20, 4);
	memcpy((u8 *)dst + 28, old + 24, 4);
	memcpy((u8 *)dst + 32, old + 28, 4);
	memcpy((u8 *)dst + 36, old + 32, 4);
	memcpy((u8 *)dst + 40, old + 36, 4);
	memcpy((u8 *)dst + 44, old + 40, 4);
	memcpy((u8 *)dst + 48, old + 44, 4);
	memcpy((u8 *)dst + 52, old + 48, 4);
	memcpy((u8 *)dst + 56, old + 52, 4);
	memcpy((u8 *)dst + 60, old + 56, 4);
	memcpy((u8 *)dst + 64, old + 60, 4);
	memset((u8 *)dst + 0, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_cmc10_info(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 28);
	if (IS_ERR(old))
		return 28;
	memset(dst, 0, 32);
	memcpy((u8 *)dst + 0, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 20);
	memset((u8 *)dst + 4, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_cnr_h_info(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 9132);
	if (IS_ERR(old))
		return 9132;
	memset(dst, 0, 9136);
	memcpy((u8 *)dst + 0, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 4);
	memcpy((u8 *)dst + 16, old + 12, 9120);
	memset((u8 *)dst + 4, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_contrast_info(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 12);
	if (IS_ERR(old))
		return 12;
	memset(dst, 0, 16);
	memcpy((u8 *)dst + 0, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 4);
	memset((u8 *)dst + 4, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_csa_info(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 16);
	if (IS_ERR(old))
		return 16;
	memset(dst, 0, 20);
	memcpy((u8 *)dst + 0, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 4);
	memcpy((u8 *)dst + 16, old + 12, 4);
	memset((u8 *)dst + 4, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_dct_info(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 200);
	if (IS_ERR(old))
		return 200;
	memset(dst, 0, 204);
	memcpy((u8 *)dst + 0, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 4);
	memcpy((u8 *)dst + 16, old + 12, 4);
	memcpy((u8 *)dst + 20, old + 16, 4);
	memcpy((u8 *)dst + 24, old + 20, 4);
	memcpy((u8 *)dst + 28, old + 24, 4);
	memcpy((u8 *)dst + 32, old + 28, 4);
	memcpy((u8 *)dst + 36, old + 32, 4);
	memcpy((u8 *)dst + 40, old + 36, 4);
	memcpy((u8 *)dst + 44, old + 40, 4);
	memcpy((u8 *)dst + 48, old + 44, 4);
	memcpy((u8 *)dst + 52, old + 48, 4);
	memcpy((u8 *)dst + 56, old + 52, 4);
	memcpy((u8 *)dst + 60, old + 56, 4);
	memcpy((u8 *)dst + 64, old + 60, 4);
	memcpy((u8 *)dst + 68, old + 64, 4);
	memcpy((u8 *)dst + 72, old + 68, 4);
	memcpy((u8 *)dst + 76, old + 72, 4);
	memcpy((u8 *)dst + 80, old + 76, 4);
	memcpy((u8 *)dst + 84, old + 80, 4);
	memcpy((u8 *)dst + 88, old + 84, 4);
	memcpy((u8 *)dst + 92, old + 88, 4);
	memcpy((u8 *)dst + 96, old + 92, 4);
	memcpy((u8 *)dst + 100, old + 96, 4);
	memcpy((u8 *)dst + 104, old + 100, 4);
	memcpy((u8 *)dst + 108, old + 104, 4);
	memcpy((u8 *)dst + 112, old + 108, 4);
	memcpy((u8 *)dst + 116, old + 112, 4);
	memcpy((u8 *)dst + 120, old + 116, 4);
	memcpy((u8 *)dst + 124, old + 120, 4);
	memcpy((u8 *)dst + 128, old + 124, 4);
	memcpy((u8 *)dst + 132, old + 128, 4);
	memcpy((u8 *)dst + 136, old + 132, 4);
	memcpy((u8 *)dst + 140, old + 136, 4);
	memcpy((u8 *)dst + 144, old + 140, 4);
	memcpy((u8 *)dst + 148, old + 144, 4);
	memcpy((u8 *)dst + 152, old + 148, 4);
	memcpy((u8 *)dst + 156, old + 152, 4);
	memcpy((u8 *)dst + 160, old + 156, 4);
	memcpy((u8 *)dst + 164, old + 160, 4);
	memcpy((u8 *)dst + 168, old + 164, 4);
	memcpy((u8 *)dst + 172, old + 168, 4);
	memcpy((u8 *)dst + 176, old + 172, 4);
	memcpy((u8 *)dst + 180, old + 176, 4);
	memcpy((u8 *)dst + 184, old + 180, 4);
	memcpy((u8 *)dst + 188, old + 184, 4);
	memcpy((u8 *)dst + 192, old + 188, 4);
	memcpy((u8 *)dst + 196, old + 192, 4);
	memcpy((u8 *)dst + 200, old + 196, 4);
	memset((u8 *)dst + 4, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_edge_info_v3(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 852);
	if (IS_ERR(old))
		return 852;
	memset(dst, 0, 856);
	memcpy((u8 *)dst + 0, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 4);
	memcpy((u8 *)dst + 16, old + 12, 4);
	memcpy((u8 *)dst + 20, old + 16, 4);
	memcpy((u8 *)dst + 24, old + 20, 4);
	memcpy((u8 *)dst + 28, old + 24, 4);
	memcpy((u8 *)dst + 32, old + 28, 4);
	memcpy((u8 *)dst + 36, old + 32, 8);
	memcpy((u8 *)dst + 44, old + 40, 8);
	memcpy((u8 *)dst + 52, old + 48, 8);
	memcpy((u8 *)dst + 60, old + 56, 8);
	memcpy((u8 *)dst + 68, old + 64, 8);
	memcpy((u8 *)dst + 76, old + 72, 8);
	memcpy((u8 *)dst + 84, old + 80, 8);
	memcpy((u8 *)dst + 92, old + 88, 4);
	memcpy((u8 *)dst + 96, old + 92, 4);
	memcpy((u8 *)dst + 100, old + 96, 4);
	memcpy((u8 *)dst + 104, old + 100, 4);
	memcpy((u8 *)dst + 108, old + 104, 4);
	memcpy((u8 *)dst + 112, old + 108, 4);
	memcpy((u8 *)dst + 116, old + 112, 4);
	memcpy((u8 *)dst + 120, old + 116, 4);
	memcpy((u8 *)dst + 124, old + 120, 4);
	memcpy((u8 *)dst + 128, old + 124, 4);
	memcpy((u8 *)dst + 132, old + 128, 4);
	memcpy((u8 *)dst + 136, old + 132, 4);
	memcpy((u8 *)dst + 140, old + 136, 4);
	memcpy((u8 *)dst + 144, old + 140, 32);
	memcpy((u8 *)dst + 176, old + 172, 24);
	memcpy((u8 *)dst + 200, old + 196, 4);
	memcpy((u8 *)dst + 204, old + 200, 32);
	memcpy((u8 *)dst + 236, old + 232, 24);
	memcpy((u8 *)dst + 260, old + 256, 16);
	memcpy((u8 *)dst + 276, old + 272, 12);
	memcpy((u8 *)dst + 288, old + 284, 16);
	memcpy((u8 *)dst + 304, old + 300, 12);
	memcpy((u8 *)dst + 316, old + 312, 16);
	memcpy((u8 *)dst + 332, old + 328, 12);
	memcpy((u8 *)dst + 344, old + 340, 12);
	memcpy((u8 *)dst + 356, old + 352, 16);
	memcpy((u8 *)dst + 372, old + 368, 12);
	memcpy((u8 *)dst + 384, old + 380, 12);
	memcpy((u8 *)dst + 396, old + 392, 4);
	memcpy((u8 *)dst + 400, old + 396, 4);
	memcpy((u8 *)dst + 404, old + 400, 4);
	memcpy((u8 *)dst + 408, old + 404, 4);
	memcpy((u8 *)dst + 412, old + 408, 32);
	memcpy((u8 *)dst + 444, old + 440, 24);
	memcpy((u8 *)dst + 468, old + 464, 24);
	memcpy((u8 *)dst + 492, old + 488, 32);
	memcpy((u8 *)dst + 524, old + 520, 24);
	memcpy((u8 *)dst + 548, old + 544, 24);
	memcpy((u8 *)dst + 572, old + 568, 24);
	memcpy((u8 *)dst + 596, old + 592, 24);
	memcpy((u8 *)dst + 620, old + 616, 4);
	memcpy((u8 *)dst + 624, old + 620, 16);
	memcpy((u8 *)dst + 640, old + 636, 12);
	memcpy((u8 *)dst + 652, old + 648, 12);
	memcpy((u8 *)dst + 664, old + 660, 16);
	memcpy((u8 *)dst + 680, old + 676, 12);
	memcpy((u8 *)dst + 692, old + 688, 12);
	memcpy((u8 *)dst + 704, old + 700, 12);
	memcpy((u8 *)dst + 716, old + 712, 12);
	memcpy((u8 *)dst + 728, old + 724, 16);
	memcpy((u8 *)dst + 744, old + 740, 16);
	memcpy((u8 *)dst + 760, old + 756, 4);
	memcpy((u8 *)dst + 764, old + 760, 4);
	memcpy((u8 *)dst + 768, old + 764, 4);
	memcpy((u8 *)dst + 772, old + 768, 4);
	memcpy((u8 *)dst + 776, old + 772, 4);
	memcpy((u8 *)dst + 780, old + 776, 4);
	memcpy((u8 *)dst + 784, old + 780, 4);
	memcpy((u8 *)dst + 788, old + 784, 4);
	memcpy((u8 *)dst + 792, old + 788, 4);
	memcpy((u8 *)dst + 796, old + 792, 4);
	memcpy((u8 *)dst + 800, old + 796, 4);
	memcpy((u8 *)dst + 804, old + 800, 4);
	memcpy((u8 *)dst + 808, old + 804, 4);
	memcpy((u8 *)dst + 812, old + 808, 4);
	memcpy((u8 *)dst + 816, old + 812, 4);
	memcpy((u8 *)dst + 820, old + 816, 4);
	memcpy((u8 *)dst + 824, old + 820, 4);
	memcpy((u8 *)dst + 828, old + 824, 4);
	memcpy((u8 *)dst + 832, old + 828, 4);
	memcpy((u8 *)dst + 836, old + 832, 4);
	memcpy((u8 *)dst + 840, old + 836, 4);
	memcpy((u8 *)dst + 844, old + 840, 4);
	memcpy((u8 *)dst + 848, old + 844, 4);
	memcpy((u8 *)dst + 852, old + 848, 4);
	memset((u8 *)dst + 4, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_gamma_info_v1(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 12304);
	if (IS_ERR(old))
		return 12304;
	memset(dst, 0, 12308);
	memcpy((u8 *)dst + 4, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4100);
	memcpy((u8 *)dst + 4108, old + 4104, 4100);
	memcpy((u8 *)dst + 8208, old + 8204, 4100);
	memset((u8 *)dst + 0, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_hist2_info(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 40);
	if (IS_ERR(old))
		return 40;
	memset(dst, 0, 44);
	memcpy((u8 *)dst + 0, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 4);
	memcpy((u8 *)dst + 16, old + 12, 4);
	memcpy((u8 *)dst + 20, old + 16, 4);
	memcpy((u8 *)dst + 24, old + 20, 16);
	memcpy((u8 *)dst + 40, old + 36, 4);
	memset((u8 *)dst + 4, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_hsv_info_v3(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 3676);
	if (IS_ERR(old))
		return 3676;
	memset(dst, 0, 3680);
	memcpy((u8 *)dst + 0, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 3404);
	memcpy((u8 *)dst + 3416, old + 3412, 4);
	memcpy((u8 *)dst + 3420, old + 3416, 24);
	memcpy((u8 *)dst + 3444, old + 3440, 64);
	memcpy((u8 *)dst + 3508, old + 3504, 4);
	memcpy((u8 *)dst + 3512, old + 3508, 68);
	memcpy((u8 *)dst + 3580, old + 3576, 100);
	memset((u8 *)dst + 4, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_hsv_info_v4(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 3752);
	if (IS_ERR(old))
		return 3752;
	memset(dst, 0, 3756);
	memcpy((u8 *)dst + 0, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 3404);
	memcpy((u8 *)dst + 3416, old + 3412, 4);
	memcpy((u8 *)dst + 3420, old + 3416, 4);
	memcpy((u8 *)dst + 3424, old + 3420, 32);
	memcpy((u8 *)dst + 3456, old + 3452, 64);
	memcpy((u8 *)dst + 3520, old + 3516, 64);
	memcpy((u8 *)dst + 3584, old + 3580, 4);
	memcpy((u8 *)dst + 3588, old + 3584, 68);
	memcpy((u8 *)dst + 3656, old + 3652, 100);
	memset((u8 *)dst + 4, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_hue_info(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 12);
	if (IS_ERR(old))
		return 12;
	memset(dst, 0, 16);
	memcpy((u8 *)dst + 4, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 4);
	memset((u8 *)dst + 0, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_noise_filter_info(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 120);
	if (IS_ERR(old))
		return 120;
	memset(dst, 0, 124);
	memcpy((u8 *)dst + 0, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 4);
	memcpy((u8 *)dst + 16, old + 12, 4);
	memcpy((u8 *)dst + 20, old + 16, 4);
	memcpy((u8 *)dst + 24, old + 20, 4);
	memcpy((u8 *)dst + 28, old + 24, 16);
	memcpy((u8 *)dst + 44, old + 40, 32);
	memcpy((u8 *)dst + 76, old + 72, 4);
	memcpy((u8 *)dst + 80, old + 76, 4);
	memcpy((u8 *)dst + 84, old + 80, 4);
	memcpy((u8 *)dst + 88, old + 84, 16);
	memcpy((u8 *)dst + 104, old + 100, 12);
	memcpy((u8 *)dst + 116, old + 112, 8);
	memset((u8 *)dst + 4, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_post_cnr_h_info(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 1836);
	if (IS_ERR(old))
		return 1836;
	memset(dst, 0, 1840);
	memcpy((u8 *)dst + 0, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 4);
	memcpy((u8 *)dst + 16, old + 12, 1824);
	memset((u8 *)dst + 4, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_rgb_ltm_info(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 600);
	if (IS_ERR(old))
		return 600;
	memset(dst, 0, 616);
	memcpy((u8 *)dst + 0, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 560);
	memcpy((u8 *)dst + 568, old + 564, 4);
	memcpy((u8 *)dst + 572, old + 568, 4);
	memcpy((u8 *)dst + 576, old + 572, 4);
	memcpy((u8 *)dst + 580, old + 576, 4);
	memcpy((u8 *)dst + 584, old + 580, 4);
	memcpy((u8 *)dst + 588, old + 584, 4);
	memcpy((u8 *)dst + 592, old + 588, 4);
	memcpy((u8 *)dst + 596, old + 592, 4);
	memset((u8 *)dst + 4, 0xff, sizeof(u32));
	/* Legacy LTM map FDs have no direct-map equivalent in this driver. */
	memset((u8 *)dst + 568, 0, 48);
	((u32 *)((u8 *)dst + 568))[0] = 1;
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_uvd_info_v1(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 112);
	if (IS_ERR(old))
		return 112;
	memset(dst, 0, 116);
	memcpy((u8 *)dst + 0, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 4);
	memcpy((u8 *)dst + 16, old + 12, 4);
	memcpy((u8 *)dst + 20, old + 16, 4);
	memcpy((u8 *)dst + 24, old + 20, 4);
	memcpy((u8 *)dst + 28, old + 24, 4);
	memcpy((u8 *)dst + 32, old + 28, 4);
	memcpy((u8 *)dst + 36, old + 32, 16);
	memcpy((u8 *)dst + 52, old + 48, 16);
	memcpy((u8 *)dst + 68, old + 64, 4);
	memcpy((u8 *)dst + 72, old + 68, 4);
	memcpy((u8 *)dst + 76, old + 72, 8);
	memcpy((u8 *)dst + 84, old + 80, 4);
	memcpy((u8 *)dst + 88, old + 84, 4);
	memcpy((u8 *)dst + 92, old + 88, 4);
	memcpy((u8 *)dst + 96, old + 92, 4);
	memcpy((u8 *)dst + 100, old + 96, 4);
	memcpy((u8 *)dst + 104, old + 100, 4);
	memcpy((u8 *)dst + 108, old + 104, 4);
	memcpy((u8 *)dst + 112, old + 108, 4);
	memset((u8 *)dst + 4, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_ygamma_info_v1(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 2060);
	if (IS_ERR(old))
		return 2060;
	memset(dst, 0, 2064);
	memcpy((u8 *)dst + 0, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 2052);
	memset((u8 *)dst + 4, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_ynr_info_v3(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 268);
	if (IS_ERR(old))
		return 268;
	memset(dst, 0, 272);
	memcpy((u8 *)dst + 0, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 4);
	memcpy((u8 *)dst + 16, old + 12, 4);
	memcpy((u8 *)dst + 20, old + 16, 4);
	memcpy((u8 *)dst + 24, old + 20, 4);
	memcpy((u8 *)dst + 28, old + 24, 4);
	memcpy((u8 *)dst + 32, old + 28, 240);
	memset((u8 *)dst + 4, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
static __maybe_unused unsigned long sprd_legacy_copy_isp_dev_yrandom_info(void *dst, const void __user *src)
{
	u8 *old = memdup_user(src, 32);
	if (IS_ERR(old))
		return 32;
	memset(dst, 0, 36);
	memcpy((u8 *)dst + 0, old + 0, 4);
	memcpy((u8 *)dst + 8, old + 4, 4);
	memcpy((u8 *)dst + 12, old + 8, 4);
	memcpy((u8 *)dst + 16, old + 12, 4);
	memcpy((u8 *)dst + 20, old + 16, 4);
	memcpy((u8 *)dst + 24, old + 20, 4);
	memcpy((u8 *)dst + 28, old + 24, 8);
	memset((u8 *)dst + 4, 0xff, sizeof(u32));
	kfree(old);
	return 0;
}
#endif
