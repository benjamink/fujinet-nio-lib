/* State and CS1 bus access shared by the Palm OS Springboard sources. */
#ifndef FN_PALMOS_INTERNAL_H
#define FN_PALMOS_INTERNAL_H

#include <stdint.h>
#include "sbnio.h"

/* Where fn_palmos_open() found the module; cs1 is 0 while none is open. */
extern uint32_t _fn_palmos_cs0;
extern uint32_t _fn_palmos_cs1;
extern uint16_t _fn_palmos_card;
extern uint8_t _fn_palmos_seq;   /* last doorbell sequence sent */

/* CS1 is plain 16-bit MOVE.W cycles. The real build keeps bare volatile
 * accesses; the host test swaps in a fake module behind the same macros. */
#ifdef FN_PALMOS_HOST_TEST
uint16_t fn_palmos_test_read(uint32_t address);
void fn_palmos_test_write(uint32_t address, uint16_t value);
typedef uint32_t fn_palmos_port_t;
#define FN_PALMOS_GET(port)     fn_palmos_test_read(((port) += 2) - 2)
#define FN_PALMOS_PUT(port, v)  fn_palmos_test_write((port), (v))
#define FN_PALMOS_RD(offset)    fn_palmos_test_read(_fn_palmos_cs1 + (offset))
#define FN_PALMOS_WR(reg, v)    fn_palmos_test_write(_fn_palmos_cs1 + SBN_W(reg), (v))
#else
typedef volatile uint16_t *fn_palmos_port_t;
#define FN_PALMOS_GET(port)     (*(port)++)
#define FN_PALMOS_PUT(port, v)  (*(port) = (v))
#define FN_PALMOS_RD(offset)    (*(volatile uint16_t *)(_fn_palmos_cs1 + (offset)))
#define FN_PALMOS_WR(reg, v)    (*(volatile uint16_t *)(_fn_palmos_cs1 + SBN_W(reg)) = (v))
#endif

#define FN_PALMOS_PORT(address) ((fn_palmos_port_t)(address))

#endif
