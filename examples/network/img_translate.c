/* examples/network/img_translate.c
 * usage: img_translate <url> <selector> > out.iff
 * e.g.   img_translate https://example.com/a.png w=640,h=400,colors=16 > a.iff
 */
#define _DEFAULT_SOURCE /* usleep() under -std=c99 */
#include <stdio.h>
#include <unistd.h>
#include "fujinet-nio.h"

int main(int argc, char **argv)
{
    fn_handle_t h;
    uint8_t buf[512];
    uint16_t n;
    uint16_t status = 0;
    uint32_t off = 0;
    uint32_t len = 0;
    uint8_t fl;
    uint8_t err;

    if (argc < 3) {
        fprintf(stderr, "usage: %s url selector\n", argv[0]);
        return 2;
    }

    err = fn_init();
    if (err != FN_OK) {
        fprintf(stderr, "init: %s\n", fn_error_string(err));
        return 1;
    }

    err = fn_open_translated(&h, FN_METHOD_GET, argv[1], FN_OPEN_FOLLOW_REDIR,
                             FN_TRANSLATE_IMAGE, 0, argv[2]);
    if (err != FN_OK) {
        fprintf(stderr, "open: %s\n", fn_error_string(err));
        return 1;
    }

    for (;;) {
        n = 0;
        fl = 0;
        err = fn_read(h, off, buf, sizeof buf, &n, &fl);
        if (err == FN_ERR_NOT_READY || err == FN_ERR_BUSY) {
            usleep(20000);
            continue;
        }
        if (err != FN_OK) {
            fprintf(stderr, "read: %s after %lu bytes written\n",
                    fn_error_string(err), (unsigned long)off);
            if (err == FN_ERR_INVALID && off == 0) {
                /* The firmware could not decode the body: not an image, or
                   an error page such as a 404. */
                fprintf(stderr, "the response is not an image the firmware can read; check the URL\n");
            }
            break;
        }
        fwrite(buf, 1, n, stdout);
        off += n;
        if (fl & FN_READ_EOF) {
            break;
        }
        if (n == 0) {
            usleep(20000);
        }
    }

    if (err == FN_OK) {
        err = fn_info(h, &status, &len, &fl);
        if (err != FN_OK) {
            fprintf(stderr, "info: %s\n", fn_error_string(err));
        } else {
            fprintf(stderr, "status=%u bytes=%lu\n", status, (unsigned long)off);
        }
    }

    fn_close(h);
    fn_shutdown();
    return err != FN_OK;
}
