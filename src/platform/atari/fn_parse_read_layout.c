#include <stddef.h>
#include "fn_internal.h"
/* Keep the private Atari assembly parser's offsets tied to the cc65 ABI. */
#define ASSERT_FIELD(name, offset) \
    typedef char read_layout_##name[(offsetof(fn_parse_ctx_t, name) == offset) ? 1 : -1]
ASSERT_FIELD(response, 0);
ASSERT_FIELD(data, 2);
ASSERT_FIELD(data_max, 6);
ASSERT_FIELD(data_offset, 8);
ASSERT_FIELD(data_len, 10);
ASSERT_FIELD(handle, 12);
ASSERT_FIELD(offset_echo, 14);
ASSERT_FIELD(status, 24);
ASSERT_FIELD(flags, 25);
ASSERT_FIELD(available, 27);
