#include <stdio.h>
#include <stdlib.h>
#include "sa_packet_validate.h"
int main(int argc,char **argv)
{
    const unsigned tickets[6]={1,2,3,4,5,20},logs[6]={7,10,16,7,7,7};
    if(argc!=7)return 2;
    unsigned negatives=0;
    for(unsigned i=0;i<6;i++){
        FILE *f=fopen(argv[i+1],"rb");if(!f)return 3;
        fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);
        unsigned char *b=malloc((size_t)size);if(!b)return 4;
        if(fread(b,1,(size_t)size,f)!=(size_t)size)return 5;
        fclose(f);
        struct sa_packet_expect e={23,tickets[i],2,1,logs[i],0,logs[i]<<8|1,0x400000000000ull,0x123456789ull};
        if(!sa_packet_valid(b,(size_t)size,&e))return 6;
        e.ticket++;if(sa_packet_valid(b,(size_t)size,&e))return 7;e.ticket--;negatives++;
        e.epoch++;if(sa_packet_valid(b,(size_t)size,&e))return 8;e.epoch--;negatives++;
        if(sa_packet_valid(b,(size_t)size-8,&e))return 9;
        negatives++;
        const unsigned offsets[]={0,4,8,12,16,28,32,36,40,44,48,52,56,60,64,68,72,76,80,84,88,92,120,124,128,136,140,144,148,152,156,160,164,168,172,176,180,184,188,192,196,200,204,208,212,216,220,224,252,256};
        for(unsigned j=0;j<sizeof(offsets)/sizeof(offsets[0]);j++){
            b[offsets[j]]^=1;
            if(sa_packet_valid(b,(size_t)size,&e)){fprintf(stderr,"corrupt byte %u accepted\n",offsets[j]);return 10;}
            b[offsets[j]]^=1;negatives++;
        }
        free(b);
    }
    printf("SA_FIRMWARE_PACKET_C_PASS packets=6 rejected=%u; offline RTL fixtures only\n",negatives);return 0;
}
