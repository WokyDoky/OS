#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h> // Required for strerror

// Forward declarations
int str_len(const char *str);
void write_string(int fd, const char *str);
int better_write(int fd, const void *buf, size_t size);
/**
 * @return 1 if string is a string of numbers, else 0.
 */
int isNumber(const char *str) {
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
int argument_to_int(char *num){
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
 * @return length of str.
*/
int str_len(const char *str) {
    int length = 0;
    while (str[length]) {
        length++;
    }
    return length;
}

/**
 * @return 1 if S1 and S2 are equal else 0.
 */
int compareStrings(char S1[], char S2[]){

    if (str_len(S1) != str_len(S2))
        return 0;
    int i = 0;
    while (S1[i]) {
        if (S1[i] != S2[i])
            return 0;
        i++;
    }
    return 1;
}

/*
--------------
PRINTING
--------------
*/

/**
 * A helper function to substitute printf. Writes a null-terminated string
 * to the specified file descriptor.
 * @param fd The file descriptor to write to (e.g., STDOUT_FILENO, STDERR_FILENO).
 * @param str The null-terminated string to write.
 */
void write_string(int fd, const char *str) {
    if (better_write(fd, str, str_len(str)) == -1) {
        // If writing an error to stderr fails, there's nothing more we can do.
        // If writing to stdout fails, we can at least try to report it.
        if (fd == STDOUT_FILENO) {
            const char *errMsg = "Error: Could not write to standard output.\n";
            better_write(STDERR_FILENO, errMsg, str_len(errMsg));
        }
    }
}
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
 * Closes all open files in fd_list
 */
void close_files(int fd_list[], int count){
    for(int i = 0; i < count; i++){
        close(fd_list[i]);
    }
}

/**
 * @param argc argument counter.
 * @param argv argument vector.
 * @return value of n (lines to print), else 0.
 */

int has_flag(int argc, char *argv[]){
    int nCount = 0;
    int pos = -1;
    for (int i = 1; i < argc; i++){
        if (compareStrings(argv[i], "-n") && i < argc){
            nCount++;
            pos = i;
        }
    }

    pos++;
    if (nCount == 1){
        if(isNumber(argv[pos])){
            if (argv[pos]){
                return argument_to_int(argv[pos]);
            }
        }
    }
    return 0;
}
/*
---
Check arguments above this line
---
*/


/**
 * Prints help
 */
void help (){
    write_string(STDOUT_FILENO, "Usage: tail [OPTION]... [FILE]...\n");
    write_string(STDOUT_FILENO, "Print the last 10 lines of each FILE to standard output.\n");
    write_string(STDOUT_FILENO, "With more than one FILE, precede each with a header giving the file name.\n\n");
    write_string(STDOUT_FILENO, "With no FILE, or when FILE is -, read standard input.\n");
    write_string(STDOUT_FILENO, "\n    -n, --lines=[-]NUM       output the last NUM lines, instead of the last 10;\n");
    write_string(STDOUT_FILENO, "\n");
    write_string(STDOUT_FILENO, "GNU coreutils online help: <https://www.gnu.org/software/coreutils/>\n");
    write_string(STDOUT_FILENO, "Full documentation <https://www.gnu.org/software/coreutils/tail>\n");
    write_string(STDOUT_FILENO, "or available locally via: info '(coreutils) tail invocation'\n");
}

/**
 * A helper function to handle write errors. Writing to stdout can fail
 * (e.g., if the pipe is broken), so we should check for it.
 */
void write_checked(const void *buf, size_t count) {
    if (write(STDOUT_FILENO, buf, count) == -1) {
        write_string(STDERR_FILENO, "Error writing to stdout\n");
        // In a real utility, you might want to exit here.
    }
}

/*
-------------------
TAIL
-------------------
*/

#define BUFFER_SIZE 8192

int print_tail(int fd_list[], char *file_names[], int fd_count, int n) {
    for (int i = 0; i < fd_count; i++) {
        if (fd_count > 1) {
            if (i > 0) {
                write_checked("\n", 1);
            }
            // Print header using direct write calls
            write_checked("==> ", 4);
            write_checked(file_names[i], str_len(file_names[i]));
            write_checked(" <==\n", 5);
        }

        struct stat st;
        if (fstat(fd_list[i], &st) == -1) {
            write_string(STDERR_FILENO, "error getting stat");
            write_string(STDERR_FILENO, "\n");
            continue; // Move to the next file on error
        }
        off_t file_size = st.st_size;

        if (file_size == 0) {
            continue; // Empty file, nothing to print
        }

        // --- 1. SCAN BACKWARDS TO FIND THE STARTING POSITION ---
        char buffer[BUFFER_SIZE];
        off_t pos = file_size;
        int newlines_found = 0;

        // file might not end in a new line.
        lseek(fd_list[i], -1, SEEK_END);
        read(fd_list[i], buffer, 1);
        if (buffer[0] != '\n') {
            newlines_found++;
        }

        while (pos > 0 && newlines_found <= n) {
            ssize_t chunk_size = (pos > BUFFER_SIZE) ? BUFFER_SIZE : pos;
            pos -= chunk_size;

            lseek(fd_list[i], pos, SEEK_SET);
            ssize_t bytes_read = read(fd_list[i], buffer, chunk_size);
            if (bytes_read <= 0) break;

            for (ssize_t j = bytes_read - 1; j >= 0; j--) {
                if (buffer[j] == '\n') {
                    newlines_found++;
                    if (newlines_found > n) {
                        // We found our start. It's the character after this newline.
                        pos = pos + j + 1;
                        goto end_scan; // Exit nested loops
                    }
                }
            }
        }
        end_scan:; // Label to break out of the nested loop

        // If we scanned the whole file and found less than 'n' newlines,
        // the starting position is the beginning of the file (0).
        if (newlines_found <= n) {
            pos = 0;
        }

        // --- 2. SEEK TO START AND PRINT TO THE END ---
        lseek(fd_list[i], pos, SEEK_SET);

        ssize_t bytes_read;
        while ((bytes_read = read(fd_list[i], buffer, BUFFER_SIZE)) > 0) {
            write_checked(buffer, bytes_read);
        }
    }
    return 0; // Success
}

int main(int argc, char *argv[]){
    // Default to 10 lines
    int lines_to_print = 10;

    // Arrays to store actual filenames and their file descriptors
    char *files_to_open[argc];
    int file_count = 0;

    if (argc > 1 && compareStrings(argv[1], "--help")){
        help();
        return 0;
    }


    // --- Argument Parsing ---
    for (int i = 1; i < argc; i++){
        if (compareStrings(argv[i], "-n")){
            if (i + 1 < argc && isNumber(argv[i+1])){
                lines_to_print = argument_to_int(argv[i+1]);
                i++;
            }
            else{
                write_string(STDERR_FILENO, "tail: option requires an argument -- 'n'\n");
                return 1;
            }
        } else{
            // It's not a flag we recognize, so treat it as a filename
            files_to_open[file_count] = argv[i];
            file_count++;
        }
    }

    // --- Execution  ---
    int fd_list[file_count];
    int opened_files_count = 0;

    if (file_count == 0){
        // No files provided, read from standard input
        fd_list[0] = STDIN_FILENO; // STDIN_FILENO is 0
        char *stdin_name[] = {"standard input"};
        opened_files_count = 1;
        // Tail on stdin is a more complex problem (can't seek),
        // this implementation will read the whole stream.
        // For this exercise, we'll assume it's a pipe from a file.
        print_tail(fd_list, stdin_name, opened_files_count, lines_to_print);
    } else{
        for (int i = 0; i < file_count; i++){
            int fd = open(files_to_open[i], O_RDONLY);
            if (fd != -1){
                fd_list[opened_files_count++] = fd;
            } else {
                // Report error opening a specific file
                char *errMsg = strerror(errno);
                write_string(STDERR_FILENO, "tail: cannot open '");
                write_string(STDERR_FILENO, files_to_open[i]);
                write_string(STDERR_FILENO, "' for reading: ");
                write_string(STDERR_FILENO, errMsg);
                write_string(STDERR_FILENO, "\n");
            }
        }

        if (opened_files_count > 0){
            print_tail(fd_list, files_to_open, opened_files_count, lines_to_print);
            close_files(fd_list, opened_files_count);
        }
    }

    return 0;
}