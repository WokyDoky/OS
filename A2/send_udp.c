#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>

#define BUF_SIZE 480
#define _GNU_SOURCE

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

/**
 * @brief Compares two strings, s1 and s2.
 * * @param s1 The first null-terminated string.
 * @param s2 The second null-terminated string.
 * @return An integer less than, equal to, or greater than zero if s1 is found,
 * respectively, to be less than, to match, or be greater than s2.
 */
int strcmp(const char *s1, const char *s2) {
    // Method used for testing. 
    while (*s1 && *s2) {
        // If characters are different, stop and return the difference
        if (*s1 != *s2) {
            break; 
        }
        s1++;
        s2++;
    }
    
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}
/* ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
================================================================================
                            UTILITIES
================================================================================
*/


int is_valid_ip(const char *addr) {
    if (addr == NULL) return 0;

    /**
     * An IPv4 address has the format x.x.x.x, 
     * where x is called an octet and must be a decimal value 
     * between 0 and 255.
     */
    int octet_value = 0;
    int num_digits = 0;
    int octet_count = 1;  // Start with the 1st octet

    // Loop through the string character by character
    for (int i = 0; ; i++) {
        char c = addr[i];
        if (is_digit(c)) {
            octet_value = (octet_value * 10) + (c - '0');
            num_digits++;
            if (num_digits > 3 || octet_value > 255) {
                return 0;
            }
        } 
        // Check if we've reached the end of an octet (or the whole string)
        else if (c == '.' || c == '\0') {
            // An octet cannot be empty (e.g., "1..2" or ".1.2.3")
            if (num_digits == 0) {
                return 0;
            }
            // If it's a dot, prepare for the next octet
            if (c == '.') {
                octet_count++;                
                if (octet_count > 4) return 0;
                num_digits = 0;
                octet_value = 0;
            } 
            // If it's the end of the string, we're done
            else { // c == '\0'
                break;
            }
        } 
        // Any other character is invalid
        else {
            return 0;
        }
    }

    // A valid IP must have exactly 4 octets
    return (octet_count == 4);
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
/**
 * @brief Resolves server name, creates and connects a UDP socket.
 *
 * @param server_name The FQDN or IP address of the server.
 * @param port_name The port number or service name (e.g., "http").
 * @return The file descriptor for the connected socket, or -1 on error.
 */
static int open_udp_fd(const char *server_name, const char *port_name) {
  struct addrinfo hints;
  struct addrinfo *result, *curr;
  int fd = -1;
  int gai_code;

  // Configure hints for getaddrinfo as specified
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;       // Use IPv4
  hints.ai_socktype = SOCK_DGRAM;  // Use UDP
  hints.ai_protocol = 0;
  hints.ai_flags = 0;

  gai_code = getaddrinfo(server_name, port_name, &hints, &result);
  if (gai_code != 0) {
    fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(gai_code));
    return -1;
  }

  // Loop through the results and try to connect
  for (curr = result; curr != NULL; curr = curr->ai_next) {
    fd = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
    if (fd < 0) {
      // Could not create socket, try next address
      continue;
    }

    // For UDP, connect() sets the default destination address for send()
    if (connect(fd, curr->ai_addr, curr->ai_addrlen) == 0) {
      // Success!
      break;
    }

    // If connect failed, close the socket and try the next address
    close(fd);
    fd = -1;
  }

  // Free the linked list allocated by getaddrinfo
  freeaddrinfo(result);

  if (fd < 0) {
    fprintf(stderr, "Could not create and connect the socket.\n");
    return -1;
  }

  return fd;
}

/**
 * @brief Displays usage information.
 */
void help (char **argv){
    fprintf(stderr, "Usage: %s <server_name> <port_name>\n", argv[0]);
}


/* 
================================================================================
                                    Main
================================================================================
*/
int main(int argc, char **argv) {
    if (argc != 3){
        help(argv);
        exit(1);
    }
    
    char* direccion_servidor = argv[1];
    char* port = argv[2];


    if (!is_valid_port(port)){
        help(argv);
        exit(1);
    }

    int fd = open_udp_fd(direccion_servidor, port); 
    if (fd < 0) {
        exit(1);
    }

    char buffer[BUF_SIZE];
    ssize_t bytes_leidos;
    ssize_t bytes_enviados;
    
    while ((bytes_leidos = read(STDIN_FILENO, buffer, BUF_SIZE)) > 0){
        bytes_enviados = send(fd, buffer, bytes_leidos, 0);
        if (bytes_enviados < 0){
            printf("Send failed\n");
            close(fd);
            exit(1);
        }
        if (bytes_enviados != bytes_leidos){
            printf("Could not send the entire chunk\n");
            close(fd);
        }
    }
    if (bytes_leidos < 0){
        printf("Reading failed\n");
        close(fd);
        exit(1);
    }

    if (bytes_leidos==0){
        if (send(fd, NULL, 0, 0) < 0){
            printf("EOF handling failed.\n");
            close(fd);
            exit(1);
        }
    }
    if (close(fd) < 0){
        printf("Close failed\n");
        exit(1);
    }

    return 0;
}
