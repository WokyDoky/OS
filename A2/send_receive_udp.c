#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>

#define STDIN_BUFFER_SIZE 480
#define UDP_BUFFER_SIZE 65536

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
/* ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
================================================================================
                            UTILITIES
================================================================================
*/

/** 
 * @brief setup_udp_socket - Create and configure UDP socket for sending to server
 * @param server_name numerical label such as 192.0.2.1 that is assigned to a device connected to a computer network
 * @param port_name a 16-bit number, ranging from 0 to 65,535
 * @returns socket fd on success, -1 on error
 */
static int setup_udp_socket(const char *server_name, const char *port_name) {
    struct addrinfo hints;
    struct addrinfo *result;
    int gai_code;
    int sockfd;
    
    /* Configure hints for UDP socket */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = 0;
    hints.ai_flags = 0;
    
    gai_code = getaddrinfo(server_name, port_name, &hints, &result);
    if (gai_code != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(gai_code));
        return -1;
    }
    
    sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sockfd < 0) {
        fprintf(stderr, "socket error: %s\n", strerror(errno));
        freeaddrinfo(result);
        return -1;
    }
    
    if (connect(sockfd, result->ai_addr, result->ai_addrlen) < 0) {
        fprintf(stderr, "connect error: %s\n", strerror(errno));
        close(sockfd);
        freeaddrinfo(result);
        return -1;
    }
    
    freeaddrinfo(result);
    return sockfd;
}


int main(int argc, char *argv[]) {
    int sockfd;
    char stdin_buffer[STDIN_BUFFER_SIZE];
    char udp_buffer[UDP_BUFFER_SIZE];
    ssize_t bytes_read;
    ssize_t bytes_received;
    fd_set read_fds;
    int max_fd;
    int stdin_closed = 0;
    int select_result;
    

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <server_name> <port_name>\n", argv[0]);
        return 1;
    }
    
    const char *server_name = argv[1];
    const char *port_name = argv[2];
    
    sockfd = setup_udp_socket(server_name, port_name);
    if (sockfd < 0) {
        fprintf(stderr, "Failed to setup UDP socket\n");
        return 1;
    }
    
    /* Main loop */
    while (1) {
        /* Setup file descriptor set for select() */
        FD_ZERO(&read_fds);
        
        /* Add stdin to set if not yet closed */
        if (!stdin_closed) {
            FD_SET(STDIN_FILENO, &read_fds);
        }
        
        /* Always monitor UDP socket */
        FD_SET(sockfd, &read_fds);
        
        /* Determine max fd for select() */
        max_fd = sockfd;
        if (!stdin_closed && STDIN_FILENO > max_fd) {
            max_fd = STDIN_FILENO;
        }
        
        /* Wait for data on either stdin or UDP socket */
        select_result = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        
        if (select_result < 0) {
            if (errno == EINTR) {
                continue;  /* Interrupted by signal, try again */
            }
            fprintf(stderr, "select error: %s\n", strerror(errno));
            close(sockfd);
            return 1;
        }
        
        /* Check if stdin has data available */
        if (!stdin_closed && FD_ISSET(STDIN_FILENO, &read_fds)) {
            bytes_read = read(STDIN_FILENO, stdin_buffer, STDIN_BUFFER_SIZE);
            
            if (bytes_read < 0) {
                fprintf(stderr, "read error on stdin: %s\n", strerror(errno));
                close(sockfd);
                return 1;
            }
            
            if (bytes_read == 0) {
                /* EOF on stdin - send empty UDP packet and mark stdin as closed */
                if (send(sockfd, "", 0, 0) < 0) {
                    fprintf(stderr, "send error (empty packet): %s\n", strerror(errno));
                    close(sockfd);
                    return 1;
                }

                stdin_closed = 1;
                
                /* If stdin closed, we only terminate when we receive empty packet */
                /* So continue monitoring UDP socket */
            } else {
                /* Send the data as UDP packet */
                if (send(sockfd, stdin_buffer, bytes_read, 0) < 0) {
                    fprintf(stderr, "send error: %s\n", strerror(errno));
                    close(sockfd);
                    return 1;
                }
            }
        }
        
        /* Check if UDP socket has data available */
        if (FD_ISSET(sockfd, &read_fds)) {
            bytes_received = recv(sockfd, udp_buffer, UDP_BUFFER_SIZE, 0);
            
            if (bytes_received < 0) {
                fprintf(stderr, "recv error: %s\n", strerror(errno));
                close(sockfd);
                return 1;
            }
            
            if (bytes_received == 0) {
                close(sockfd);
                return 0;
            }
            
            /* Write received data to stdout */
            if (better_write(STDOUT_FILENO, udp_buffer, bytes_received) < 0) {
                fprintf(stderr, "write error on stdout: %s\n", strerror(errno));
                close(sockfd);
                return 1;
            }
        }
    }
    
    /* Should never reach here */
    close(sockfd);
    return 0;
}