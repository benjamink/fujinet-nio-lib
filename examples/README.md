# FujiNet-NIO Library Examples

This folder contains example applications demonstrating how to use the fujinet-nio-lib library.

## Build Structure

The build system organizes artifacts into separate directories:

```
examples/
  build/                    # Intermediate build files
    linux/                   # Linux target
      network/               # Network examples
        http_get.o
        tcp_get.o
        tcp_stream.o
    atari/                   # Atari target
      network/
        http_get.o
        tcp_get.o
        tcp_stream.o
  bin/                       # Final executables
    linux/
      http_get
      tcp_get
      tcp_stream
    atari/
      http_get.xex
      tcp_get.xex
      tcp_stream.xex
  disk-images/
    bbc/
      network.ssd
      disk.ssd            # when BBC disk examples are added
```

## Building Examples

### Build all examples for a specific target:
```bash
make TARGET=linux    # Build for Linux (native testing)
make TARGET=atari    # Build for Atari 8-bit
make TARGET=bbc      # Build BBC-supported examples
```

For `TARGET=bbc`, `make` now builds both the linked binaries and grouped disk images.

### Build a specific example:
```bash
make http_get TARGET=linux
make tcp_get TARGET=linux
make tcp_stream TARGET=linux
```

### Build BBC disk images:
```bash
make disk TARGET=bbc
make TARGET=bbc http_get && make TARGET=bbc disk
```

From the project root:

```bash
make disk-bbc
make disk-bbc-clib
make FN_DEFAULT_TEST_URL="http://192.168.1.101:8080/get" \
     FN_TCP_HOST="192.168.1.101" \
     FN_TCP_PORT="7777" \
     FN_TCP_TLS=0 \
     disk-bbc
make FN_DEFAULT_TEST_URL="http://192.168.1.101:8080/get" \
     FN_TCP_HOST="192.168.1.101" \
     FN_TCP_PORT="7777" \
     FN_TCP_TLS=0 \
     disk-bbc-clib
```

The BBC disk output is grouped by example category. At present, the BBC build emits
`disk-images/bbc/network.ssd` containing the supported BBC network examples.
The BBC CLIB build emits the equivalent grouped image under:
`disk-images/bbc-clib/network.ssd`.

### Build Atari examples with custom configuration:
```bash
# Build with TLS enabled and custom host/port
make TARGET=atari FN_TCP_HOST=example.com FN_TCP_PORT=443 FN_TCP_TLS=1
```

### Build BBC examples with custom configuration:
```bash
make TARGET=bbc FN_DEFAULT_TEST_URL="http://127.0.0.1:8080/get"
make TARGET=bbc FN_TCP_HOST=127.0.0.1 FN_TCP_PORT=7777 tcp_get
```

Changing these values now forces the relevant BBC example objects to rebuild, so
the generated binaries and `network.ssd` pick up the new settings.

### Debug the build configuration:
```bash
make info TARGET=linux
```

### Clean build artifacts:
```bash
make clean           # Clean current target only
make clean-all       # Clean all targets
```

## Running Examples

### Linux Examples

Set the `FN_PORT` environment variable to specify the serial device:

```bash
# For ESP32 via USB-serial
export FN_PORT=/dev/ttyUSB0

# For POSIX fujinet-nio via PTY
export FN_PORT=/dev/pts/2

# Run HTTP example
./bin/linux/http_get

# Run TCP example (connects to localhost:7777 by default)
./bin/linux/tcp_get

# Run TLS example with custom host
FN_TCP_HOST=127.0.0.1 FN_TCP_PORT=7778 FN_TCP_TLS=1 ./bin/linux/tcp_get

# Run with full URL override
FN_TEST_URL="tls://echo.fujinet.online:6001" ./bin/linux/tcp_get

# Run TCP streaming example
./bin/linux/tcp_stream

```

### Atari Examples

Copy the `.xex` files to your Atari storage medium (SD card, ATR disk, etc.) and run them from the Atari DOS menu.

**Note:** Atari examples use compile-time configuration for TCP settings. To change the target server, rebuild with different parameters:

```bash
# Build for a specific server
make TARGET=atari FN_TCP_HOST=192.168.1.100 FN_TCP_PORT=7777

# Build with TLS enabled
make TARGET=atari FN_TCP_HOST=example.com FN_TCP_PORT=443 FN_TCP_TLS=1
```

Clock examples now live in the `nio-apps` repository so they can be shared across application targets.

### BBC Examples

BBC examples assume the `fn-rom` network ROM is installed in the target machine or emulator.

Currently supported BBC example set:

- `http_get`
- `tcp_get`

Current BBC notes:

- `tcp_stream` is not yet enabled for BBC
- the example link path currently injects a tiny `initenv` shim object as a cc65 BBC runtime workaround
- BBC example binaries are linked at `$1900` so they can be safely `*RUN` from disk images without colliding with DFS workspace
- BBC disk images are built with the bundled `scripts/create_ssd.py` and per-file `.inf` metadata
- BBC disk images are grouped by example category, so the current BBC output is a single `network.ssd` containing `HTTPGET` and `TCPGET`

## Example Categories

### Network Examples (`network/`)

- **http_get** - Simple HTTP GET request demonstrating:
  - Library initialization
  - Opening an HTTP connection
  - Getting response info (status, content-length)
  - Reading data in chunks
  - Handling EOF
  - Closing the connection

- **tcp_get** - TCP/TLS client demonstrating:
  - TCP socket connections
  - TLS encryption support (via `tls://` URL scheme)
  - Sending data to server
  - Half-close (FIN) for echo servers
  - Idle timeout for response detection
  - Reading response data
  - Connection state monitoring

- **tcp_stream** - TCP streaming example demonstrating:
  - Non-blocking reads for real-time applications
  - Frame-based data reception
  - Pattern suitable for games and interactive applications
  - Statistics tracking (frames received, bytes, timing)

- **img_translate** (Linux only) - Image translation via `fn_open_translated()`:
  - `./bin/linux/img_translate <url> <selector> > out.iff`
  - Selector example: `fmt=ilbm,w=640,h=400,colors=16`
  - Writes the translated bytes to stdout

### Disk Examples (`disk/`) - Planned

Examples for disk device operations.

## Configuration

### Environment Variables (All Platforms)

All examples use `getenv()` to read configuration. On Linux, these come from the shell environment. On cc65 targets (Atari, BBC, Apple, etc.), the library uses `putenv()` to populate environment variables from compile-time defines at startup.

#### HTTP Get Example
- `FN_TEST_URL` - URL to fetch (default: `http://localhost:8080/get`)

#### TCP Get Example
- `FN_TEST_URL` - Full URL (e.g., `tcp://host:port` or `tls://host:port`)
- `FN_TCP_HOST` - Host to connect to (default: `localhost`)
- `FN_TCP_PORT` - Port to connect to (default: `7777`)
- `FN_TCP_TLS` - Set to `1` to enable TLS (default: disabled)
- `FN_TCP_REQUEST` - Custom request string to send

#### TCP Stream Example
- `FN_TEST_URL` - Full URL (e.g., `tcp://host:port`)
- `FN_TCP_HOST` - Host to connect to (default: `localhost`)
- `FN_TCP_PORT` - Port to connect to (default: `7777`)

#### Common
- `FN_PORT` - Serial port device (default: `/dev/ttyUSB0`)

### Compile-Time Defines (cc65 Targets)

For cc65 targets, environment variables are populated from compile-time defines. This allows the same code to work on both Linux and 8-bit platforms, including BBC:

```bash
make TARGET=atari FN_TCP_HOST=192.168.1.100 FN_TCP_PORT=7778 FN_TCP_TLS=1
```

| Variable | Default | Description |
|----------|---------|-------------|
| `FN_TCP_HOST` | `localhost` | Target host |
| `FN_TCP_PORT` | `7777` | Target port |
| `FN_TCP_TLS` | `0` | Enable TLS (0=disabled, 1=enabled) |
| `FN_TCP_REQUEST` | `Hello from FujiNet-NIO!\r\n` | Request string to send |
| `FN_DEFAULT_TEST_URL` | `http://localhost:8080/get` | Default HTTP URL |

### How It Works

The examples use a `setup_env()` function (guarded by `#ifdef __CC65__`) that calls `putenv()` to populate the environment from compile-time defines. This allows the rest of the code to use `getenv()` uniformly:

```c
#ifdef __CC65__
static char env_fn_tcp_host[] = "FN_TCP_HOST=" FN_TCP_HOST;

static void setup_env(void)
{
    putenv(env_fn_tcp_host);
}
#endif

int main(void)
{
#ifdef __CC65__
    setup_env();
#endif
    
    // Now getenv() works the same on all platforms
    const char *host = getenv("FN_TCP_HOST");
    // ...
}
```

## Adding New Examples

1. Create a new `.c` file in the appropriate category folder (e.g., `network/my_example.c`)
2. Add the example name to the appropriate list in the `Makefile`:
   ```makefile
   NETWORK_EXAMPLES := http_get tcp_get tcp_stream my_example
   ```
3. Build with `make my_example TARGET=<target>`

### Adding a New Category

1. Create a new folder (e.g., `disk/`)
2. Add your example files
3. Add a new example list in the Makefile:
    ```makefile
    DISK_EXAMPLES := disk_info
    ```
4. Add to `ALL_EXAMPLES`:
    ```makefile
    ALL_EXAMPLES := $(NETWORK_EXAMPLES) $(CLOCK_EXAMPLES) $(DISK_EXAMPLES)
    ```
5. Add a build directory rule and pattern rule for the new category

## Testing with Local Servers

For testing the TCP/TLS examples locally, you can use the test services from the fujinet-nio project:

```bash
# Start TCP echo server on port 7777
./scripts/start_test_services.sh tcp

# Start TLS echo server on port 7778
./scripts/start_test_services.sh tls

# Start TCP streaming server on port 7779 (continuously sends frames)
./scripts/start_test_services.sh stream

# Start all services
./scripts/start_test_services.sh all

# Run the examples
FN_TCP_HOST=localhost FN_TCP_PORT=7777 ./bin/linux/tcp_get
FN_TCP_HOST=localhost FN_TCP_PORT=7778 FN_TCP_TLS=1 ./bin/linux/tcp_get
FN_TCP_HOST=localhost FN_TCP_PORT=7779 ./bin/linux/tcp_stream
```

### Streaming Server

The streaming server (`./scripts/start_test_services.sh stream`) continuously sends frames of data in the format:
```
FRAME 0: 12:34:56.789
FRAME 1: 12:34:56.889
FRAME 2: 12:34:56.989
...
```

This is useful for testing non-blocking read patterns in the `tcp_stream` example.

## Test Suite

The `suite.py` script runs all examples against both POSIX and ESP32 targets:

```bash
# Run against POSIX only
python suite.py --posix-port /dev/pts/2

# Run against ESP32 only  
python suite.py --esp32-port /dev/ttyUSB0

# Run against both with explicit services IP
python suite.py --posix-port /dev/pts/2 --esp32-port /dev/ttyUSB0 --services-ip 192.168.1.101
```

Prerequisites:
- Test services must be running: `./scripts/start_test_services.sh all`
- ESP32 must be flashed with fujinet-nio firmware
