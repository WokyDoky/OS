

# Operating Systems: Core UNIX Utilities in C

This repository contains C implementations of the standard UNIX utilities `head` and `tail`, along with a custom file-searching tool named `findlocation`. This project was developed for the CS4375 Operating Systems Concepts course and focuses on using low-level POSIX system calls for file I/O, memory management, and process handling, avoiding the use of standard C library functions like `printf` or `fopen` for I/O operations[cite: 315, 353].

-----

## General Overview

The project is divided into three main components:

1.  **`head`**: A program that outputs the first part of files.
2.  **`tail`**: A program that outputs the last part of files. It uses different strategies for seekable and non-seekable inputs.
3.  **`findlocation`**: An efficient tool that searches a sorted data file for a specific phone number prefix. It leverages memory mapping (`mmap`) for high performance on regular files.

### Key Technical Features

  * **Core System Calls**: `open`, `read`, `write`, `close`, `lseek`, `mmap`, `munmap`.
  * **Memory Management**: Manual memory management using `malloc` and `free`, particularly for buffering in `tail` and `findlocation` when handling non-seekable streams.
  * **Efficiency**: The `tail` and `findlocation` programs are designed to be efficient. [cite\_start]`tail` uses `lseek` to avoid reading the entire file when possible, and `findlocation` uses `mmap` and a binary search algorithm to achieve O(log n) lookup time on seekable files.

-----

## How to Compile and Run

A `Makefile` is included for easy compilation.

### Compilation

To compile all three programs at once, simply run:

```bash
make
```

This will create three executables: `head`, `tail`, and `findlocation`.

You can also compile each program individually:

```bash
make head
make tail
make findlocation
```

To remove the compiled executables, run:

```bash
make clean
```

### Execution and Usage

#### **`./head`**

Displays the first 10 lines of a file or standard input by default. The number of lines can be changed with the `-n` flag.

**Examples:**

```bash
# Display the first 10 lines of nanpa
./head nanpa

# Display the first 17 lines of nanpa
./head -n 17 nanpa

# Read from standard input and display the first 5 lines
cat nanpa | ./head -n 5
```

-----

#### **`./tail`**

Displays the last 10 lines of a file or standard input by default. The number of lines can be changed with the `-n` flag.

**Examples:**

```bash
# Display the last 10 lines of nanpa
./tail nanpa

# Display the last 42 lines of nanpa
./tail -n 42 nanpa

# Read from standard input and display the last 20 lines
cat nanpa | ./tail -n 20
```

-----

#### **`./findlocation`**

Searches for a 6-digit prefix in a formatted data file (like the provided `nanpa` file).

**Examples:**

```bash
# Find the location for prefix 915220 in the file nanpa
./findlocation 915220 nanpa

# Find the location for prefix 503526 by piping the file contents
cat nanpa | ./findlocation 503526
```