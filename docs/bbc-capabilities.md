# BBC public API capabilities

The BBC library is ROM-backed: MOS channels own network sessions, and OSWORD
&78 reason &06 supplies generic device calls. The Atari SIO transport and its
buffers are not part of this target. `bbc` and `bbc-clib` share this contract.

`tests/bbc_api_capabilities.json` is the checked inventory of every function
prototype in `fujinet-nio.h` and `fn_bbc.h`, including explicitly platform-specific functions.
`make test-bbc-api-link` links each entry independently against both archives.
The assembly reference cannot be optimized away. New declarations require an
explicit classification. This proves linkability, not runtime correctness.
Other public headers (raw/session/legacy/Atari stream) are outside this inventory.

## available

`fn_open_headers`, `fn_bbc_open_headers`, `fn_read_available`, `fn_ready`, `fn_file_list_pages`, `fn_appstore_delete`, `fn_appstore_list`, `fn_appstore_list_next_key`, `fn_appstore_read`, `fn_appstore_stat`, `fn_appstore_write`, `fn_clock_get`, `fn_clock_get_format`, `fn_clock_get_timezone`, `fn_clock_get_tz`, `fn_clock_set`, `fn_clock_set_timezone`, `fn_clock_set_timezone_save`, `fn_clock_sync_network_time`, `fn_close`, `fn_error_string`, `fn_format_mount_display`, `fn_fuji_get_hsio`, `fn_fuji_get_info`, `fn_fuji_set_hsio`, `fn_info`, `fn_init`, `fn_is_ready`, `fn_json_query`, `fn_open`, `fn_open_long`, `fn_open_translated`, `fn_read`, `fn_resolve_mount_target`, `fn_set_body_length`, `fn_set_content_profile`, `fn_slot_catalog_delete`, `fn_slot_catalog_get`, `fn_slot_catalog_next_entry`, `fn_slot_catalog_put`, `fn_slot_catalog_range`, `fn_tcp_open`, `fn_version`, `fn_wifi_get_adapter_info`, `fn_wifi_get_config`, `fn_wifi_get_join_status`, `fn_wifi_get_status`, `fn_wifi_scan`, `fn_wifi_set_config`, `fn_wifi_try_join`, `fn_write`.

## unsupported-stub

None.

## missing

`fn_disk_clear_changed`, `fn_disk_clear_changed_context`, `fn_disk_context_init`, `fn_disk_flush`, `fn_disk_flush_context`, `fn_disk_info`, `fn_disk_info_context`, `fn_disk_inspect_context`, `fn_disk_last_error`, `fn_disk_last_error_context`, `fn_disk_mount`, `fn_disk_mount_context`, `fn_disk_read_sector`, `fn_disk_read_sector_context`, `fn_disk_restore_boot`, `fn_disk_restore_boot_context`, `fn_disk_unmount`, `fn_disk_unmount_context`, `fn_disk_write_sector`, `fn_disk_write_sector_context`, `fn_shutdown`, `fn_write_get_last_diagnostics`.

## other-platform

`fn_amiga_transport_last_broker_cause`, `fn_amiga_transport_last_broker_detail`, `fn_amiga_transport_last_serial_detail`, `fn_atari_capability_flags`, `fn_atari_hsio_disable`, `fn_atari_hsio_enable`, `fn_atari_hsio_error_count`, `fn_atari_hsio_index`, `fn_atari_last_sio_status`, `fn_atari_max_payload`, `fn_atari_proc_count`, `fn_atari_proc_disable`, `fn_atari_proc_enable`, `fn_atari_proc_notify`, `fn_atari_proc_pending`, `fn_atari_query_capabilities`, `fn_atari_sio_call`, `fn_atari_u1mb_clear_device`, `fn_atari_u1mb_clear_devices`, `fn_atari_u1mb_clear_disks`, `fn_atari_u1mb_pbi`.

## Improvement sequence and measurement

1. Keep the inventory and independent archive-link gate current.
2. ListPages/filter through the existing device-call bridge; verify wire fields,
   filtered pages and malformed replies without adding resident ROM code.
3. Split Wi-Fi archive members; compare status-only and join-only CODE/BSS maps.
4. Bulk network reads: compare 32/120/512-byte cycle counts against OSBGET,
   including NotReady/EOF/partial progress and buffer guards.
5. Ready/read-available: map firmware handles to MOS-owned sessions and account
   for bytes already buffered in the ROM; measure idle bus requests.
6. HTTP headers: extend the ROM open contract with bounded caller-owned input;
   test malformed lengths, credentials, cleanup and old-ROM failure behavior.

Do not claim percentage savings until measured. Resident ROM space is limited;
new OSWORD reasons must retain old callers and fail safely on older ROMs.
Use soft65c02 for independent ABI/boundary/cycle vectors and Beebium for actual
MOS integration. General library changes also require complete `make check`.

## ListPages

BBC now uses the shared ListPages codec through the ROM's existing OSWORD &78
reason &06. The caller supplies the request/reply buffer; the adapter adds no
persistent packet buffer and requires no resident ROM changes. Filtering,
directory inclusion and filtered totals remain firmware responsibilities.
The BBC Beebium test verifies two filtered pages, exact request bytes and a
short malformed reply. Shared independent host wire vectors also run through
the BBC raw-call adapter.

## Wi-Fi archive granularity

Measured with the independent single-symbol BBC link probes (same cc65, includes
transitive runtime helpers; these are not whole application sizes):

| Probe | CODE before | CODE after | BSS before/after |
|---|---:|---:|---:|
| `fn_wifi_get_status` | 5421 | 2514 | 19 / 19 |
| `fn_wifi_try_join` | 5421 | 1969 | 19 / 19 |

Each public Wi-Fi operation is now a separate archive member. Shared host wire
vectors run against the BBC implementation as well as the general implementation.
This also caught and fixed configuration writes passing a null reply-length
pointer to the ROM bridge, which requires a valid output pointer.

## Bulk read (OSWORD &78 reason &07)

New ROMs copy the current buffered chunk directly into caller memory. The
library loops for larger reads, preserving the no-probe/NotReady boundary;
The current ROM (reasons &07–&09) is required; there is no older-ROM fallback. Count and flags are initialized
so NotReady cannot leave stale output values. The library adds an 11-byte
control block; the ROM adds 212 bytes and no persistent workspace (BBC ROM
free space 958 -> 746 bytes at this stage).

soft65c02 cycle measurements use preloaded buffers, excluding network time:

| Bytes | Bulk | Direct BGET byte loop |
|---:|---:|---:|
| 1 | 1145 | 650 |
| 32 | 1796 | 20335 |
| 120 | 3644 | 76215 |
| 255 | 6477 | 161940 |

The baseline directly calls the ROM BGET entry, omitting MOS and library loop
costs. A 512-byte three-chunk transfer with two stub refills took 14722 cycles;
this is not a measured network throughput figure. Beebium TCP/HTTP tests verify the current ROM path.

## Readiness

BBC Ready combines one firmware Ready request with local OSWORD &78 reason &08
metadata for active library sessions. It translates firmware handles to MOS
handles and adds ROM-buffered bytes, saturating at 65535. Invalid/malformed
replies fail; no separate ROM compatibility probe is performed. Other ROM/BASIC sessions are not
exposed as library sessions. Local channel queries cause no bus traffic.

`fn_read_available()` returns the remaining ROM-buffered lower bound from the
last bulk read, with FN_READ_AVAILABLE set when nonzero. It performs no bus
request. This does not promise
that the firmware/network has no additional bytes when the hint is zero.

## HTTP request headers

`fn_open_headers()` now supports BBC GET/POST/PUT requests (and method 0).
It reuses the existing 128-byte open-name workspace, with no new persistent
buffer. The complete encoded request must fit: 13 bytes of framing, URL bytes,
and 4 bytes plus name/value bytes per header. Oversize input returns
`FN_ERR_INVALID` before any network request. Other methods remain unsupported.

For larger requests, include `fn_bbc.h` and use `fn_bbc_open_headers()` with a
caller-owned buffer of up to 512 bytes. This is a synchronous, non-reentrant
ROM open: keep the buffer and input strings alive until return, and do not
let them overlap. Storage can be reused immediately afterwards. For example:

```c
static uint8_t workspace[256];
static const fn_http_header_t headers[] = {{"X-Client", "BBC"}};
fn_handle_t handle;
uint8_t result;
fn_bbc_http_open_t request;
/* Call fn_init() first. */
request.handle = &handle;
request.url = "https://example.com/status";
request.headers = headers;
request.count = 1;
request.buffer = workspace;
request.capacity = sizeof(workspace);
request.body_len = 0;
request.method = FN_METHOD_GET;
request.flags = 0;
/* Check the return value before using handle. */
result = fn_bbc_open_headers(&request);
```

Requires ROM OSWORD &78 reason &09. Older ROMs return `FN_ERR_UNSUPPORTED`
without opening a channel. The ROM scatter-sends the caller's existing Network
Open v1 payload; firmware remains responsible for HTTP header semantics. The explicit payload overrides pending content-profile/body-length hints. There
is no new wire protocol or firmware requirement. One-shot state is cancelled
on failed opens too. Host tests cover exact bytes, bounds, old-ROM rejection
and cleanup; Beebium verifies two header opens followed by an ordinary open.

## Final ROM budget

After bulk read, channel metadata and header-open support, `make sizes` reports
15926/16384 bytes used on BBC (458 free), and 15979/16384 on Master (405 free).
This is 500 bytes of resident ROM growth from the starting build. These APIs
reuse existing ROM workspace; the library bulk-read control block is 11 bytes.
Ready uses bounded temporary stack storage (85-byte reply plus 16-byte local
control block), rather than a persistent buffer. Header construction uses
caller storage or the existing short-open buffer. Further resident additions
must account for the small remaining ROM margin.

## Current-ROM requirement

The old byte-at-a-time read fallback and Ready compatibility preflight have
been removed. Independent BBC archive-link probes shrink from 740 to 661 CODE
bytes for `fn_read`, and 2284 to 2167 for `fn_ready`; no BSS change. Ready now
queries local metadata once per returned session rather than probing twice.
These figures are separate probes, not additive whole-application savings.
Deploy the library and current fn-rom together. Error handling remains, but
there is no alternate implementation for older ROMs.
