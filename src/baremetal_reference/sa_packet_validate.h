#ifndef SA_PACKET_VALIDATE_H
#define SA_PACKET_VALIDATE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "sa_window_metrics.h"
/* LE A53 / LE host regression. Caller supplies an aligned immutable DMA buffer. */
struct sa_packet_expect {
    uint32_t epoch,ticket,segment,nco_seq,logn,zoom,control;
    uint64_t raw_nco,counter_epoch;
};
static uint64_t sa_join(uint32_t lo,uint32_t hi){return (uint64_t)lo|((uint64_t)hi<<32);}
static int sa_packet_valid(const void *buffer,size_t length,const struct sa_packet_expect *e)
{
    if(e->logn<7 || e->logn>16 || e->zoom>6 || length!=256u+(8u<<e->logn))return 0;
    const uint32_t *h=(const uint32_t *)buffer;
    const uint64_t *p=(const uint64_t *)((const unsigned char *)buffer+256);
    uint32_t n=1u<<e->logn;uint64_t checksum=0;
    if(h[0]!=0x31504153u || h[1]!=1 || h[2]!=256 || h[3]!=length || h[4]!=0x3f ||
       !sa_join(h[5],h[6]) || h[7]!=e->epoch || h[8]!=e->segment || h[9]!=e->nco_seq || h[10]!=e->ticket ||
       h[11]!=n || h[12]!=e->logn || h[13]!=e->zoom || h[14]!=e->logn+1 || h[15]!=1 ||
       h[16]!=2400000000u || h[17]!=1200000000u || h[18]!=(150000000u>>e->zoom) ||
       h[19]!=1200000000u || sa_join(h[20],h[21])!=1800000000ull || sa_join(h[22],h[23])!=e->raw_nco ||
       sa_join(h[30],h[31])!=e->counter_epoch || h[32] || h[37]!=1 ||
       (h[36]&0x001fffffu)!=((e->zoom<<18)|(e->logn<<13)|15u) || (h[36]&0x80000000u)!=(e->control&0x80000000u) ||
       (h[36]&0x7fe00000u) || h[38] || h[39] ||
       sa_join(h[40],h[41])!=sa_sum_w[e->logn-7] || sa_join(h[42],h[43])!=sa_sum_w2[e->logn-7] ||
       h[44]!=17 || h[45]!=8 || h[46]!=24 || h[47]!=16 || h[48]!=1 || h[49]!=0 ||
       memcmp((const void *)(h+56),sa_window_sha,32))return 0;
    for(unsigned k=50;k<56;k++)if(h[k])return 0;
    uint64_t first=sa_join(h[26],h[27]),last=sa_join(h[28],h[29]);
    if(last<first || ((last-first)&3))return 0;
    for(unsigned k=0;k<n;k++){if(p[k]>(1ull<<47))return 0;checksum^=p[k];}
    return checksum==sa_join(h[34],h[35]);
}
#endif
