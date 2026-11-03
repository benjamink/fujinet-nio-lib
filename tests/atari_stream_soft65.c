#include <stdint.h>
#include <string.h>
#include "fn_stream.h"
struct guarded { uint8_t before; fn_stream_io_t io; uint8_t after; } g;
extern uint8_t *test_buf;
#pragma zpsym ("test_buf")
extern uint8_t test_frame[4], test_len, test_mode;
uint8_t test_result[20];
static uint8_t mode, sends, reads;
uint8_t mock_sio(void) {
    uint8_t i;
    if(test_frame[0]!=0x7f || test_frame[3]) return 1;
    if(test_frame[1]=='s') {
        if(test_mode!=0x40 || test_len!=10 || test_frame[2]) return 1;
        memcpy(test_buf,"\1\170\x12\x34\x56\x78\0\0\0\0",10);
        if(mode==1) test_buf[0]=2;
        return 0;
    }
    if(test_frame[1]=='S') {
        ++sends;
        if(test_mode!=0x80 || test_len!=test_frame[2] || test_buf!=(uint8_t*)&g.io) return 1;
        if(mode==2) return 1;
        return 0;
    }
    if(test_frame[1]=='T') {
        ++reads;
        if(test_mode!=0x40 || test_len!=test_frame[2] || test_buf!=&g.io.status) return 1;
        memset(test_buf,0,test_len);
        memcpy(test_buf+3,g.io.token,8);
        if(mode==3) test_buf[3]^=1;
        if(g.io.operation==2) {
            test_buf[1]=4;test_buf[2]=g.io.length;
            for(i=0;i<g.io.length;++i) test_buf[11+i]='A'+(i&15);
        }
        if(g.io.operation==3) test_buf[2]=1;
        if(mode==4) { test_buf[0]=4;test_buf[2]=0; }
        return 0;
    }
    return 1;
}
void test_func(void) {
    uint8_t e;
    memset(test_result,0xff,sizeof(test_result));
    memset(&g,0xa5,sizeof(g)); mode=0;
    e=fn_stream_init(&g.io);
    test_result[0]=(e==0 && g.io.version==1 && g.io.token[0]==0x12 && g.io.token[3]==0x78 && g.io.sequence[0]==0)?0:1;
    g.io.length=120;
    e=fn_stream_read(&g.io);
    test_result[1]=(e==0 && g.io.actual==120 && g.io.flags==4 && g.io.received[119]=='H' && g.io.sequence[0]==1)?0:1;
    test_result[2]=(g.before==0xa5 && g.after==0xa5)?0:1;
    g.io.length=2;g.io.data[0]='x';g.io.data[1]='y';
    e=fn_stream_write(&g.io);
    test_result[3]=(e==0 && g.io.actual==1 && g.io.sequence[0]==2 && g.io.data[1]=='y')?0:1;
    mode=2;sends=0;
    e=fn_stream_write(&g.io);
    test_result[4]=(e==0x80 && sends==3 && g.io.sequence[0]==2 && g.io.data[0]=='x')?0:1;
    mode=3;sends=reads=0;
    e=fn_stream_write(&g.io);
    test_result[5]=(e==0x80 && sends==3 && reads==3 && g.io.sequence[0]==2)?0:1;
    mode=4;
    e=fn_stream_write(&g.io);
    test_result[6]=(e==4 && g.io.sequence[0]==3)?0:1;
    mode=0;g.io.length=121;sends=0;
    e=fn_stream_read(&g.io);
    test_result[7]=(e==0x81 && sends==0)?0:1;
    g.io.length=0;e=fn_stream_read(&g.io);
    test_result[8]=(e==0x81 && sends==0)?0:1;
    mode=1;e=fn_stream_init(&g.io);
    test_result[9]=(e==0x81)?0:1;
}
