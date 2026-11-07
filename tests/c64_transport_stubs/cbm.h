/* Host stand-in for cc65's <cbm.h>: only the KERNAL bus calls the C64 transport
 * uses. The test records each call and plays the device's side. */
#ifndef C64_TRANSPORT_STUB_CBM_H
#define C64_TRANSPORT_STUB_CBM_H

#include <stdint.h>

extern uint8_t g_c64_st;
#define C64_ST g_c64_st

void cbm_k_listen(uint8_t device);
void cbm_k_second(uint8_t secondary);
void cbm_k_ciout(uint8_t value);
void cbm_k_unlsn(void);
void cbm_k_talk(uint8_t device);
void cbm_k_tksa(uint8_t secondary);
uint8_t cbm_k_acptr(void);
void cbm_k_untlk(void);

#endif
