# UDP/TCP Networking Suite - CS4375 Operating Systems Concepts

This repository contains a complete suite of UDP and TCP networking programs developed for CS4375 Fall 2025, Homework Assignment 2. The programs demonstrate socket programming in C using POSIX system calls, implementing various networking concepts including UDP communication, TCP tunneling, and Forward Error Correction (FEC).

## Table of Contents

- [Overview](#overview)
- [Programs](#programs)
- [Requirements](#requirements)
- [Compilation](#compilation)
- [Usage](#usage)
- [Testing](#testing)
- [Program Details](#program-details)
- [Network Configuration](#network-configuration)

---

## Overview

This project consists of seven networking programs that progressively build upon each other:

1. **send_udp** - Send data over UDP
2. **receive_udp** - Receive data over UDP
3. **reply_udp** - UDP echo server
4. **send_receive_udp** - Bidirectional UDP communication
5. **tunnel_udp_over_tcp_client** - Tunnel UDP packets over TCP (client side)
6. **tunnel_udp_over_tcp_server** - Tunnel UDP packets over TCP (server side)
7. **udp_fec** - Forward Error Correction for UDP


---

## Programs

### 1. send_udp
Reads data from standard input and sends it as UDP packets to a specified server.

**What to expect:**
- Reads stdin in 480-byte chunks
- Each chunk becomes one UDP packet
- Sends empty packet (size 0) on EOF
- No reliability guarantees - packets may be lost or reordered

### 2. receive_udp
Receives UDP packets on a specified port and writes them to standard output.

**What to expect:**
- Binds to specified port and listens for UDP packets
- Writes packet contents to stdout immediately
- Terminates on receiving empty packet
- May receive incomplete or reordered data

### 3. reply_udp
UDP echo server that sends received packets back to the sender.

**What to expect:**
- Receives packets and immediately echoes them back
- Preserves sender's address for reply
- Continues running indefinitely (even on empty packets)
- Useful for testing round-trip communication

### 4. send_receive_udp
Bidirectional UDP client using `select()` for multiplexing.

**What to expect:**
- Sends stdin data as UDP packets
- Receives UDP packets and writes to stdout
- Non-blocking operation using `select()`
- Terminates on stdin EOF or empty UDP packet reception

### 5. tunnel_udp_over_tcp_client
Tunnels UDP packets over a TCP connection (client side).

**What to expect:**
- Receives regular UDP packets on local port
- Wraps packets with 2-byte length header
- Sends over TCP connection with reliability
- Reconstructs UDP packets from TCP stream and forwards them

### 6. tunnel_udp_over_tcp_server
Tunnels UDP packets over a TCP connection (server side).

**What to expect:**
- Accepts TCP connection and connects to UDP server
- Receives TCP stream and reconstructs UDP packets
- Forwards packets via UDP to destination
- Provides bidirectional tunneling

### 7. udp_fec
Implements Forward Error Correction for UDP through packet repetition and reordering.

**What to expect:**
- Client mode: Receives regular UDP, sends FEC UDP with repetitions
- Server mode: Receives FEC UDP, removes duplicates, reorders, sends regular UDP
- Configurable repetition factor (0-7 additional transmissions)
- Configurable delays for repetition spacing and reordering
- Dramatically improves reliability over lossy networks

---

## Requirements

### Software Requirements
- Linux operating system (tested on Arch)
- GCC compiler with C99 support
- Standard C library and POSIX headers

### System Requirements
- Network interface (localhost for testing, network connection for real scenarios)
- Root access (for network simulation with `tc` - optional)
- Multiple terminal windows or network machines for testing

### Testing Environment
- **Localhost testing**: Works on any Linux machine
- **Network testing**: Requires access to UTEP network when using `dandelion` server
- **Firewall note**: Ports on `dandelion` only accessible from UTEP IP addresses

---

## Compilation

Compile all programs using GCC:

```bash
# Individual programs
gcc -Wall -Wextra -O2 -o send_udp send_udp.c
gcc -Wall -Wextra -O2 -o receive_udp receive_udp.c
gcc -Wall -Wextra -O2 -o reply_udp reply_udp.c
gcc -Wall -Wextra -O2 -o send_receive_udp send_receive_udp.c
gcc -Wall -Wextra -O2 -o tunnel_udp_over_tcp_client tunnel_udp_over_tcp_client.c
gcc -Wall -Wextra -O2 -o tunnel_udp_over_tcp_server tunnel_udp_over_tcp_server.c
gcc -Wall -Wextra -O2 -o udp_fec udp_fec.c

# Or compile all at once
make
```

**Compiler flags explained:**
- `-Wall -Wextra`: Enable all warnings
- `-O2`: Optimization level 2
- No additional libraries needed (uses standard C library)

---

## Usage

### send_udp

```bash
./send_udp <server_name> <port_name>
```

**Arguments:**
- `server_name`: Server hostname or IP address (e.g., "localhost", "192.168.1.100")
- `port_name`: Port number (0-65535)

**Example:**
```bash
echo "Hello, UDP!" | ./send_udp localhost 8000
cat large_file.bin | ./send_udp 192.168.1.100 9000
```

---

### receive_udp

```bash
./receive_udp <port>
```

**Arguments:**
- `port`: Local port to bind to (0-65535)

**Example:**
```bash
./receive_udp 8000 > received_data.txt
```

**Testing send_udp and receive_udp together:**
```bash
# Terminal 1
./receive_udp 8000 > output.txt

# Terminal 2
cat input.txt | ./send_udp localhost 8000

# Compare files (may differ due to packet loss/reordering)
diff input.txt output.txt
```

---

### reply_udp

```bash
./reply_udp <port>
```

**Arguments:**
- `port`: Local port to bind to (0-65535)

**Example:**
```bash
# Start echo server
./reply_udp 8000

# In another terminal, send data and receive echo
echo "test" | ./send_udp localhost 8000
```

---

### send_receive_udp

```bash
./send_receive_udp <server_name> <port_name>
```

**Arguments:**
- `server_name`: Server hostname or IP address
- `port_name`: Port number (0-65535)

**Example:**
```bash
# Terminal 1: Start echo server
./reply_udp 8000

# Terminal 2: Start receiver
./send_receive_udp localhost 8000
# Type messages, press Enter, see echoed responses
# Press Ctrl+D to send EOF and terminate
```

**File transmission example:**
```bash
# Terminal 1
./reply_udp 8000

# Terminal 2
cat document.txt | ./send_receive_udp localhost 8000 > echoed.txt
```

---

### tunnel_udp_over_tcp_client

```bash
./tunnel_udp_over_tcp_client <udp_port> <tcp_server_name> <tcp_port_name>
```

**Arguments:**
- `udp_port`: Local UDP port to bind to (0-65535)
- `tcp_server_name`: TCP server hostname or IP address
- `tcp_port_name`: TCP server port (0-65535)

**Example:**
```bash
./tunnel_udp_over_tcp_client 5000 localhost 6000
```

**What happens:**
- Binds UDP socket to port 5000
- Connects TCP socket to localhost:6000
- UDP packets received on 5000 → wrapped with header → sent over TCP
- TCP stream received → reconstructed to UDP packets → sent back to original sender

---

### tunnel_udp_over_tcp_server

```bash
./tunnel_udp_over_tcp_server <tcp_port> <udp_server_name> <udp_port_name>
```

**Arguments:**
- `tcp_port`: Local TCP port to listen on (0-65535)
- `udp_server_name`: UDP destination hostname or IP address
- `udp_port_name`: UDP destination port (0-65535)

**Example:**
```bash
./tunnel_udp_over_tcp_server 6000 localhost 7000
```

**What happens:**
- Listens for TCP connection on port 6000
- Connects UDP socket to localhost:7000
- TCP stream received → reconstructed to UDP packets → sent via UDP to localhost:7000
- UDP packets received from localhost:7000 → wrapped → sent over TCP

---

### Complete Tunnel Chain Example

This demonstrates UDP communication tunneled through TCP for reliability:

```bash
# Terminal 1: Final destination (echo server)
./reply_udp 7000

# Terminal 2: Tunnel server
./tunnel_udp_over_tcp_server 6000 localhost 7000

# Terminal 3: Tunnel client
./tunnel_udp_over_tcp_client 5000 localhost 6000

# Terminal 4: Send data through the tunnel
echo "This message travels: UDP → TCP → UDP → echo → reverse path" | \
  ./send_receive_udp localhost 5000
```

**Data flow:**
```
send_receive_udp (port 5000)
    ↓ UDP packets
tunnel_client (receives UDP, sends TCP)
    ↓ TCP stream
tunnel_server (receives TCP, sends UDP)
    ↓ UDP packets
reply_udp (port 7000, echoes back)
    ↑ (reverse path)
```

**Expected behavior:**
- 100% reliability (TCP ensures all packets arrive)
- Packets arrive in order (TCP guarantees ordering)
- Slightly higher latency than pure UDP
- No packet loss even over unreliable networks

---

### udp_fec

The most complex program with two operating modes: client and server.

#### Client Mode

```bash
./udp_fec -c <regular_udp_port> <fec_server_name> <fec_port_name> \
  <repetition_factor> <repetition_delay_ms> <reordering_delay_ms>
```

**Arguments:**
- `-c`: Client mode flag
- `regular_udp_port`: Local port to receive regular UDP packets (0-65535)
- `fec_server_name`: FEC server hostname or IP address
- `fec_port_name`: FEC server port (0-65535)
- `repetition_factor`: Number of additional transmissions (0-7)
- `repetition_delay_ms`: Delay between repetitions in milliseconds (0-1000)
- `reordering_delay_ms`: Maximum time to wait for reordering in milliseconds (0-8000)

**Example:**
```bash
./udp_fec -c 5000 localhost 6000 3 50 200
```
- Receives regular UDP on port 5000
- Sends FEC UDP to localhost:6000
- Sends each packet 4 times total (original + 3 repetitions)
- 50ms delay between repetitions
- Waits up to 200ms to reorder received packets

#### Server Mode

```bash
./udp_fec -s <fec_udp_port> <regular_server_name> <regular_port_name> \
  <repetition_factor> <repetition_delay_ms> <reordering_delay_ms>
```

**Arguments:**
- `-s`: Server mode flag
- `fec_udp_port`: Local port to receive FEC UDP packets (0-65535)
- `regular_server_name`: Regular UDP destination hostname or IP
- `regular_port_name`: Regular UDP destination port (0-65535)
- `repetition_factor`: Number of additional transmissions (0-7)
- `repetition_delay_ms`: Delay between repetitions in milliseconds (0-1000)
- `reordering_delay_ms`: Maximum time to wait for reordering in milliseconds (0-8000)

**Example:**
```bash
./udp_fec -s 6000 localhost 7000 3 50 200
```
- Receives FEC UDP on port 6000
- Sends regular UDP to localhost:7000
- Sends replies 4 times total (original + 3 repetitions)
- 50ms delay between repetitions
- Waits up to 200ms to reorder received packets

---

### Complete FEC Chain Example (4-Machine Scenario)

This demonstrates bidirectional UDP communication with Forward Error Correction:

```bash
# Terminal 1: Machine D - Final destination (echo server)
./reply_udp 7000

# Terminal 2: Machine C - FEC Server
./udp_fec -s 6000 localhost 7000 3 50 200

# Terminal 3: Machine B - FEC Client
./udp_fec -c 5000 localhost 6000 3 50 200

# Terminal 4: Machine A - Send data
echo "Protected message" | ./send_receive_udp localhost 5000
```

**Data flow diagram:**
```
Machine A: send_receive_udp (localhost:5000)
    ↓ Regular UDP packets
Machine B: udp_fec -c (receives on 5000)
    ↓ FEC UDP packets (with sequence numbers, sent 4x each)
    ↓ [Potentially unreliable network with packet loss]
Machine C: udp_fec -s (receives on 6000)
    ↓ Regular UDP packets (duplicates removed, reordered)
Machine D: reply_udp (port 7000)
    ↑ (Reverse path with FEC protection)
```

**What happens:**
1. Machine A sends regular UDP to Machine B
2. Machine B (client mode):
   - Adds 4-byte sequence number header
   - Sends packet immediately
   - Repeats transmission 3 more times with 50ms delays
3. Packets travel over network (may experience loss, reordering)
4. Machine C (server mode):
   - Receives FEC packets
   - Checks sequence numbers to eliminate duplicates
   - Queues packets and reorders by sequence number
   - Forwards in order to Machine D (or after 200ms timeout)
5. Machine D echoes back
6. Return path uses same FEC protection (C→B)
7. Machine A receives echoed data

**Expected behavior:**
- High reliability even with packet loss (e.g., 99.9% with rep=3 on 10% loss network)
- Packets delivered in sequence order (or with bounded delay)
- Increased bandwidth usage (4x with rep=3)
- Added latency from repetition delays (150ms with rep=3, delay=50ms)

---

### Parameter Selection Guidelines

#### Repetition Factor (0-7)
- **0**: No repetition (standard UDP, no FEC)
- **1**: Each packet sent 2x (doubles bandwidth, good for ~5% loss)
- **3**: Each packet sent 4x (4x bandwidth, excellent for ~10-15% loss)
- **7**: Each packet sent 8x (8x bandwidth, extreme protection for ~30%+ loss)

**Recommendation:** Start with 3 for good balance

#### Repetition Delay (0-1000ms)
- **10-50ms**: Fast repetition, good for low-latency requirements
- **50-100ms**: Balanced approach
- **100-500ms**: Slower, reduces network congestion

**Recommendation:** 50ms provides good spacing without excessive delay

#### Reordering Delay (0-8000ms)
- **100-200ms**: Tight reordering window, faster delivery
- **200-500ms**: Balanced, handles moderate reordering
- **500-2000ms**: Patient reordering, better sequence preservation
- **Higher values**: For high-latency or severely reordered networks

**Recommendation:** 200ms balances ordering with responsiveness

**Trade-off considerations:**
```
Higher repetition factor:
  + Better reliability
  + More tolerance for packet loss
  - More bandwidth usage
  - Higher latency

Shorter repetition delay:
  + Lower latency
  + Faster recovery from loss
  - Higher instantaneous bandwidth burst
  - Potential network congestion

Longer reordering delay:
  + Better sequence preservation
  + Fewer out-of-order deliveries
  - Higher latency
  - Longer wait for missing packets
```

---

## Testing

### Basic Functionality Tests

#### Test 1: UDP Communication
```bash
# Terminal 1
./receive_udp 8000 > received.txt

# Terminal 2
cat test_file.txt | ./send_udp localhost 8000

# Wait for completion, then compare
wc -c test_file.txt received.txt
# May show different sizes due to packet loss
```

#### Test 2: Echo Server
```bash
# Terminal 1
./reply_udp 8000

# Terminal 2
echo "Hello" | ./send_receive_udp localhost 8000
# Should see "Hello" echoed back
```

#### Test 3: TCP Tunnel
```bash
# Setup complete chain (see tunnel example above)
# Send large file
cat large_file.bin | ./send_receive_udp localhost 5000 > output.bin

# Verify integrity
md5sum large_file.bin output.bin
# Should match exactly (TCP guarantees delivery)
```

#### Test 4: FEC with Simulated Packet Loss
```bash
# Add packet loss to loopback interface (requires root)
sudo tc qdisc add dev lo root netem loss 10%

# Setup FEC chain (see FEC example above)
cat test_data.txt | ./send_receive_udp localhost 5000 > received.txt

# Compare
diff test_data.txt received.txt
# Should be identical or very close with FEC

# Remove packet loss
sudo tc qdisc del dev lo root
```

### Performance Tests

#### Throughput Test
```bash
# Generate 10MB of random data
dd if=/dev/urandom of=test10mb.bin bs=1M count=10

# Measure time for transmission
time cat test10mb.bin | ./send_receive_udp localhost 8000 > /dev/null
```

---

## Program Details

### Buffer Sizes

Each program uses specific buffer sizes as per assignment requirements:

| Program | Read Buffer | Send/Recv Buffer | Notes |
|---------|-------------|------------------|-------|
| send_udp | 480 bytes (stdin) | - | Avoids IP fragmentation |
| receive_udp | - | 65536 bytes | Max UDP payload |
| reply_udp | - | 65536 bytes | Max UDP payload |
| send_receive_udp | 480 bytes (stdin) | 65536 bytes (UDP) | Dual buffers |
| tunnel_client | 65536 (UDP) | 65538 (TCP) | +2 for header |
| tunnel_server | 65536 (UDP) | 65538 (TCP) | +2 for header |
| udp_fec | 65532 (regular) | 65536 (FEC) | +4 for seq# |

### Protocol Specifications

#### UDP Tunneling Protocol (tunnel programs)

**Message format:**
```
| 2-byte length (network order) | UDP payload (0-65536 bytes) |
```

- Length field: uint16_t in network byte order (big-endian)
- Indicates size of following UDP payload
- Empty UDP packet: length=0, total message=2 bytes
- Maximum message: length=65536, total message=65538 bytes

**Encoding example:**
```c
uint16_t payload_len = 100;
uint16_t net_len = htons(payload_len);  // Convert to network order
// Send: [net_len (2 bytes)][payload (100 bytes)]
```

**Decoding:**
```c
uint16_t net_len;
read(tcp_fd, &net_len, 2);
uint16_t payload_len = ntohs(net_len);  // Convert to host order
// Then read payload_len bytes
```

#### FEC Protocol (udp_fec)

**FEC packet format:**
```
| 4-byte sequence number (network order) | Original UDP payload (0-65532 bytes) |
```

- Sequence number: uint32_t in network byte order
- Starts at 0, increments for each packet
- Wraps around after 2^32 (4,294,967,296) packets
- Regular UDP payload reduced by 4 bytes for FEC header

**Duplicate detection:**
- Uses array of 65536 uint16_t values
- Split sequence number into high 16 bits and low 16 bits
- Use low 16 bits as index, store high 16 bits as value
- If stored value matches high bits → duplicate
- Otherwise update array and forward packet

**Sequence number split example:**
```c
uint32_t seq = 0x12345678;
uint16_t hi = seq >> 16;         // 0x1234
uint16_t lo = seq & 0xFFFF;      // 0x5678

// Check array
if (dedup_array[lo] == hi) {
    // Duplicate
} else {
    dedup_array[lo] = hi;
    // Forward packet
}
```

### Port Numbering

Valid port range: 0-65535

**Reserved ports (0-1023):**
- Require root privileges
- Avoid for testing

**Recommended ports (1024-49151):**
- Registered ports, generally safe for testing
- Examples: 8000, 9000, 5000-7000

**Dynamic ports (49152-65535):**
- Typically used by OS for ephemeral connections
- Can be used but may conflict

**Testing recommendations:**
- Use ports 5000-9000 for testing
- Ensure no conflicts with other services
- Check with `netstat -tuln | grep PORT` before using

### System Calls Used

**Socket creation and setup:**
- `socket()`: Create socket file descriptor
- `bind()`: Bind socket to address/port
- `listen()`: Mark socket as passive (TCP)
- `accept()`: Accept incoming connection (TCP)
- `connect()`: Connect to remote address

**Data transfer:**
- `send()`, `sendto()`: Send data over socket
- `recv()`, `recvfrom()`: Receive data from socket
- `read()`, `write()`: Generic I/O on file descriptors

**I/O multiplexing:**
- `select()`: Monitor multiple file descriptors
- `FD_ZERO()`, `FD_SET()`, `FD_ISSET()`: fd_set manipulation

**Address resolution:**
- `getaddrinfo()`: Resolve hostname to address
- `freeaddrinfo()`: Free allocated address info

**Byte order conversion:**
- `htons()`, `htonl()`: Host to network byte order (16/32-bit)
- `ntohs()`, `ntohl()`: Network to host byte order (16/32-bit)

**Time:**
- `clock_gettime()`: Get high-resolution time (for udp_fec)

**Memory:**
- `malloc()`, `calloc()`, `realloc()`: Allocate memory (for udp_fec queues)
- `free()`: Deallocate memory
- `memcpy()`, `memset()`: Memory operations

---

## Network Configuration

### IPv4 vs IPv6

All programs support IPv4. The code uses `AF_INET` for IPv4 sockets.

**To add IPv6 support (optional):**
- Change `AF_INET` to `AF_INET6`
- Change `struct sockaddr_in` to `struct sockaddr_in6`
- Use `struct sockaddr_in6` for dual-stack support

**Current implementation:**
```c
hints.ai_family = AF_INET;  // IPv4 only
```


---


## Additional Notes

### Code Organization

All programs follow similar structure:
1. Argument parsing and validation
2. Socket creation and setup
3. Main event loop (with select() where needed)
4. Resource cleanup

**Helper functions used:**
- `better_write()`: Ensures complete writes (handles partial writes)
- `convert_port_name()`: Validates and converts port string to uint16_t

### Error Handling

All programs implement proper error handling:
- Check return values of all system calls
- Print descriptive error messages using `strerror(errno)`
- Clean up resources before exit
- Exit with non-zero status on error

### Memory Management

**Stack allocation:**
- send_udp, receive_udp, reply_udp, send_receive_udp: All buffers on stack
- tunnel programs: Buffers on stack

**Dynamic allocation:**
- udp_fec: Uses malloc/free for queue entries
- Must be leak-free (verified with valgrind)

### Resource Cleanup

All programs properly clean up:
- Close all file descriptors with `close()`
- Free dynamically allocated memory with `free()`
- Free getaddrinfo results with `freeaddrinfo()`

---

## References

### Man Pages (Essential Reading)

```bash
man 2 socket      # Creating sockets
man 2 bind        # Binding to address/port
man 2 connect     # Connecting to peer
man 2 listen      # Listening for connections (TCP)
man 2 accept      # Accepting connections (TCP)
man 2 send        # Sending data
man 2 recv        # Receiving data
man 2 sendto      # Sending to specific address
man 2 recvfrom    # Receiving with source address
man 2 select      # I/O multiplexing
man 3 getaddrinfo # Address resolution
man 7 ip          # IP protocol
man 7 udp         # UDP protocol
man 7 tcp         # TCP protocol
```

---

## License and Attribution

This code was developed as part of CS4375 Operating Systems Concepts coursework at UTEP, Fall 2025.

**Helper functions provided by:**
- Professor Dr. Christoph Lauter, UTEP
- Functions: `better_write()`, `convert_port_name()`

**Course:** CS4375 Operating Systems Concepts  
**Semester:** Fall 2025  
**Institution:** University of Texas at El Paso

---


**Important:** When testing on UTEP's `dandelion` server, ensure you are connected from a UTEP IP address, as external access to custom ports is blocked by the firewall.

---

**Last Updated:** November 2025