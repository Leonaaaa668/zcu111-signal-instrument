#ifndef RX_MEMORY_MAP_H
#define RX_MEMORY_MAP_H
/* Diagnostic standalone-only allocations. NOT a Linux physical-address ABI. */
#define RX_MAILBOX_ADDR 0x20400000u
#define RX_MAILBOX_MAGIC 0x52584431u
#define RX_DMA_BASE 0xB00C0000u
#define RX_RESULT_BASE 0xB00D0000u
#define RX_GPIO_BASE 0xB00B0000u
#define RX_BD_ADDR 0x30000000u
#define RX_BUFFER0 0x30010000u
#define RX_BUFFER1 0x30100000u
#define RX_MAX_PACKET (256u+8u*65536u)
#define RX_WORK_END 0x30200000u
_Static_assert(RX_BUFFER0+RX_MAX_PACKET<=RX_BUFFER1,"RX buffers overlap");
_Static_assert(RX_BUFFER1+RX_MAX_PACKET<=RX_WORK_END,"RX work region exceeded");
_Static_assert(RX_MAILBOX_ADDR>=0x20400000u && RX_MAILBOX_ADDR<0x20600000u,"RX mailbox block");
_Static_assert(0x28000000u+67108864u<=RX_BD_ADDR,"TX/RX memory overlap");
#endif
