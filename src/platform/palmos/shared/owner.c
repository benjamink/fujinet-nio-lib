/* A4-owned foreground state and guards are placed near the code-resource
 * midpoint so both early trap entries and later backends can reach them.
 * No additional buffers or application A5 dependencies are introduced. */
#include "service.h"
#include "fn_internal.h"
#include "owner.h"
#include "sbnio.h"

UInt32 nl_owner_token, nl_owner_db;
UInt16 nl_owner_card, nl_busy, nl_uncertain_operation;

Boolean nl_client_idle(void) { return !nl_busy && !nl_owner_token; }
Err nl_can_close(void)
{
    if (!nl_client_idle()) return nioLibErrBusy;
    /* One bounded attempt per entry. Physical ownership and command ambiguity
     * are independent: neither backend clears its persistent pending marker. */
    if (nl_usb_owned()) nl_usb_close();
    if (nl_serial_owned()) nl_serial_close();
    return nl_usb_owned() || nl_serial_owned() ? nioLibErrBusy : errNone;
}

Err nl_enter(UInt32 token)
{
    UInt16 card;
    LocalID db;
    if (nl_busy) return nioLibErrBusy;
    nl_busy = 1;
    if (!token || token != nl_owner_token || SysCurAppDatabase(&card, &db) ||
        card != nl_owner_card || db != nl_owner_db) {
        nl_busy = 0;
        return nioLibErrOwner;
    }
    nl_deadline(35000);
    return errNone;
}

static void release_client(void)
{
    /* Common parsers use scratch pointers. Never retain an app buffer after
     * returning through a trap, even though the next common call replaces it. */
    _fn_transport_ctx.request = NULL; _fn_transport_ctx.response = NULL;
    _fn_parse_ctx.response = NULL; _fn_parse_ctx.data = NULL;
    nl_busy = 0;
}

void nl_leave(void)
{
    if (nl_ambiguous()) nl_uncertain_operation = 1;
    release_client();
}


Boolean nl_physical_owned(void)
{
    return nl_usb_owned() || nl_serial_owned();
}

void nl_note_mutation_failure(UInt16 result)
{
    /* A malformed success can hide an allocation or accepted file bytes. The
     * fresh transport result distinguishes it from an unsent local rejection;
     * an explicit remote error is not a malformed success acknowledgement. */
    if (result && (nl_ambiguous() || nl_transport_pending() ||
        (nl_last_result() == SBN_RES_OK && _fn_parse_ctx.status == FN_OK)))
        nl_uncertain_operation = 1;
}
