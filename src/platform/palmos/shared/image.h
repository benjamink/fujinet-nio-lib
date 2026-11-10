/* nDat/0: big-endian NLG1, data length, total allocation, fixup count;
 * then initialized bytes and (offset, kind) UInt32 pairs. kind 1=data, 2=code.
 * All offsets are link-time offsets, never persisted runtime pointers. */
#ifndef NIO_GLOBALS_IMAGE_H
#define NIO_GLOBALS_IMAGE_H
#define NL_IMAGE_HEADER 16UL
#define NL_IMAGE_LIMIT 32767UL
#define NL_IMAGE_TYPE 'nDat'
#define NL_IMAGE_ID 0
/* No globals, allocation, traps or libc: also compiled by native tests. */
int nl_image_validate(const unsigned char *image, unsigned long length,
                      unsigned long code_size);
void nl_image_copy(const unsigned char *image, unsigned char *dest,
                    unsigned long data_base, unsigned long code_base);
#endif
