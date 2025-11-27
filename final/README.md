# Project Overview
This project implements the client-side logic for utepfs, a distributed filesystem based on FUSE (Filesystem in Userspace). The client intercepts local filesystem operations (like ls, mkdir, cat) and forwards them over a TCP/IP socket to a remote server using a custom binary protocol.

## Key Features

Reliable Networking: Implements custom read_n and write_n helpers to handle TCP fragmentation and EINTR interrupts, ensuring data integrity over the network.

Protocol Compliance: Features a centralized validate_header function that strictly enforces protocol requirements, checking Request IDs and OpCodes for every transaction.

Error Translation: Includes a robust mapping system that translates the server's custom byte-level error codes into standard POSIX errno values (e.g., ENOENT, EACCES), allowing standard Linux tools to handle errors gracefully.

Dynamic Directory Listing: The readdir implementation handles server-side chunking, dynamically allocating memory to retrieve directory listings of any size.


## Critical Dependency Warning

This program relies on a specific remote server provided by the course instructor.

Server Address: server.christoph-lauter.org

Port: 7777

The filesystem will not function if this server is offline, unreachable, or if the server-side process has been terminated. If operations hang indefinitely or fail immediately, please verify your internet connection and the server status.

# Building the Project

A Makefile is provided to simplify the compilation process. Open your terminal in the project directory.

Run the build command:

```
make
```


This will generate the executable named utepfs.

To remove build artifacts:

```
make clean
```

# Running the Filesystem

### 1. Create a Mount Point

FUSE requires an empty directory to serve as the "door" to the filesystem.

```
mkdir mnt
```

### 2. Mount the Filesystem

Run the executable with the server details. It is highly recommended to use the -f flag to run in the foreground, allowing you to see debug output and errors.

./utepfs --server=server.christoph-lauter.org --port=7777 ./mnt -f


> Note: The terminal will appear to "hang" while the program is running. This is normal behavior for a foreground process.

### 3. Usage

Open a new terminal window. You can now interact with the ./mnt directory using standard Linux commands.

Examples:

- List files (may take a moment due to network latency)
ls -la ./mnt

- Create a text file
echo "Hello Distributed World" > ./mnt/test.txt

- Read the file
cat ./mnt/test.txt

- Check filesystem statistics
df -h ./mnt


> Performance Note: You may observe visible latency when running commands.
