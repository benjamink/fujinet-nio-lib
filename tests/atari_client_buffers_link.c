#include "fn_atari_buffers.h"
#ifdef CLIENT_BUFFER_OWNED
FN_ATARI_DEFINE_PACKET_BUFFERS();
#endif
int main(void) { return fn_init(); }
