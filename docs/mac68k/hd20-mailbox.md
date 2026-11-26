# Classic Macintosh: the HD20 mailbox

This is what the `mac68k` library and the FujiNet Mac board's floppy-port controller agree
on. The library's side is `src/platform/mac68k/fn_transport.c`. The controller's side is the
board's RP2040 firmware, which is not in an NIO repository.

## The units

The board presents each of its disks to the Mac as a Directly Connected Disk (an HD20) on
the floppy port, which the ROM's `.Sony` driver (reference number -5) runs. Every unit is 16
blocks (8 KB) longer than its image. Those last 16 blocks are the mailbox, which the
controller serves itself rather than from the image.

`fn_transport_init` finds a mailbox by walking the drive queue. It looks only at `.Sony`
drives larger than 16 blocks that aren't a floppy's size (800, 1440, 1600 or 2880 blocks),
so it never seeks a SCSI disk or a floppy. For each, it reads block `size - 16` and takes the
first whose block starts with `FNIO`. So the controller keeps a valid header in mailbox block
0 from the moment the unit appears. A unit is never exactly a floppy's size, being its image
plus 16 blocks.

## Block 0: the header

All numbers are big-endian.

| Bytes | Field | Value |
|---|---|---|
| 0-3 | magic | `FNIO` |
| 4 | version | 1 |
| 5 | state | 0: a request not yet answered; 1: the response is ready |
| 6-7 | sequence | 16-bit sequence number |
| 8-11 | length | bytes of packet after the header |
| 12-15 | reserved | 0 |
| 16 on | packet | one raw FujiBus packet, with no SLIP framing |

The packet continues through blocks 1 to 15. The largest packet either way is 8176 bytes,
the mailbox's 8192 less the header.

## An exchange

1. The Mac takes the next sequence number, wrapping at 16 bits, and writes the header with
   state 0 and the request after it. That is one driver write of whole blocks from mailbox
   block 0, `(16 + length + 511) / 512` of them.
2. The controller takes the packet from those blocks and sends it to fujinet-nio over its
   packet link. When the response comes back, it puts it in the mailbox: the header with state
   1, the request's sequence number and the response's length, and the packet after it. Every
   block of the response is in place before block 0 shows state 1, because the Mac reads the
   rest only once it sees that.
3. The Mac reads block 0 until it shows state 1 with its own sequence number. Then it reads
   the other blocks the length needs, in one read.

Only state 1 with the request's sequence number answers it. State 0, or state 1 with
another number, means the Mac carries on waiting. The second case is a response to an
earlier request that timed out. At init the library takes its sequence number from the
header it finds, so its first request never matches a response left in the mailbox.

There is one request at a time.

### Errors

The mailbox has no error state. NIO's result comes back inside the FujiBus response, as on
every transport. A controller that can't deliver a request, or gets no response to it,
leaves the mailbox as it is, and the Mac times out.

On the Mac's side, a failed driver call, or a block 0 that no longer starts with `FNIO`,
ends the exchange with `FN_ERR_IO`. A response longer than the caller's buffer, or whose
length runs past the mailbox, ends it with `FN_ERR_INVALID`.

## Timing

- The Mac polls block 0 back to back: each poll is one HD20 block read, with no pause
  between them.
- It waits up to `FN_MAC_TIMEOUT_TICKS`, 2100 ticks or 35 seconds, as the Palm OS transports
  do, so a slow request through NIO, such as an HTTP fetch, can finish. Build with
  `-DFN_MAC_TIMEOUT_TICKS=<ticks>` to change it. A timeout returns `FN_ERR_TIMEOUT`.
- The request may still be running in NIO after a timeout. Its response, when it comes, has
  an old sequence number and is ignored.
- The controller has to post each response within that time. The contract sets no lower
  bound; a quicker answer just means fewer polls.

## What has been run

- `make mac68k` builds the library with Retro68 for the 68000. Its transport uses no
  instruction from a later CPU, and only inline Toolbox traps.
- `make test-mac68k-transport` runs `fn_transport.c` on the host, against a fake drive queue,
  HD20 driver and controller that follow this document
  (`tests/mac68k_transport_host_test.c`).
- No run on a real Mac or a board is recorded here yet.
