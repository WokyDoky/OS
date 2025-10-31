#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdint.h>

#define UDP_BUFFER_SIZE 65536
#define TCP_BUFFER_SIZE 65538 // UDP max + 2-byte header
#define RECONSTRUCTION_BUFFER_SIZE 131076 // 2×TCP buffer

/*
================================================================================
                            UTILITY
================================================================================
*/


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
 * @brief Creates and binds a UDP socket to the specified port
 * @param port The port number to bind to (host byte order).
 * @return The file descriptor for the UDP socket, or -1 on error.
 */
static int create_udp_socket(uint16_t port) {
    int sockfd;
    struct sockaddr_in addr;
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "Error creating UDP socket: %s\n", strerror(errno));
        return -1;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Error binding UDP socket to port %u: %s\n", 
                port, strerror(errno));
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

// void debug_printer(unsigned char *str){
//     int i = 0;
//     while (str[i]){
//         printf("%c\n", str[i]);
//         i++;
//     }
//     printf("END\n");
// }

/*
================================================================================
                            PACKET HANDLING
================================================================================
*/

/**
 * @brief Resolves server name, creates and connects a TCP socket.
 * @param server_name The FQDN or IP address of the server.
 * @param port_name The port number or service name (e.g., "8080").
 * @return The file descriptor for the connected TCP socket, or -1 on error.
 */
static int create_tcp_connection(const char *server_name, const char *port_name) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sockfd;
    int gai_code;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags = 0;
    
    gai_code = getaddrinfo(server_name, port_name, &hints, &result);
    if (gai_code != 0) {
        fprintf(stderr, "Error in getaddrinfo: %s\n", gai_strerror(gai_code));
        return -1;
    }
    
    /* Try each address until we successfully connect */
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd < 0) {
            continue;
        }
        
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break; /* Success */
        }
        
        close(sockfd);
    }
    
    if (rp == NULL) {
        fprintf(stderr, "Could not connect to %s:%s\n", server_name, port_name);
        freeaddrinfo(result);
        return -1;
    }
    
    freeaddrinfo(result);
    return sockfd;
}

/**
 * @brief Encapsulates and sends a UDP packet over TCP with a length header.
 *
 * This function prepends a 2-byte length header (in network byte order) to
 * the UDP payload and sends the complete message over the TCP connection.
 * The length header indicates the size of the UDP payload that follows.
 * Empty UDP packets (length 0) are supported and result in a 2-byte message.
 *
 * @param tcp_fd File descriptor of the TCP connection.
 * @param udp_data Buffer containing the UDP payload data.
 * @param udp_len Length of the UDP payload (0 to 65536 bytes).
 * @return 0 on success, -1 on error.
 */
static int send_udp_over_tcp(int tcp_fd, const unsigned char *udp_data, size_t udp_len) {
    unsigned char message[TCP_BUFFER_SIZE];
    uint16_t net_len;
    size_t total_len;
    
    /* Encapsulation */
    net_len = htons((uint16_t)udp_len);
    memcpy(message, &net_len, 2);
    
    if (udp_len > 0) {
        memcpy(message + 2, udp_data, udp_len);
    }
    
    total_len = 2 + udp_len;
    
    /* Send over TCP */
    if (better_write(tcp_fd, message, total_len) != 0) {
        fprintf(stderr, "Error writing to TCP connection: %s\n", strerror(errno));
        return -1;
    }
    
    return 0;
}


/*
--------------------------------------------------
MAIN
--------------------------------------------------
*/

int main(int argc, char *argv[]) {
    uint16_t udp_port;
    const char *tcp_server_name;
    const char *tcp_port_name;
    int udp_fd, tcp_fd;
    unsigned char udp_buffer[UDP_BUFFER_SIZE];
    unsigned char tcp_buffer[TCP_BUFFER_SIZE];
    unsigned char reconstruction_buffer[RECONSTRUCTION_BUFFER_SIZE];
    struct sockaddr_in udp_sender_addr;
    int udp_sender_available = 0;
    size_t recon_index = 0; // Position in reconstruction buffer
    int in_header = 1;
    uint16_t expected_payload_len = 0;
    fd_set readfds;
    int max_fd;
    
    
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <udp_port> <tcp_server_name> <tcp_port_name>\n", argv[0]);
        return 1;
    }
    
    if (convert_port_name(&udp_port, argv[1]) < 0) {
        fprintf(stderr, "Invalid UDP port: %s\n", argv[1]);
        return 1;
    }
    
    tcp_server_name = argv[2];
    tcp_port_name = argv[3];
    
    udp_fd = create_udp_socket(udp_port);
    if (udp_fd < 0) {
        return 1;
    }
    
    tcp_fd = create_tcp_connection(tcp_server_name, tcp_port_name);
    if (tcp_fd < 0) {
        close(udp_fd);
        return 1;
    }
    
    /* Select param */
    max_fd = (udp_fd > tcp_fd) ? udp_fd : tcp_fd;
    
    while (1) {
        /* Clear and setup */
        FD_ZERO(&readfds);
        FD_SET(udp_fd, &readfds);
        FD_SET(tcp_fd, &readfds);
        
        /* Wait for data */
        int ret = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (ret < 0) {
            /* Potentiall interrupt */
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "Error in select: %s\n", strerror(errno));
            break;
        }
        
        /* If UDP data ready */
        if (FD_ISSET(udp_fd, &readfds)) {
            socklen_t udp_sender_addr_len = sizeof(udp_sender_addr);
            ssize_t udp_recv_len = recvfrom(udp_fd, udp_buffer, UDP_BUFFER_SIZE, 0, (struct sockaddr *)&udp_sender_addr, &udp_sender_addr_len);
            
            // DEBUG
            // printf("DEBUG UDP: %s\n", udp_buffer);
            // debug_printer(udp_buffer);
            if (udp_recv_len < 0) {
                fprintf(stderr, "Error receiving UDP packet: %s\n", strerror(errno));
                continue;
            }
            
            /* we now know where to send UDP replies to */
            udp_sender_available = 1;
            
            /* Send over TCP with length header */
            if (send_udp_over_tcp(tcp_fd, udp_buffer, udp_recv_len) < 0) {
                break;
            }
        }
        
        /* If TCP data ready */
        if (FD_ISSET(tcp_fd, &readfds)) {
            ssize_t tcp_recv_len = read(tcp_fd, tcp_buffer, TCP_BUFFER_SIZE);
            
            if (tcp_recv_len < 0) {
                fprintf(stderr, "Error reading from TCP connection: %s\n", strerror(errno));
                break;
            }
            
            if (tcp_recv_len == 0) {
                /* EOF on TCP connection */
                break;
            }
            
            /* Process received TCP bytes */
            for (ssize_t i = 0; i < tcp_recv_len; i++) {
                reconstruction_buffer[recon_index++] = tcp_buffer[i];
                // DBEUG
                // printf("DEBUG T: %d\n", tcp_buffer[i]);
                // printf("DBEUG S: %s\n", reconstruction_buffer[recon_index]);
                if (in_header) { 
                    /* Still reading the 2-byte header */
                    if (recon_index == 2) {
                        /* Header complete, extract payload length */
                        uint16_t net_len;
                        memcpy(&net_len, reconstruction_buffer, 2); // uint16_t net_len = *(uint16_t *)reconstruction_buffer;
                        expected_payload_len = ntohs(net_len);
                        
                        if (expected_payload_len == 0) {
                            /* Empty message - send it out if we have a destination */
                            if (udp_sender_available) {
                                sendto(udp_fd, reconstruction_buffer + 2, 0, 0, 
                                    (struct sockaddr *)&udp_sender_addr, sizeof(udp_sender_addr));
                            }
                            recon_index = 0;
                            in_header = 1;
                        } else {
                            in_header = 0;
                        }
                    }
                } else {
                    /* Reading payload */
                    if ((uint16_t)recon_index == 2 + expected_payload_len) {
                        /* Message complete */
                        if (udp_sender_available) {
                            sendto(udp_fd, reconstruction_buffer + 2, expected_payload_len, 0, (struct sockaddr *)&udp_sender_addr, sizeof(udp_sender_addr));
                        }
                        
                        /* Reset for next message */
                        recon_index = 0;
                        in_header = 1;
                        expected_payload_len = 0;
                    }
                }
            }
        }
    }
    
    /* Cleanup */
    close(tcp_fd);
    close(udp_fd);
    
    return 0;
}