/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef ZCU111_SA_UAPI_H
#define ZCU111_SA_UAPI_H
#include <linux/types.h>
#include <linux/ioctl.h>
struct zsa_config { __u32 log_n; __u32 reserved; };
struct zsa_receipt {
 __u32 ticket_id, epoch, segment_id, nco_update_seq, log_n, zoom;
 __aligned_u64 packet_sequence, counter_epoch;
};
#define ZSA_IOC_CONFIG _IOW('Z', 1, struct zsa_config)
#define ZSA_IOC_RECEIPT _IOR('Z', 2, struct zsa_receipt)
/* ABI2: tuning is performed by the privileged RFDC service while it owns
 * the exclusive RX fd. An interrupted/failed tuning transaction blocks RX. */
struct zsa_tune_state {
 __aligned_u64 center_hz, nco_word;
 __u32 nco_update_seq, flags;
};
struct zsa_config_v2 { __u32 log_n, nco_update_seq; __aligned_u64 center_hz; };
#define ZSA_TUNE_VALID 1U
#define ZSA_TUNE_IN_PROGRESS 2U
#define ZSA_IOC_TUNE_STATE _IOR('Z', 3, struct zsa_tune_state)
#define ZSA_IOC_TUNE_BEGIN _IO('Z', 4)
#define ZSA_IOC_TUNE_COMMIT _IOWR('Z', 5, struct zsa_tune_state)
#define ZSA_IOC_CONFIG_V2 _IOW('Z', 6, struct zsa_config_v2)
#endif
