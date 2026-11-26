# Linux framed serial writes

The Linux/POSIX channel enables FN_CHANNEL_BULK_WRITE and uses the stream
session's existing write_bytes callback. SLIP encoding remains in the shared
session, using its existing wire buffer; other targets retain their current
callbacks. There is no wire change or application-command retry.

This follows a Pico soak capture in which the host submitted sixteen separate
one-byte USB transfers containing only a Write frame's header, then timed out
without submitting its six-byte body or closing delimiter. The final header
transfer completed only when Close began one second later. That demonstrates
incomplete outgoing submission, but does not establish the reason for the
USB completion/readiness delay.

Linux now writes the whole encoded frame in one syscall when possible. Short
writes advance the offset; EAGAIN and EINTR preserve ordering. A monotonic
deadline bounds the entire frame, and readiness waits are at most 10 ms so the
writer rechecks progress even without a driver wake. Errors do not replay an
already accepted prefix. No additional persistent buffer is allocated.

Validation:
- make test-linux-channel covers actual transport callback selection and SLIP
  escaping in one write, partial writes, EAGAIN without a readiness wake,
  EINTR, total deadlines and hard errors.
- make check was attempted: primary configured targets and amiga-driver built,
  but extra target mac68k cannot build without m68k-apple-macos-gcc (Retro68).
  This is not an all-target pass.
- Hardware acceptance requires repeating the Pico soak with a rebuilt Linux
  client. No firmware update is needed.
- make test passed separately after the unavailable Mac cross-compiler stopped
  make check. Both Bouncy World Linux binaries were relinked; test-linux-network,
  test-linux-text and test-linux-gfx passed. The graphical binary contains the
  bulk callback and fn_channel_write_bytes.
