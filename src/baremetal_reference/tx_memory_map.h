#ifndef TX_MEMORY_MAP_H
#define TX_MEMORY_MAP_H
/* Baremetal-only reserved PS DDR. No Linux or concurrent application use. */
#define TX_SOURCE_ADDR 0x24000000u
#define TX_READBACK_ADDR 0x28000000u
#define TX_MAILBOX_ADDR 0x20300000u
#define TX_MAX_BYTES (64u*1024u*1024u)
#define TX_COPY_CHUNK_BYTES (4u*1024u*1024u)
#define TX_MMU_BLOCK_BYTES (2u*1024u*1024u)
#define TX_PROTOCOL_VERSION 2u
/* Change magic: old scripts must reject v2 before writing the old PS buffer. */
#define TX_MAILBOX_MAGIC 0x54585757u
_Static_assert(TX_SOURCE_ADDR+TX_MAX_BYTES<=TX_READBACK_ADDR,"PS buffers overlap");
_Static_assert(TX_READBACK_ADDR+TX_MAX_BYTES<=0x80000000u,"Outside PS DDR low window");
_Static_assert(TX_MAILBOX_ADDR+TX_MMU_BLOCK_BYTES<=TX_SOURCE_ADDR,"Mailbox overlaps upload");
_Static_assert(TX_MAX_BYTES%256u==0u,"DMA descriptor alignment");
_Static_assert(TX_MAX_BYTES/4u<0x03ffffffu,"26-bit descriptor length exceeded");
#endif
