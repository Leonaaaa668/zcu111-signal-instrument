/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef ZCU111_TX_UAPI_H
#define ZCU111_TX_UAPI_H
#include <linux/types.h>
#include <linux/ioctl.h>
#define ZTX_ABI_VERSION 1U
#define ZTX_MAX_BYTES (64U*1024U*1024U)
#define ZTX_F_PREPARED 1U
#define ZTX_F_LOADED 2U
#define ZTX_F_PLAYING 4U
#define ZTX_F_LOADING 8U
#define ZTX_F_FAULT 16U
struct ztx_info {
 __u32 abi_version, struct_bytes, flags, guard_status;
 __u32 cdma_status, mm2s_status, dac_state, last_errno;
 __aligned_u64 max_bytes, loaded_bytes, received_bytes, session, nco_word;
 __u32 reserved[6];
};
struct ztx_begin {
 __aligned_u64 bytes;
 __u8 sha256[32];
 __u32 reserved[6];
};
#define ZTX_IOC_INFO _IOR('V', 1, struct ztx_info)
#define ZTX_IOC_BEGIN _IOW('V', 2, struct ztx_begin)
#define ZTX_IOC_COMMIT _IO('V', 3)
#define ZTX_IOC_ABORT _IO('V', 4)
#define ZTX_IOC_PLAY _IO('V', 5)
#define ZTX_IOC_STOP _IO('V', 6)
/* Root board-init service calls only after official RFDC/clock validation. */
#define ZTX_IOC_PREPARED _IO('V', 7)
/* Explicit root diagnostic: register reads only, no DMA reset or transfer. */
struct ztx_dma_diag { __u32 cdma_status, mm2s_status; };
#define ZTX_IOC_DIAG _IOR('V', 8, struct ztx_dma_diag)
#endif
