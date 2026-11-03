# Third-party code

| Where | What | Licence |
|---|---|---|
| `src/platform/atari/fn_hsio.s` | Derived from the high-speed SIO routine of Matthias Reichl (hipatch 1.30 / MyPicoDOS HISIO), Copyright (c) 2003-2010 Matthias Reichl | GPL version 2 or later |

This repository is licensed under the GNU General Public License version 3 (`LICENSE`), which the
"or later" of the original permits.

The optional Atari small-stream polled SIO driver (`src/platform/atari/fn_stream_sio.s`)
is derived from Michael Sternberg's SYN-ACK WIRE Atari `net_sio.s`, commit
`3b6fa091`, under GPL-3.0. It is isolated from the existing OS-SIO transport.
