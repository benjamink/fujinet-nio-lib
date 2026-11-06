#include "image.h"

static unsigned long get32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) |
           ((unsigned long)p[2] << 8) | p[3];
}

static void put32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8); p[3] = (unsigned char)v;
}

int nl_image_validate(const unsigned char *p, unsigned long length,
                      unsigned long code_size)
{
    unsigned long data, total, count, i, off, kind, target, previous = 0;
    if (!p || length < NL_IMAGE_HEADER || get32(p) != 0x4e4c4731UL) return 0;
    data = get32(p + 4); total = get32(p + 8); count = get32(p + 12);
    if (!data || data > total || total > NL_IMAGE_LIMIT || data > length - 16) return 0;
    if ((length - 16 - data) % 8 || count != (length - 16 - data) / 8) return 0;
    for (i = 0; i < count; ++i) {
        off = get32(p + 16 + data + i * 8);
        kind = get32(p + 20 + data + i * 8);
        if ((off & 1) || data < 4 || off > data - 4 ||
            (i && off < previous + 4)) return 0;
        previous = off;
        target = get32(p + 16 + off);
        if (kind == 1) { if (target >= total) return 0; }
        else if (kind == 2) { if ((target & 1) || target >= code_size) return 0; }
        else return 0;
    }
    return 1;
}

void nl_image_copy(const unsigned char *p, unsigned char *dest,
                    unsigned long data_base, unsigned long code_base)
{
    unsigned long data = get32(p + 4), total = get32(p + 8);
    unsigned long count = get32(p + 12), i, off, kind;
    for (i = 0; i < total; ++i) dest[i] = i < data ? p[16 + i] : 0;
    for (i = 0; i < count; ++i) {
        off = get32(p + 16 + data + i * 8);
        kind = get32(p + 20 + data + i * 8);
        put32(dest + off, get32(dest + off) + (kind == 1 ? data_base : code_base));
    }
}
