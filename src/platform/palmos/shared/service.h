#ifndef NL_SERVICE_H
#define NL_SERVICE_H
#define NIOLIB_IMPLEMENTATION
#include "NioLib.h"
#include <EntryPoints.h>
#include "private.h"
#include "fn_protocol.h"
Err nl_enter(UInt32 token);
void nl_leave(void);
/* Clear the last transport result before the operation. A failed typed decode
 * of a successful mutating reply must survive later queries and cleanup. */
void nl_note_mutation_failure(UInt16 result);
/* The outer guard is deliberately independent of A4 and works before Open. */
#define SERVICE(name, params, args) \
    static Err name##_a4 params SYSLIB_POINTER_ENTRYPOINT; \
    Err name params { \
        SysLibTblEntryPtr entry = SysLibTblEntry(refNum); \
        if (!entry || !entry->globalsP) return nioLibErrNotOpen; \
        return name##_a4 args; \
    } \
    static Err name##_a4 params

#endif
