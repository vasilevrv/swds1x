#include "host_sdk.h"
#include <stdlib.h>
#define ZCL_MAX_SCENE_EXT_FIELD_SIZE 4
#include "guard_sources.inc"

int main(void) {
    u8 on[]={1,0,1};
    assert(relay2_zclPacketValid(6,on,sizeof(on)));
    u8 timed[]={1,0,0x42,0,1,0,2,0};
    assert(relay2_zclPacketValid(6,timed,sizeof(timed)));
    for (u16 n=0;n<sizeof(timed);n++) assert(!relay2_zclPacketValid(6,timed,n));
    u8 interval[]={0,0,2,0,0,0x10,1};
    assert(relay2_zclPacketValid(0xFC00,interval,sizeof(interval)));
    interval[0]=8; assert(!relay2_zclPacketValid(0xFC00,interval,sizeof(interval)));
    interval[0]=0; interval[6]=2; assert(!relay2_zclPacketValid(0xFC00,interval,sizeof(interval)));
    u8 startup[]={0,0,2,3,0x40,0x30,0xFE};
    assert(!relay2_zclPacketValid(6,startup,sizeof(startup)));
    startup[6]=0xFF; assert(relay2_zclPacketValid(6,startup,sizeof(startup)));
    u8 str[]={0,0,2,4,0,0x42,100};
    assert(!relay2_zclPacketValid(0,str,sizeof(str)));
    u8 report[]={0,0,6,0,0,0,0x21,1,0,10,0,1,0};
    assert(relay2_zclPacketValid(6,report,sizeof(report)));
    assert(!relay2_zclPacketValid(6,report,sizeof(report)-1));
    u8 scene[]={1,0,0,0,0,1,0,0,0,6,0,1,1};
    assert(relay2_zclPacketValid(5,scene,sizeof(scene)));
    assert(!relay2_zclPacketValid(5,scene,sizeof(scene)-1));
    u8 wait[14]={9,0,5,0x97};
    assert(relay2_zclPacketValid(0x19,wait,sizeof(wait)));
    assert(!relay2_zclPacketValid(0x19,wait,sizeof(wait)-1));
    u8 block[19]={9,0,5,0}; block[16]=2;
    assert(relay2_zclPacketValid(0x19,block,sizeof(block)));
    assert(!relay2_zclPacketValid(0x19,block,sizeof(block)-1));
    /* Exact heap bounds expose even single-byte overreads to ASan. */
    u32 random=1;
    const u16 clusters[]={0,3,4,5,6,0x19,0x702,0xB04,0xFC00};
    for (u32 i=0;i<100000;i++) {
        random=random*1664525U+1013904223U;
        u16 size=random%81;
        u8 *packet=malloc(size ? size : 1); assert(packet);
        for (u16 n=0;n<size;n++) { random=random*1664525U+1013904223U; packet[n]=random>>24; }
        (void)relay2_zclPacketValid(clusters[i%9],packet,size);
        free(packet);
    }
    puts("PASS: ZCL wire lengths, invalid startup/direction, strings, reporting, scenes, OTA, 100000 fuzz packets");
    return 0;
}
