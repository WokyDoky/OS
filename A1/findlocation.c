/*
Useful vide: 
https://www.youtube.com/watch?v=m7E9piHcfr4
*/

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>

#define RECORD_SIZE 32
#define PREFIX_LEN 6
#define LOCATION_LEN 25

/**
 *  @return length of str. 
*/
int str_len(const char *str) {
    int length = 0;
    while (str && str[length]) {
        length++;
    }
    return length;
}

/**
 * @return 1 if string is a string of numbers, else 0.
 */
int is_number(const char *str) {
    if (str == NULL || *str == '\0') { // Handle NULL or empty string
        return 0;
    }

    int i = 0;
    int has_digits = 0;

    while (str[i] != '\0') {
        if (str[i] >= '0' && str[i] <= '9') {
            has_digits = 1;
        } else {
            return 0;
        }
        i++;
    }
    return has_digits;
}

/**
 * @return an int from a string.
 */
int string_to_int(char *num){
    int ans = 0;
    unsigned i = 0;
        while ( num[i] >= '0' && num[i] <= '9' ){
            ans *= 10;
            ans += num[i] - '0';
            ++i;
        }
    return ans;
}

/**
 * @brief Compares the first n characters of two strings.
 * @return 0 if they are equal, <0 if s1 < s2, >0 if s1 > s2.
 */
int compare_string(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] < s2[i]) return -1;
        if (s1[i] > s2[i]) return 1;
        if (s1[i] == '\0') return 0; // End of string
    }
    return 0;
}

/**
 * @brief Cuts a string to a specified length.
 *
 * @param dest The buffer where the cut string will be stored.
 * @param dest_size The total size of the destination buffer.
 * @param src The original source string to cut.
 * @param n The maximum number of characters to copy from src.
 */
void cut_string(char *dest, int dest_size, const char *src, int n) {
  int i = 0;
  while (i < n && src[i] != '\0' && i < dest_size - 1) {
    dest[i] = src[i];
    i++;
  }
  dest[i] = '\0';
}



/*
--------------
PRINTING
--------------
*/

/**
 * @brief Ensures that a specified number of bytes are written to a file descriptor,
 * handling partial writes by looping until all data is sent.
 * * @author Professor Dr. Christoph Lauter, UTEP.
 * @note This function was provided as part of the course materials for
 * Operating Systems Concepts and is not original work.
 * @param fd    The file descriptor to write to.
 * @param buf   A pointer to the buffer containing the data.
 * @param size  The total number of bytes to write.
 * @return 0 on success, -1 on failure.
 */
int better_write(int fd, const void *buf, size_t size) {
  size_t bytes_to_write;
  size_t bytes_already_written;
  size_t bytes_written_this_time;
  ssize_t res_write;

  bytes_to_write = size;
  bytes_already_written = (size_t) 0;
  while (bytes_to_write > ((size_t) 0)) {
    res_write = write(fd,
		      &((const char *) buf)[bytes_already_written],
		      bytes_to_write);
    if (res_write < ((ssize_t) 0)) {
      return -1;
    }
    bytes_written_this_time = (size_t) res_write;
    bytes_to_write -= bytes_written_this_time;
    bytes_already_written += bytes_written_this_time;
  }
  return 0;
}

/**
 * @brief Writes a string to a given file descriptor using the write system call.
 * @param fd The file descriptor to write to (e.g., STDOUT_FILENO, STDERR_FILENO).
 * @param str The null-terminated string to write.
 */
void print_to_fd(int fd, const char *str) {
    better_write(fd, str, str_len(str));
}

/*
================================================================================
                              FIND LOCATION
================================================================================
*/

#define LINE_LENGTH 32
#define PREFIX_LENGTH 6
#define LOCATION_LENGTH 25

/**
 * @brief Performs a binary search on the data for the given prefix.
 *
 * @param data A pointer to the start of the file data in memory.
 * @param size The total size of the data in bytes.
 * @param prefix The 6-digit phone number prefix to search for.
 */
void binary_search(const char *data, size_t size, const char *prefix) {
    // If data is empty or not a multiple of the line length, it's malformed or not found.
    if (size == 0 || size % LINE_LENGTH != 0) {
        print_to_fd(STDOUT_FILENO, "Not found\n");
        return;
    }

    long num_records = size / LINE_LENGTH;
    long low = 0;
    long high = num_records - 1;
    int found = 0;

    while (low <= high) {
        long mid = low + (high - low) / 2;
        const char *current_line = data + mid * LINE_LENGTH;
        int cmp = compare_string(prefix, current_line, PREFIX_LENGTH);

        if (cmp == 0) {
            better_write(STDOUT_FILENO, current_line + PREFIX_LENGTH, LOCATION_LENGTH);
            print_to_fd(STDOUT_FILENO, "\n");
            found = 1;
            break;
        } else if (cmp < 0) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }

    if (!found) {
        print_to_fd(STDOUT_FILENO, "Not found\n");
    }
}

/**
 * @brief Handles unseekable file descriptors by reading the entire file into heap memory.
 *
 * @param fd The unseekable file descriptor (e.g., a pipe).
 * @param prefix The 6-digit prefix to search for.
 */
void handle_unseekable(int fd, const char *prefix) {
    size_t capacity = 4096;
    char *data = (char *)malloc(capacity);
    if (data == NULL) {
        print_to_fd(STDERR_FILENO, "Error: Memory allocation failed.\n");
        exit(1);
    }

    size_t total_read = 0;
    ssize_t bytes_read;

    // Read in chunks, reallocating as necessary.
    while ((bytes_read = read(fd, data + total_read, capacity - total_read)) > 0) {
        total_read += bytes_read;
        if (total_read == capacity) {
            capacity *= 2;
            char *new_data = (char *)realloc(data, capacity);
            if (new_data == NULL) {
                print_to_fd(STDERR_FILENO, "Error: Memory reallocation failed.\n");
                free(data);
                exit(1);
            }
            data = new_data;
        }
    }

    if (bytes_read < 0) {
        print_to_fd(STDERR_FILENO, "Error: Failed to read from input.\n");
        free(data);
        exit(1);
    }
    
    // Validate file content integrity.
    if (total_read % LINE_LENGTH != 0) {
        print_to_fd(STDERR_FILENO, "Error: Input data is malformed.\n");
        free(data);
        exit(1);
    }

    binary_search(data, total_read, prefix);
    free(data);
}

/**
 * @brief Handles seekable file descriptors by memory-mapping the file.
 *
 * @param fd The seekable file descriptor (e.g., a regular file).
 * @param prefix The 6-digit prefix to search for.
 */
void handle_seekable(int fd, const char *prefix) {
    off_t file_size = lseek(fd, 0, SEEK_END);
    if (file_size < 0) {
        print_to_fd(STDERR_FILENO, "Error: Could not determine file size.\n");
        exit(1);
    }

    if (file_size == 0) {
        print_to_fd(STDOUT_FILENO, "Not found\n");
        return;
    }

    // Validate file content integrity.
    if (file_size % LINE_LENGTH != 0) {
        print_to_fd(STDERR_FILENO, "Error: File is malformed.\n");
        exit(1);
    }

    // Map the file into the process's address space.
    char *data = (char *)mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        print_to_fd(STDERR_FILENO, "Error: Failed to map file to memory.\n");
        exit(1);
    }

    binary_search(data, file_size, prefix);

    // Unmap the memory region.
    if (munmap(data, file_size) != 0) {
        print_to_fd(STDERR_FILENO, "Warning: Failed to unmap memory.\n");
    }
}

/*
================================================================================
                                  MAIN
================================================================================
*/

int main(int argc, char *argv[]) {
    // 1. Argument validation
    if (argc < 2) {
        print_to_fd(STDERR_FILENO, "Usage: ./findlocation <6-digit-prefix> [file]\n");
        return 1;
    }

    // Check for a 6-digit.
    if (str_len(argv[1]) != 6 || !is_number(argv[1])) {
        print_to_fd(STDERR_FILENO, "Error: Prefix must be 6 digits.\n");
        return 1;
    }

    // The first argument is now the prefix itself.
    const char *prefix = argv[1];

    // 2. Determine input source and get file descriptor
    int fd;
    int close_fd_at_end = 0;
    if (argc > 2) {
        // Use the file provided as the second argument
        fd = open(argv[2], O_RDONLY);
        if (fd < 0) {
            print_to_fd(STDERR_FILENO, "Error: Cannot open file.\n");
            return 1;
        }
        close_fd_at_end = 1;
    } else {
        // Use standard input
        fd = STDIN_FILENO;
    }

    // 3. Check if the file descriptor is seekable and branch to the correct handler
    off_t seek_res = lseek(fd, 0, SEEK_CUR);

    if (seek_res == (off_t)-1) {
        // Unseekable (e.g., a pipe): read into memory
        handle_unseekable(fd, prefix);
    } else {
        // Seekable (a regular file): use memory mapping
        lseek(fd, 0, SEEK_SET); // Reset file offset after check
        handle_seekable(fd, prefix);
    }

    // 4. Cleanup: close the file if it was opened by this program
    if (close_fd_at_end) {
        close(fd);
    }

    return 0;
}