

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define UDP_RECV_BUFFER_SIZE 65536
#define TCP_RECV_BUFFER_SIZE 65538
#define RECONSTRUCTION_BUFFER_SIZE 131076

/**
 ** @author Professor Dr. Christoph Lauter, UTEP.
 * @note This function was provided as part of the course materials for
 * Operating Systems Concepts and is not original work.
 */

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
 * @brief Creates, configures, and binds a TCP listening socket.
 * @details This function creates an AF_INET, SOCK_STREAM socket, sets the
 * SO_REUSEADDR option to allow quick server restarts, and binds it
 * to all available interfaces (INADDR_ANY) on the specified port.
 * It does not call listen().
 * @param port The port number (in host byte order) to bind the socket to.
 * @return The file descriptor for the bound socket on success, or -1 on failure.
 */
static int create_tcp_listen_socket(uint16_t port) {
    int sockfd;
    struct sockaddr_in server_addr;
    int reuse = 1;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "Error creating TCP socket: %s\n", strerror(errno));
        return -1;
    }
    
    /* Allow port reuse */
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        fprintf(stderr, "Error setting SO_REUSEADDR: %s\n", strerror(errno));
        close(sockfd);
        return -1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "Error binding TCP socket: %s\n", strerror(errno));
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

/**
 * @brief Resolves a server address, creates a UDP socket, and connects to it.
 * @details This function uses getaddrinfo to resolve the server and port
 * for a UDP (SOCK_DGRAM) socket. It then creates the socket and
 * "connects" it to the server's address.
 * @param server_name The hostname or IP address of the UDP server.
 * @param port_name The port number or service name of the UDP server.
 * @param result_out [out] A pointer to a struct addrinfo pointer. On success,
 * this will be populated with the address info used,
 * which must be freed by the caller using freeaddrinfo().
 * @return The file descriptor for the connected UDP socket on success, or -1 on failure.
 */
static int create_udp_socket(const char *server_name, const char *port_name, struct addrinfo **result_out) {
    struct addrinfo hints;
    struct addrinfo *result;
    int gai_code;
    int sockfd;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = 0;
    hints.ai_flags = 0;
    
    gai_code = getaddrinfo(server_name, port_name, &hints, &result);
    if (gai_code != 0) {
        fprintf(stderr, "Error in getaddrinfo: %s\n", gai_strerror(gai_code));
        return -1;
    }
    
    sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sockfd < 0) {
        fprintf(stderr, "Error creating UDP socket: %s\n", strerror(errno));
        freeaddrinfo(result);
        return -1;
    }
    
    if (connect(sockfd, result->ai_addr, result->ai_addrlen) < 0) {
        fprintf(stderr, "Error connecting UDP socket: %s\n", strerror(errno));
        close(sockfd);
        freeaddrinfo(result);
        return -1;
    }
    
    *result_out = result;
    return sockfd;
}

/**
 * @brief Reads one UDP datagram, frames it, and sends it over a TCP connection.
 * @details This function reads a single packet from the UDP socket. It then
 * prepends a 2-byte length header (in network byte order) to the
 * packet's payload. This new framed message (2-byte length + payload)
 * is then reliably sent over the TCP socket using better_write().
 * @param udp_fd The file descriptor for the (connected) UDP socket to read from.
 * @param tcp_fd The file descriptor for the TCP socket to write to.
 * @return 0 on success, -1 on failure (e.g., read or write error).
 * Returns 0 on EINTR during recv.
 */
static int process_udp_to_tcp(int udp_fd, int tcp_fd) {
    unsigned char udp_buffer[UDP_RECV_BUFFER_SIZE];
    unsigned char message[UDP_RECV_BUFFER_SIZE + 2];
    ssize_t udp_bytes;
    uint16_t length_net;
    
    udp_bytes = recv(udp_fd, udp_buffer, UDP_RECV_BUFFER_SIZE, 0);
    if (udp_bytes < 0) {
        if (errno == EINTR) {
            return 0;
        }
        fprintf(stderr, "Error receiving from UDP socket: %s\n", strerror(errno));
        return -1;
    }
    
    /* Create message: 2 bytes length + UDP payload */
    length_net = htons((uint16_t)udp_bytes);
    memcpy(message, &length_net, 2);
    if (udp_bytes > 0) {
        memcpy(message + 2, udp_buffer, udp_bytes);
    }
    
    /* Send over TCP */
    if (better_write(tcp_fd, message, udp_bytes + 2) != 0) {
        fprintf(stderr, "Error writing to TCP socket: %s\n", strerror(errno));
        return -1;
    }
    
    return 0;
}

/**
 * @brief Reads from a TCP stream, reconstructs framed messages, and sends them over UDP.
 * @param tcp_fd The file descriptor for the TCP socket to read from.
 * @param udp_fd The file descriptor for the (connected) UDP socket to send to.
 * @param reconstruction_buffer [in/out] A buffer used to store partial
 * messages between TCP reads.
 * @param recon_index [in/out] A pointer to the current write position
 * in the reconstruction buffer.
 * @param expected_payload_length [in/out] A pointer to a variable storing
 * the expected payload length of the
 * current message being parsed.
 * @param in_header_mode [in/out] A pointer to a flag (1 or 0) indicating
 * if the state machine is currently parsing a
 * header (1) or a payload (0).
 * @return 0 on success (data processed, more expected),
 * 1 on EOF (connection closed by peer),
 * -1 on a fatal error.
 */
static int process_tcp_to_udp(int tcp_fd, int udp_fd, unsigned char *reconstruction_buffer, size_t *recon_index, uint16_t *expected_payload_length, int *in_header) {
    unsigned char tcp_buffer[TCP_RECV_BUFFER_SIZE];
    ssize_t tcp_bytes;
    size_t i;
    
    tcp_bytes = read(tcp_fd, tcp_buffer, TCP_RECV_BUFFER_SIZE);
    if (tcp_bytes < 0) {
        if (errno == EINTR) {
            return 0;
        }
        fprintf(stderr, "Error reading from TCP socket: %s\n", strerror(errno));
        return -1;
    }
    
    if (tcp_bytes == 0) {
        /* EOF on TCP connection */
        return 1;
    }
    
    /* Process each byte from TCP buffer */
    for (i = 0; i < (size_t)tcp_bytes; i++) {
        reconstruction_buffer[*recon_index] = tcp_buffer[i];
        (*recon_index)++;
        
        if (*in_header) {
            /* We're still reading the 2-byte header */
            if (*recon_index == 2) {
                /* Header complete, extract length */
                uint16_t length_net;
                memcpy(&length_net, reconstruction_buffer, 2);
                *expected_payload_length = ntohs(length_net);
                
                if (*expected_payload_length == 0) {
                    /* Empty message, send it out immediately */
                    if (send(udp_fd, NULL, 0, 0) < 0) {
                        fprintf(stderr, "Error sending empty UDP packet: %s\n", strerror(errno));
                        return -1;
                    }
                    *recon_index = 0;
                    *in_header = 1;
                } else {
                    /* Switch to payload mode */
                    *in_header = 0;
                }
            }
        } else {
            /* We're reading the payload */
            if (*recon_index == (size_t)(*expected_payload_length + 2)) {
                /* Complete message received, send over UDP */
                if (send(udp_fd, reconstruction_buffer + 2, *expected_payload_length, 0) < 0) {
                    fprintf(stderr, "Error sending UDP packet: %s\n", strerror(errno));
                    return -1;
                }
                
                /* Reset for next message */
                *recon_index = 0;
                *in_header = 1;
                *expected_payload_length = 0;
            }
        }
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    uint16_t tcp_port;
    const char *udp_server_name;
    const char *udp_port_name;
    int tcp_listen_fd = -1;
    int tcp_conn_fd = -1;
    int udp_fd = -1;
    struct addrinfo *udp_result = NULL;
    
    /* State for TCP message reconstruction */
    unsigned char reconstruction_buffer[RECONSTRUCTION_BUFFER_SIZE];
    size_t recon_index = 0;
    uint16_t expected_payload_length = 0;
    int in_header_mode = 1;
    
    /* Check arguments */
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <TCP_port> <UDP_server_name> <UDP_port_name>\n", argv[0]);
        return 1;
    }
    
    /* Parse TCP port */
    if (convert_port_name(&tcp_port, argv[1]) < 0) {
        fprintf(stderr, "Error: Invalid TCP port '%s'\n", argv[1]);
        return 1;
    }
    
    udp_server_name = argv[2];
    udp_port_name = argv[3];
    
    /* Create TCP listening socket */
    tcp_listen_fd = create_tcp_listen_socket(tcp_port);
    if (tcp_listen_fd < 0) {
        return 1;
    }
    
    /* Create and connect UDP socket */
    udp_fd = create_udp_socket(udp_server_name, udp_port_name, &udp_result);
    if (udp_fd < 0) {
        close(tcp_listen_fd);
        return 1;
    }
    
    /* Listen for TCP connections */
    if (listen(tcp_listen_fd, 1) < 0) {
        fprintf(stderr, "Error listening on TCP socket: %s\n", strerror(errno));
        close(tcp_listen_fd);
        close(udp_fd);
        freeaddrinfo(udp_result);
        return 1;
    }
    
    /* Accept TCP connection */
    tcp_conn_fd = accept(tcp_listen_fd, NULL, NULL);
    if (tcp_conn_fd < 0) {
        fprintf(stderr, "Error accepting TCP connection: %s\n", strerror(errno));
        close(tcp_listen_fd);
        close(udp_fd);
        freeaddrinfo(udp_result);
        return 1;
    }
    
    /* Main loop: use select to multiplex between TCP and UDP */
    while (1) {
        fd_set read_fds;
        int max_fd;
        int select_result;
        
        FD_ZERO(&read_fds);
        FD_SET(tcp_conn_fd, &read_fds);
        FD_SET(udp_fd, &read_fds);
        
        max_fd = (tcp_conn_fd > udp_fd) ? tcp_conn_fd : udp_fd;
        
        select_result = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (select_result < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "Error in select: %s\n", strerror(errno));
            break;
        }
        
        /* Check if UDP socket has data */
        if (FD_ISSET(udp_fd, &read_fds)) {
            if (process_udp_to_tcp(udp_fd, tcp_conn_fd) < 0) {
                break;
            }
        }
        
        /* Check if TCP connection has data */
        if (FD_ISSET(tcp_conn_fd, &read_fds)) {
            int result = process_tcp_to_udp(tcp_conn_fd, udp_fd, 
                                           reconstruction_buffer,
                                           &recon_index,
                                           &expected_payload_length,
                                           &in_header_mode);
            if (result < 0) {
                break;
            }
            if (result > 0) {
                /* EOF on TCP connection, terminate normally */
                break;
            }
        }
    }
    
    /* Cleanup */
    if (tcp_conn_fd >= 0) {
        close(tcp_conn_fd);
    }
    if (tcp_listen_fd >= 0) {
        close(tcp_listen_fd);
    }
    if (udp_fd >= 0) {
        close(udp_fd);
    }
    if (udp_result != NULL) {
        freeaddrinfo(udp_result);
    }
    
    return 0;
}