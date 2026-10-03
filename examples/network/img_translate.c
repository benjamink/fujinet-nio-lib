/* examples/network/img_translate.c — usage: img_translate <url> <selector> > out.iff */
#define _DEFAULT_SOURCE /* usleep() under -std=c99 */
#include <stdio.h>
#include <unistd.h>
#include "fujinet-nio.h"

int main(int argc, char **argv)
{
    fn_handle_t h; uint8_t buf[512]; uint16_t n, status = 0; uint32_t off = 0, len = 0; uint8_t fl, err;
    if (argc < 3) { fprintf(stderr, "usage: %s url selector\n", argv[0]); return 2; }
    if ((err = fn_init()) != FN_OK) { fprintf(stderr, "init: %s\n", fn_error_string(err)); return 1; }
    err = fn_open_translated(&h, FN_METHOD_GET, argv[1], FN_OPEN_FOLLOW_REDIR, FN_TRANSLATE_IMAGE, 0, argv[2]);
    if (err != FN_OK) { fprintf(stderr, "open: %s\n", fn_error_string(err)); return 1; }
    for (;;) {
        n = 0; fl = 0;
        err = fn_read(h, off, buf, sizeof buf, &n, &fl);
        if (err == FN_ERR_NOT_READY || err == FN_ERR_BUSY) { usleep(20000); continue; }
        if (err != FN_OK) { fprintf(stderr, "read: %s\n", fn_error_string(err)); break; }
        fwrite(buf, 1, n, stdout); off += n;
        if (fl & FN_READ_EOF) break;
        if (!n) usleep(20000);
    }
    fn_info(h, &status, &len, &fl);
    fprintf(stderr, "status=%u bytes=%lu\n", status, (unsigned long)off);
    fn_close(h); fn_shutdown();
    return err != FN_OK;
}
