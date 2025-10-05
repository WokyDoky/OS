#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h> 

#define BUF_SIZE 65536

/*
================================================================================
                            UTILITIES
================================================================================
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
 * @brief Writes a string to a given file descriptor using the write system call.
 * @param fd The file descriptor to write to (e.g., STDOUT_FILENO, STDERR_FILENO).
 * @param str The null-terminated string to write.
 * @return 0 on success, -1 on failure.
 */
int print_to_fd(int fd, const char *str) {
    return better_write(fd, str, str_len(str));
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
int string_to_int(const char *num){
    int ans = 0;
    unsigned i = 0;
        while ( num[i] >= '0' && num[i] <= '9' ){
            ans *= 10;
            ans += num[i] - '0';
            ++i;
        }
    return ans;
}


int is_digit (char c){
    return (c >= '0' && c <= '9');
}


int is_valid_port(const char *port){
    if (port == NULL || *port == '\0') return 0;
    if (isNumber(port)){
        int pport = string_to_int(port);
        if (pport >= 0 && pport <= 65535){
            return 1;
        }
    }
    return 0;

}
/* ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
================================================================================
                            UTILITIES
================================================================================
*/

/**
 ** @author Professor Dr. Christoph Lauter, UTEP.
 * @note This function was provided as part of the course materials for
 * Operating Systems Concepts and is not original work.
 */
#include <stdint.h>
#include <stdlib.h> // Required for strtoll

static int convert_port_name(uint16_t *port, const char *port_name) {
    char *end;
    long long int nn;
    uint16_t t;
    long long int tt;

    if (port_name == NULL) return -1;
    if (*port_name == '\0') return -1;
    nn = strtoll(port_name, &end, 0);
    if (*end != '\0') return -1;
    if (nn < 0) return -1;
    t = (uint16_t) nn;
    tt = (long long int) t;
    if (tt != nn) return -1;
    *port = t;
    return 0;
}

void help (char **argv){
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
}

int driver (uint16_t port){
    char buffer[BUF_SIZE];
    ssize_t bytes_received;
    int socketfd;
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));

    // Create a UDP Socket
    socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketfd < 0){
        fprintf(stderr, "Error creating socket: %s\n", strerror(errno));
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);
 
    // bind server address to socket descriptor
    if (bind(socketfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        fprintf(stderr, "Error binding socket to port %u: %s\n", port, strerror(errno));
        close(socketfd);
        return -1;
    }  
    //receive the packet
    for (;;){
        bytes_received = recv(socketfd, buffer, sizeof(buffer), 0);

        if (bytes_received < 0){
            fprintf(stderr, "Error receiving data: %s\n", strerror(errno));
            close(socketfd);
            return -1;
        }
        //EOF
        if (bytes_received == 0) break;

        if (print_to_fd(STDOUT_FILENO, buffer) < 0){
            fprintf(stderr, "Error writing to standard output: %s\n", strerror(errno));
            close(socketfd);
            return -1;
        }
    }
    close(socketfd);
    print_to_fd(STDOUT_FILENO, "\nDebug EOF\n");
    return 0;
}


int main(int argc, char **argv){


    if (argc != 2){
        help(argv);
        exit(1);
    }

    char *port_name = argv[1];
    uint16_t port;

    if (convert_port_name(&port, port_name) != 0) {
        fprintf(stderr, "Error: Invalid port number '%s'. Must be an integer between 0 and 65535.\n", argv[1]);
        exit(1);
    }

    if (driver(port) < 0){
        // Error prints handled by function. 
        exit(1);
    }

    return 0;
}