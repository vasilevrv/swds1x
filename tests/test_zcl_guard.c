#include "host_sdk.h"
#include <stdlib.h>
#include "guard_sources.inc"
int main(void) {
    u8 enroll[]={1,0,0,0,1};
    assert(sensor_zclPacketValid(0x500,enroll,sizeof(enroll)));
    for (u16 n=0;n<sizeof(enroll);++n) assert(!sensor_zclPacketValid(0x500,enroll,n));
    enroll[0]=9; assert(!sensor_zclPacketValid(0x500,enroll,sizeof(enroll)));
    u8 cie[14]={0,0,2,0x10,0,0xf0};
    assert(sensor_zclPacketValid(0x500,cie,sizeof(cie)));
    assert(!sensor_zclPacketValid(0x500,cie,sizeof(cie)-1));
    u8 str[]={0,0,2,4,0,0x42,100};
    assert(!sensor_zclPacketValid(0,str,sizeof(str)));
    u8 wait[14]={9,0,5,0x97};
    assert(sensor_zclPacketValid(0x19,wait,sizeof(wait)));
    assert(!sensor_zclPacketValid(0x19,wait,sizeof(wait)-1));
    u8 block[19]={9,0,5,0}; block[16]=2;
    assert(sensor_zclPacketValid(0x19,block,sizeof(block)));
    assert(!sensor_zclPacketValid(0x19,block,sizeof(block)-1));
    u32 random=1;
    const u16 clusters[]={0,3,0x500,0x19};
    for (u32 i=0;i<100000;i++) {
        random=random*1664525U+1013904223U;
        u16 size=random%81;
        u8 *packet=malloc(size ? size : 1); assert(packet);
        for (u16 n=0;n<size;n++) { random=random*1664525U+1013904223U; packet[n]=random>>24; }
        (void)sensor_zclPacketValid(clusters[i%4],packet,size);
        free(packet);
    }
    puts("PASS: IAS/foundation/OTA lengths and 100000 fuzz packets");
}
