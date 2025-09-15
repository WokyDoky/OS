#include <unistd.h> // For write, STDOUT_FILENO, STDERR_FILENO
#include <fcntl.h>  // For open, O_RDONLY
#include <errno.h>  // For errno
// #include <stdio.h>

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

/**
 * Opens all valid file paths from argv and fills a provided array with their file descriptors.
 * ¡Remember to close!
 * @param argc The argument count.
 * @param argv The argument vector (array of strings).
 * @param fd_list A pre-allocated integer array to store the file descriptors.
 * @return The number of files successfully opened.
 */
int open_files_in_argument_vector(int argc, char *argv[], int fd_list[]){
    int count = 0;
    for (int i = 1; i < argc; i++){
        //¿Check if array is full?
        int fd = open(argv[i], O_RDONLY);
        if (fd != -1){
            fd_list[i - 1] = fd;
            count++;
        }
    }
    if(count < 1){
        // TODO: Ask what to do when no files are given.
        //      Possible solution, cat text. 
        print_to_fd(STDOUT_FILENO, "no files given");
    }
    return count;
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

    /*
    This flag counter does not work like the real thing
    real head takes last "-n" value
    no need for nCount
    */
    pos++;
    if (nCount == 1){
        if(isNumber(argv[pos])){
            if (argv[pos]){
                return argument_to_int(argv[pos]);
            }
        }
    }
    //print_to_fd(STDERR_FILENO, "Invalid Flag\n");
    return 0;
}

/**
 * Prints help text to standard output.
 */
void help (){
    print_to_fd(STDOUT_FILENO, "Usage: head [OPTION]... [FILE]...\nPrint the first 10 lines of each FILE to standard output.\nWith more than one FILE, precede each with a header giving the file name.\n\nWith no FILE, or when FILE is -, read standard input.\n");
    print_to_fd(STDOUT_FILENO, "\n    -n, --lines=[-]NUM        print the first NUM lines instead of the first 10; \n                              with the leading '-', print all but the last \n                              NUM lines of each file\n");
}


#define READ_BUFFER_SIZE 8192
#define LINE_BUFFER_SIZE 4096

/**
 * @brief Reads the first 'n' lines from a list of files and prints them.
 * * @param fd_list Array of file descriptors to read from.
 * @param file_names Array of corresponding file names for headers.
 * @param fd_count The number of files in the arrays.
 * @param n The number of lines to print from each file.
 */
void print_head(int fd_list[], char *file_names[], int fd_count, int n) {
    char read_buffer[READ_BUFFER_SIZE];
    char line_buffer[LINE_BUFFER_SIZE];
    int line_pos = 0;
    long bytes_read;

    // Loop through each file descriptor
    for (int i = 0; i < fd_count; i++) {
        // If there is more than one file, print a header.
        if (fd_count > 1) {
            // Add a newline before the header for all but the first file.
            if (i > 0) {
                print_to_fd(STDOUT_FILENO, "\n");
            }
            // Print the header: "==> filename <=="
            print_to_fd(STDOUT_FILENO, "==> ");
            print_to_fd(STDOUT_FILENO, file_names[i]);
            print_to_fd(STDOUT_FILENO, " <==\n");
        }

        int lines_printed = 0;
        int file_done = 0;
        line_pos = 0;

        // Read the file in chunks
        while (!file_done && (bytes_read = read(fd_list[i], read_buffer, READ_BUFFER_SIZE)) > 0) {
            // Process each character in the chunk
            for (int j = 0; j < bytes_read; j++) {
                if (line_pos < LINE_BUFFER_SIZE - 1) {
                    line_buffer[line_pos++] = read_buffer[j];
                }

                // If we found a newline, the line is complete
                if (read_buffer[j] == '\n') {
                    line_buffer[line_pos] = '\0'; // Null-terminate
                    print_to_fd(STDOUT_FILENO, line_buffer); // Print the assembled line
                    lines_printed++;
                    line_pos = 0; // Reset for the next line
                }

                // If we've printed enough lines, we're done with this file
                if (lines_printed >= n) {
                    file_done = 1;
                    break; // Exit the inner character loop
                }
            }
        }
        
        if (bytes_read < 0) {
            // Handle read errors by writing a message to standard error
            print_to_fd(STDERR_FILENO, "head: error reading file\n");
        }

        // After the loop, if there's an unterminated line left in the buffer
        // and we haven't printed enough lines yet, print it.
        // This handles files that don't end with a newline.
        if (line_pos > 0 && lines_printed < n) {
            line_buffer[line_pos] = '\0';
            print_to_fd(STDOUT_FILENO, line_buffer);
            print_to_fd(STDOUT_FILENO, "\n"); // Add a newline for consistency
        }
    }
}

int main(int argc, char *argv[]){
    // Default to 10 lines
    int lines_to_print = 10;
    
    // Arrays to store actual filenames and their file descriptors
    char *files_to_open[argc];
    int file_count = 0;

    if (argc > 1 && compareStrings(argv[1], "--help")) {
        help();
        return 0;
    }

    // --- Argument Parsing ---
    for (int i = 1; i < argc; i++) {
        if (compareStrings(argv[i], "-n")) {
            // Check if there is a number after -n
            if (i + 1 < argc && isNumber(argv[i + 1])) {
                lines_to_print = argument_to_int(argv[i + 1]);
                i++; // Skip the number argument in the next iteration
            } else {
                print_to_fd(STDERR_FILENO, "head: option requires an argument -- 'n'\n");
                return 1;
            }
        } else {
            // It's not a flag we recognize, so treat it as a filename
            files_to_open[file_count] = argv[i];
            file_count++;
        }
    }

    // --- Execution  ---
    int fd_list[file_count];
    int opened_files_count = 0;

    if (file_count == 0) {
        // No filenames provided, read from standard input
        fd_list[0] = STDIN_FILENO; // STDIN_FILENO is 0
        char *stdin_name[] = {"standard input"};
        opened_files_count = 1;
        print_head(fd_list, stdin_name, opened_files_count, lines_to_print);
    } else {
        // Open all the files we identified
        for (int i = 0; i < file_count; i++) {
            int fd = open(files_to_open[i], O_RDONLY);
            if (fd != -1) {
                fd_list[opened_files_count++] = fd;
            } else {
                // Print an error message to standard error if a file can't be opened
                print_to_fd(STDERR_FILENO, "head: cannot open '");
                print_to_fd(STDERR_FILENO, files_to_open[i]);
                print_to_fd(STDERR_FILENO, "' for reading\n");
            }
        }
        
        if (opened_files_count > 0) {
            print_head(fd_list, files_to_open, opened_files_count, lines_to_print);
            // Close all the opened files
            close_files(fd_list, opened_files_count);
        }
    }

    return 0;
}