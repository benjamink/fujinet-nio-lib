/*
 * fn_channel_serial.c - Palm OS cradle serial byte channel.
 *
 * This file owns the serial port only: the Serial Manager's SrmOpen on Palm OS
 * 3.3 and later, or the older serial library's SerOpen before that (3.1, 3.2).
 * Both default to 8N1 with RTS flow control. The common stream transport owns
 * SLIP framing, FujiBus request/response exchange and the reclaim after an
 * idle spell (FN_STREAM_WAKE_IDLE_MS).
 */

#include <PalmOS.h>
#include <SerialMgrOld.h>

#include "fujinet-nio.h"
#include "fn_channel.h"

#ifndef FN_PALM_BAUD
#define FN_PALM_BAUD 115200L
#endif

#ifndef FN_PALM_RX_BUFFER
#define FN_PALM_RX_BUFFER 1024
#endif

static UInt16 _port;
static uint8_t _open;
static uint8_t _old_serial;
static UInt8 _rx_buffer[FN_PALM_RX_BUFFER];

/* At least one tick, so a short timeout still waits. */
static Int32 ms_to_ticks(uint16_t timeout_ms)
{
    Int32 ticks = ((Int32)timeout_ms * SysTicksPerSecond() + 999) / 1000;
    return ticks > 0 ? ticks : 1;
}

static uint8_t open_old_serial(void)
{
    Err err = SysLibFind("Serial Library", &_port);

    if (err == errNone) {
        err = SerOpen(_port, 0, FN_PALM_BAUD);
    }
    if (err == serErrAlreadyOpen) {
        SerClose(_port); /* another app holds it: give our open back */
    }
    if (err != errNone) {
        return FN_ERR_NOT_FOUND;
    }
    SerSetReceiveBuffer(_port, _rx_buffer, sizeof(_rx_buffer));
    SerReceiveFlush(_port, 0);
    return FN_OK;
}

uint8_t fn_channel_init(void)
{
    UInt32 value;

    if (_open) {
        return FN_OK;
    }
    _old_serial = FtrGet(sysFileCSerialMgr, sysFtrNewSerialPresent, &value) != errNone ||
                  value == 0;
    if (_old_serial) {
        if (open_old_serial() != FN_OK) {
            return FN_ERR_NOT_FOUND;
        }
    } else {
        if (SrmOpen(serPortCradlePort, FN_PALM_BAUD, &_port) != errNone) {
            return FN_ERR_NOT_FOUND;
        }
        SrmSetReceiveBuffer(_port, _rx_buffer, sizeof(_rx_buffer));
        SrmReceiveFlush(_port, 0);
    }
    _open = 1;
    return FN_OK;
}

uint8_t fn_channel_ready(void)
{
    return _open;
}

uint8_t fn_channel_write_byte(uint8_t value, uint16_t timeout_ms)
{
    Err err;

    (void)timeout_ms;
    if (!_open) {
        return FN_ERR_NOT_READY;
    }
    if (_old_serial) {
        if (SerSend(_port, &value, 1, &err) != 1 || err != errNone) {
            SerClearErr(_port);
            return FN_ERR_IO;
        }
    } else if (SrmSend(_port, &value, 1, &err) != 1 || err != errNone) {
        SrmClearErr(_port);
        return FN_ERR_IO;
    }
    return FN_OK;
}

uint8_t fn_channel_read_byte(uint8_t *value, uint16_t timeout_ms)
{
    Err err;

    if (!_open || value == NULL) {
        return 0;
    }
    if (_old_serial) {
        if (SerReceive(_port, value, 1, ms_to_ticks(timeout_ms), &err) == 1) {
            return 1;
        }
    } else if (SrmReceive(_port, value, 1, ms_to_ticks(timeout_ms), &err) == 1) {
        return 1;
    }
    /* Line errors (framing, overrun) latch until cleared. */
    if (err != errNone && err != serErrTimeOut) {
        if (_old_serial) {
            SerClearErr(_port);
        } else {
            SrmClearErr(_port);
        }
    }
    return 0;
}

void fn_channel_drain_rx(void)
{
    if (_open && _old_serial) {
        SerReceiveFlush(_port, 0);
    } else if (_open) {
        SrmReceiveFlush(_port, 0);
    }
}

void fn_channel_close(void)
{
    if (_open && _old_serial) {
        SerSendWait(_port, -1);
        SerSetReceiveBuffer(_port, NULL, 0);
        SerClose(_port);
        _open = 0;
    } else if (_open) {
        SrmSendWait(_port);
        SrmSetReceiveBuffer(_port, NULL, 0);
        SrmClose(_port);
        _open = 0;
    }
}

/* Wraps at 2^32 ms like the tick count; callers compare differences. */
uint32_t fn_channel_now_ms(void)
{
    UInt32 ticks = TimGetTicks();
    UInt16 rate = SysTicksPerSecond();

    return (uint32_t)((ticks / rate) * 1000UL + (ticks % rate) * 1000UL / rate);
}

const char *fn_platform_name(void)
{
    return "palmos";
}
