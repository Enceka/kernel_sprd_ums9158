/* SPDX-License-Identifier: GPL-2.0-only */
/* Qogirn6L Android 13 event ABI, verified against stock module DWARF. */
#ifndef SPRD_LEGACY_CAMERA_EVENT_H
#define SPRD_LEGACY_CAMERA_EVENT_H
#pragma pack(push, 4)
struct sprd_legacy_img_frame {
	u32 channel_id;
	u32 height;
	u32 length;
	u32 sec;
	u32 usec;
	u32 frm_base_id;
	u32 index;
	u32 real_index;
	u32 img_fmt;
	u32 yaddr;
	u32 uaddr;
	u32 vaddr;
	u32 yaddr_vir;
	u32 uaddr_vir;
	u32 vaddr_vir;
	u32 irq_type;
	u32 irq_property;
	u32 frame_id;
	u32 phy_addr;
	u32 vir_addr;
	u32 addr_offset;
	u32 kaddr[2];
	u32 buf_size;
	s64 monoboottime;
	u32 mfd, zoom_ratio, total_zoom;
	struct sprd_img_vcm_dac_info dac_info;
	u32 reserved[4];
	u32 is_flash_status;
	u32 tile_num_x, tile_num_y, tile_width, tile_height;
};
struct sprd_legacy_img_read_op {
	u32 cmd, evt, sensor_id;
	union {
		struct sprd_legacy_img_frame frame;
		struct sprd_img_path_capability capability;
		u32 reserved[20];
	} parm;
};
#pragma pack(pop)
static_assert(sizeof(struct sprd_img_parm) == 2752);
static_assert(sizeof(struct sprd_img_function_mode) == 56);
static_assert(sizeof(struct sprd_legacy_img_frame) == 180);
static_assert(sizeof(struct sprd_legacy_img_read_op) == 196);
static inline void sprd_legacy_camera_event(struct sprd_legacy_img_read_op *out,
		const struct sprd_img_read_op *in)
{
	memset(out, 0, sizeof(*out));
	out->cmd = in->cmd >= SPRD_IMG_STOP_DCAM ? in->cmd - 1 : in->cmd;
	out->evt = in->evt;
	out->sensor_id = in->sensor_id;
	if (in->cmd != SPRD_IMG_GET_FRM_BUFFER && in->cmd != SPRD_IMG_GET_IMG_BUFFER) {
		memcpy(&out->parm, &in->parm, sizeof(out->parm));
		return;
	}
	out->parm.frame.channel_id = in->parm.frame.channel_id;
	out->parm.frame.height = in->parm.frame.height;
	out->parm.frame.length = in->parm.frame.length;
	out->parm.frame.sec = in->parm.frame.sec;
	out->parm.frame.usec = in->parm.frame.usec;
	out->parm.frame.frm_base_id = in->parm.frame.frm_base_id;
	out->parm.frame.index = in->parm.frame.index;
	out->parm.frame.real_index = in->parm.frame.real_index;
	out->parm.frame.img_fmt = in->parm.frame.img_fmt;
	out->parm.frame.yaddr = in->parm.frame.yaddr;
	out->parm.frame.uaddr = in->parm.frame.uaddr;
	out->parm.frame.vaddr = in->parm.frame.vaddr;
	out->parm.frame.yaddr_vir = in->parm.frame.yaddr_vir;
	out->parm.frame.uaddr_vir = in->parm.frame.uaddr_vir;
	out->parm.frame.vaddr_vir = in->parm.frame.vaddr_vir;
	out->parm.frame.irq_type = in->parm.frame.irq_type;
	out->parm.frame.irq_property = in->parm.frame.irq_property;
	out->parm.frame.frame_id = in->parm.frame.frame_id;
	out->parm.frame.phy_addr = in->parm.frame.phy_addr;
	out->parm.frame.vir_addr = in->parm.frame.vir_addr;
	out->parm.frame.addr_offset = in->parm.frame.addr_offset;
	out->parm.frame.buf_size = in->parm.frame.buf_size;
	out->parm.frame.monoboottime = in->parm.frame.monoboottime;
	out->parm.frame.mfd = in->parm.frame.mfd;
	out->parm.frame.zoom_ratio = in->parm.frame.zoom_ratio;
	out->parm.frame.total_zoom = in->parm.frame.total_zoom;
	out->parm.frame.dac_info = in->parm.frame.dac_info;
	out->parm.frame.is_flash_status = in->parm.frame.is_flash_status;
	memcpy(out->parm.frame.kaddr, in->parm.frame.kaddr, sizeof(out->parm.frame.kaddr));
	out->parm.frame.tile_num_x = in->parm.frame.ltm_info.tile_num_x;
	out->parm.frame.tile_num_y = in->parm.frame.ltm_info.tile_num_y;
	out->parm.frame.tile_width = in->parm.frame.ltm_info.tile_width;
	out->parm.frame.tile_height = in->parm.frame.ltm_info.tile_height;
}
#endif
