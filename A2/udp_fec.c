#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MAX_UDP_PAYLOAD 65532
#define MAX_FEC_PAYLOAD 65536
#define DEDUP_ARRAY_SIZE 65536

/*
================================================================================
                            DATA STRUCTURES
================================================================================
*/

/* Packet waiting for repetition transmission */
typedef struct repetition_packet {
    unsigned char *data;
    size_t data_len;
    uint32_t packet_num;
    struct timespec next_send_time;
    int repetitions_left;
    struct repetition_packet *next;
} repetition_packet_t;

/* Packet waiting in reorder queue */
typedef struct reorder_packet {
    unsigned char *data;
    size_t data_len;
    uint32_t packet_num;
    struct timespec arrival_time;
    struct reorder_packet *next;
} reorder_packet_t;

/* Main program state */
typedef struct {
    /* Mode */
    int is_client;
    
    /* Sockets */
    int regular_udp_fd;
    int fec_udp_fd;

    /* MODIFICATION: Renamed booleans */
    int have_regular_peer_addr;
    int have_fec_peer_addr;
    
    /* Configuration */
    int repetition_factor;
    int repetition_delay_ms;
    int reordering_delay_ms;
    
    /* Packet counters */
    uint32_t send_packet_num;
    uint16_t recv_dedup_array[DEDUP_ARRAY_SIZE];
    
    /* Queues */
    repetition_packet_t *rep_queue_head;
    reorder_packet_t *reorder_queue_head;
    
    /* Address storage for regular UDP peer */
    struct sockaddr_storage regular_peer_addr;
    socklen_t regular_peer_addr_len;
    
    /* MODIFICATION: Added address storage for FEC UDP peer */
    struct sockaddr_storage fec_peer_addr;
    socklen_t fec_peer_addr_len;

    /* FEC peer address info (for client/server mode) */
    struct addrinfo *fec_peer_addrinfo;
} udp_fec_state_t;

/*
================================================================================
                            UTILITY
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
 * @return length of str. 
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
 ** @author Professor Dr. Christoph Lauter, UTEP.
 * @note This function was provided as part of the course materials for
 * Operating Systems Concepts and is not original work.
 */

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

/* Compare two timespecs: returns <0 if a<b, 0 if a==b, >0 if a>b */
static int timespec_compare(const struct timespec *a, const struct timespec *b) {
    if (a->tv_sec != b->tv_sec) {
        return (a->tv_sec < b->tv_sec) ? -1 : 1;
    }
    if (a->tv_nsec != b->tv_nsec) {
        return (a->tv_nsec < b->tv_nsec) ? -1 : 1;
    }
    return 0;
}

/* Add milliseconds to a timespec */
static void timespec_add_ms(struct timespec *ts, int ms) {
    ts->tv_sec += ms / 1000;
    ts->tv_nsec += (ms % 1000) * 1000000L;
    
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec += ts->tv_nsec / 1000000000L;
        ts->tv_nsec = ts->tv_nsec % 1000000000L;
    }
}

/* Calculate difference in milliseconds between two timespecs (a - b) */
static long timespec_diff_ms(const struct timespec *a, const struct timespec *b) {
    long sec_diff = a->tv_sec - b->tv_sec;
    long nsec_diff = a->tv_nsec - b->tv_nsec;
    return sec_diff * 1000 + nsec_diff / 1000000;
}

/*
================================================================================
                            SOCKET CREATION
================================================================================
*/

/* Create and bind a UDP socket to a specific port */
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

/* Create UDP socket and get address info (DOES NOT CONNECT) */
static int create_udp_with_addrinfo(const char *server_name, const char *port_name,
                                    struct addrinfo **result_out) {
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
    
    /* MODIFICATION: Removed the connect() call */
    
    if (result_out) {
        *result_out = result;
    } else {
        freeaddrinfo(result);
    }
    
    return sockfd;
}

/*
================================================================================
                            QUEUE MANAGEMENT
================================================================================
*/

/* Add packet to repetition queue */
static int queue_for_repetition(udp_fec_state_t *state, 
                                const unsigned char *data, 
                                size_t data_len,
                                uint32_t packet_num) {
    repetition_packet_t *pkt = malloc(sizeof(repetition_packet_t));
    if (!pkt) {
        fprintf(stderr, "Error: malloc failed for repetition packet\n");
        return -1;
    }
    
    pkt->data = malloc(data_len);
    if (!pkt->data) {
        fprintf(stderr, "Error: malloc failed for repetition packet data\n");
        free(pkt);
        return -1;
    }
    
    memcpy(pkt->data, data, data_len);
    pkt->data_len = data_len;
    pkt->packet_num = packet_num;
    pkt->repetitions_left = state->repetition_factor;
    
    /* Set next send time */
    clock_gettime(CLOCK_MONOTONIC, &pkt->next_send_time);
    timespec_add_ms(&pkt->next_send_time, state->repetition_delay_ms);
    
    /* Add to end of queue */
    pkt->next = NULL;
    if (!state->rep_queue_head) {
        state->rep_queue_head = pkt;
    } else {
        repetition_packet_t *curr = state->rep_queue_head;
        while (curr->next) {
            curr = curr->next;
        }
        curr->next = pkt;
    }
    
    return 0;
}

/* Add packet to reorder queue (sorted by packet number) */
static int queue_for_reordering(udp_fec_state_t *state,
                               const unsigned char *data,
                               size_t data_len,
                               uint32_t packet_num) {
    reorder_packet_t *pkt = malloc(sizeof(reorder_packet_t));
    if (!pkt) {
        fprintf(stderr, "Error: malloc failed for reorder packet\n");
        return -1;
    }
    
    pkt->data = malloc(data_len);
    if (!pkt->data) {
        fprintf(stderr, "Error: malloc failed for reorder packet data\n");
        free(pkt);
        return -1;
    }
    
    memcpy(pkt->data, data, data_len);
    pkt->data_len = data_len;
    pkt->packet_num = packet_num;
    clock_gettime(CLOCK_MONOTONIC, &pkt->arrival_time);
    
    /* Insert in sorted order by packet number */
    if (!state->reorder_queue_head || 
        packet_num < state->reorder_queue_head->packet_num) {
        pkt->next = state->reorder_queue_head;
        state->reorder_queue_head = pkt;
    } else {
        reorder_packet_t *curr = state->reorder_queue_head;
        while (curr->next && curr->next->packet_num < packet_num) {
            curr = curr->next;
        }
        pkt->next = curr->next;
        curr->next = pkt;
    }
    
    return 0;
}

/* Process repetition queue - send packets whose time has come */
static void process_repetition_queue(udp_fec_state_t *state) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    
    repetition_packet_t **curr = &state->rep_queue_head;
    
    while (*curr) {
        if (timespec_compare(&now, &(*curr)->next_send_time) >= 0) {
            /* Time to send this repetition */
            
            /* MODIFICATION: Replace send() with sendto() logic */
            ssize_t sent;
            if (state->is_client) {
                /* Client sends FEC to the server from args */
                sent = sendto(state->fec_udp_fd, (*curr)->data, (*curr)->data_len, 0,
                              state->fec_peer_addrinfo->ai_addr, 
                              state->fec_peer_addrinfo->ai_addrlen);
            } else {
                /* Server sends FEC reply to the FEC client it heard from */
                if (state->have_fec_peer_addr) {
                    sent = sendto(state->fec_udp_fd, (*curr)->data, (*curr)->data_len, 0,
                                  (struct sockaddr *)&state->fec_peer_addr, 
                                  state->fec_peer_addr_len);
                } else {
                    sent = -1; /* Can't send, don't know where yet */
                    errno = EDESTADDRREQ; 
                }
            }
            
            if (sent < 0) {
                fprintf(stderr, "Warning: send failed in repetition: %s\n", 
                       strerror(errno));
            }
            
            (*curr)->repetitions_left--;
            
            if ((*curr)->repetitions_left > 0) {
                /* Schedule next repetition */
                timespec_add_ms(&(*curr)->next_send_time, 
                               state->repetition_delay_ms);
                curr = &(*curr)->next;
            } else {
                /* Done with this packet, remove from queue */
                repetition_packet_t *to_free = *curr;
                *curr = (*curr)->next;
                free(to_free->data);
                free(to_free);
            }
        } else {
            curr = &(*curr)->next;
        }
    }
}

/* Process reorder queue - send packets in order or after timeout */
static void process_reorder_queue(udp_fec_state_t *state) {
    if (!state->reorder_queue_head) return;
    
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    
    /* Keep sending packets from front of queue while:
     * 1. They're the next expected packet in sequence, OR
     * 2. They've been waiting longer than reordering_delay_ms
     */
    while (state->reorder_queue_head) {
        reorder_packet_t *pkt = state->reorder_queue_head;
        
        /* Check if packet has timed out */
        long wait_time = timespec_diff_ms(&now, &pkt->arrival_time);
        int timed_out = (wait_time >= state->reordering_delay_ms);
        
        /* Always send the first packet if it timed out, or if we can send in order */
        /* For simplicity, we send packets in order from the front */
        if (timed_out || 1) {  /* Always send from front for now */
            
            /* MODIFICATION: Replace send() with sendto() logic */
            ssize_t sent;
            if (state->is_client) {
                /* Client sends regular reply to the original sender */
                if (state->have_regular_peer_addr) {
                    sent = sendto(state->regular_udp_fd, pkt->data, pkt->data_len, 0,
                                  (struct sockaddr *)&state->regular_peer_addr, 
                                  state->regular_peer_addr_len);
                } else {
                    sent = -1; /* Can't send, don't know where yet */
                    errno = EDESTADDRREQ; 
                }
            } else {
                /* Server sends regular packet to the destination from args */
                sent = sendto(state->regular_udp_fd, pkt->data, pkt->data_len, 0,
                              state->fec_peer_addrinfo->ai_addr, 
                              state->fec_peer_addrinfo->ai_addrlen);
            }

            if (sent < 0) {
                fprintf(stderr, "Warning: send failed in reorder: %s\n", 
                       strerror(errno));
            }
            
            state->reorder_queue_head = pkt->next;
            free(pkt->data);
            free(pkt);
        } else {
            break;
        }
    }
}

/*
================================================================================
                            PACKET HANDLING
================================================================================
*/

/* Handle reception of regular UDP packet - encode as FEC and queue for repetition */
static int handle_regular_udp_reception(udp_fec_state_t *state) {
    unsigned char buffer[MAX_UDP_PAYLOAD];
    unsigned char fec_buffer[MAX_FEC_PAYLOAD];
    struct sockaddr_storage sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    
    ssize_t n = recvfrom(state->regular_udp_fd, buffer, sizeof(buffer), 0,
                         (struct sockaddr *)&sender_addr, &addr_len);
    
    if (n < 0) {
        if (errno == EINTR) return 0;
        fprintf(stderr, "Error receiving from regular UDP socket: %s\n", 
               strerror(errno));
        return -1;
    }
    
    /* MODIFICATION: Removed connect() block */
    /* Always store/update the regular peer's address */
    memcpy(&state->regular_peer_addr, &sender_addr, addr_len);
    state->regular_peer_addr_len = addr_len;
    state->have_regular_peer_addr = 1;
    
    /* Create FEC packet: 4 byte header + payload */
    uint32_t net_pkt_num = htonl(state->send_packet_num);
    memcpy(fec_buffer, &net_pkt_num, 4);
    memcpy(fec_buffer + 4, buffer, n);
    size_t fec_len = n + 4;
    
    /* Send immediately */
    /* MODIFICATION: Replace send() with sendto() logic */
    ssize_t sent;
    if (state->is_client) {
        /* Client sends FEC to the server from args */
        sent = sendto(state->fec_udp_fd, fec_buffer, fec_len, 0,
                      state->fec_peer_addrinfo->ai_addr, 
                      state->fec_peer_addrinfo->ai_addrlen);
    } else {
        /* Server sends FEC reply to the FEC client it heard from */
        if (state->have_fec_peer_addr) {
            sent = sendto(state->fec_udp_fd, fec_buffer, fec_len, 0,
                          (struct sockaddr *)&state->fec_peer_addr, 
                          state->fec_peer_addr_len);
        } else {
            sent = -1; /* Can't send, don't know where yet */
            errno = EDESTADDRREQ; 
        }
    }
    
    if (sent < 0) {
        fprintf(stderr, "Warning: send failed on FEC socket: %s\n", 
               strerror(errno));
    }
    
    /* Queue for repetitions if needed */
    if (state->repetition_factor > 0) {
        if (queue_for_repetition(state, fec_buffer, fec_len, 
                                state->send_packet_num) < 0) {
            return -1;
        }
    }
    
    state->send_packet_num++;
    return 0;
}

/* Handle reception of FEC UDP packet - decode, deduplicate, and queue for reordering */
static int handle_fec_udp_reception(udp_fec_state_t *state) {
    unsigned char buffer[MAX_FEC_PAYLOAD];
    struct sockaddr_storage sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    
    ssize_t n = recvfrom(state->fec_udp_fd, buffer, sizeof(buffer), 0,
                         (struct sockaddr *)&sender_addr, &addr_len);
    
    if (n < 0) {
        if (errno == EINTR) return 0;
        fprintf(stderr, "Error receiving from FEC UDP socket: %s\n", 
               strerror(errno));
        return -1;
    }
    
    /* FEC packets must be at least 4 bytes (header) */
    if (n < 4) {
        fprintf(stderr, "Warning: received FEC packet too small (%zd bytes)\n", n);
        return 0;
    }
    
    /* MODIFICATION: Removed connect() block */
    /* Always store/update the FEC peer's address */
    memcpy(&state->fec_peer_addr, &sender_addr, addr_len);
    state->fec_peer_addr_len = addr_len;
    state->have_fec_peer_addr = 1;

    
    /* Extract packet number from header */
    uint32_t net_pkt_num;
    memcpy(&net_pkt_num, buffer, 4);
    uint32_t pkt_num = ntohl(net_pkt_num);
    
    /* Deduplication check */
    uint16_t hi = (uint16_t)(pkt_num >> 16);
    uint16_t lo = (uint16_t)(pkt_num & 0xFFFF);
    
    if (state->recv_dedup_array[lo] == hi) {
        /* Duplicate packet, discard */
        return 0;
    }
    
    /* Update deduplication array */
    state->recv_dedup_array[lo] = hi;
    
    /* Queue for reordering (payload starts after 4-byte header) */
    if (queue_for_reordering(state, buffer + 4, n - 4, pkt_num) < 0) {
        return -1;
    }
    
    return 0;
}

/*
================================================================================
                            MAIN LOOP
================================================================================
*/

/* Calculate next timeout for select based on queue states */
static int calculate_next_timeout(udp_fec_state_t *state, struct timeval *timeout) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    
    struct timespec min_time;
    int have_timeout = 0;
    
    /* Check repetition queue for next send time */
    repetition_packet_t *rep = state->rep_queue_head;
    while (rep) {
        if (!have_timeout || timespec_compare(&rep->next_send_time, &min_time) < 0) {
            min_time = rep->next_send_time;
            have_timeout = 1;
        }
        rep = rep->next;
    }
    
    /* Check reorder queue for timeout */
    reorder_packet_t *reorder = state->reorder_queue_head;
    if (reorder && state->reordering_delay_ms > 0) {
        struct timespec reorder_timeout = reorder->arrival_time;
        timespec_add_ms(&reorder_timeout, state->reordering_delay_ms);
        
        if (!have_timeout || timespec_compare(&reorder_timeout, &min_time) < 0) {
            min_time = reorder_timeout;
            have_timeout = 1;
        }
    }
    
    if (!have_timeout) {
        return 0; /* No timeout, wait indefinitely */
    }
    
    /* Calculate timeout relative to now */
    long diff_ms = timespec_diff_ms(&min_time, &now);
    if (diff_ms <= 0) {
        timeout->tv_sec = 0;
        timeout->tv_usec = 0;
    } else {
        timeout->tv_sec = diff_ms / 1000;
        timeout->tv_usec = (diff_ms % 1000) * 1000;
    }
    
    return 1;
}

/* Main event loop */
static int main_loop(udp_fec_state_t *state) {
    fd_set readfds;
    struct timeval timeout;
    int max_fd;
    
    max_fd = (state->regular_udp_fd > state->fec_udp_fd) 
             ? state->regular_udp_fd : state->fec_udp_fd;
    
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(state->regular_udp_fd, &readfds);
        FD_SET(state->fec_udp_fd, &readfds);
        
        int have_timeout = calculate_next_timeout(state, &timeout);
        
        int ret = select(max_fd + 1, &readfds, NULL, NULL, 
                        have_timeout ? &timeout : NULL);
        
        if (ret < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "Error in select: %s\n", strerror(errno));
            return -1;
        }
        
        /* Timeout - process queues */
        if (ret == 0) {
            process_repetition_queue(state);
            process_reorder_queue(state);
            continue;
        }
        
        /* Handle regular UDP reception */
        if (FD_ISSET(state->regular_udp_fd, &readfds)) {
            if (handle_regular_udp_reception(state) < 0) {
                return -1;
            }
        }
        
        /* Handle FEC UDP reception */
        if (FD_ISSET(state->fec_udp_fd, &readfds)) {
            if (handle_fec_udp_reception(state) < 0) {
                return -1;
            }
        }
        
        /* Process queues after reception */
        process_repetition_queue(state);
        process_reorder_queue(state);
    }
    
    return 0;
}

/*
================================================================================
                            INITIALIZATION & CLEANUP
================================================================================
*/

/* Initialize state structure */
static void init_state(udp_fec_state_t *state) {
    memset(state, 0, sizeof(udp_fec_state_t));
    state->regular_udp_fd = -1;
    state->fec_udp_fd = -1;
    
    /* MODIFICATION: Use new names */
    state->have_regular_peer_addr = 0;
    state->have_fec_peer_addr = 0;

    state->send_packet_num = 0;
    state->rep_queue_head = NULL;
    state->reorder_queue_head = NULL;
    state->fec_peer_addrinfo = NULL;
    
    /* Initialize deduplication array to a value that won't match initially */
    memset(state->recv_dedup_array, 0xFF, sizeof(state->recv_dedup_array));
}

/* Clean up all resources */
static void cleanup_state(udp_fec_state_t *state) {
    /* Close sockets */
    if (state->regular_udp_fd >= 0) {
        close(state->regular_udp_fd);
    }
    if (state->fec_udp_fd >= 0) {
        close(state->fec_udp_fd);
    }
    
    /* Free repetition queue */
    while (state->rep_queue_head) {
        repetition_packet_t *next = state->rep_queue_head->next;
        free(state->rep_queue_head->data);
        free(state->rep_queue_head);
        state->rep_queue_head = next;
    }
    
    /* Free reorder queue */
    while (state->reorder_queue_head) {
        reorder_packet_t *next = state->reorder_queue_head->next;
        free(state->reorder_queue_head->data);
        free(state->reorder_queue_head);
        state->reorder_queue_head = next;
    }
    
    /* Free address info */
    if (state->fec_peer_addrinfo) {
        freeaddrinfo(state->fec_peer_addrinfo);
    }
}

/* Parse and validate integer argument */
static int parse_int_arg(const char *arg, int min, int max, int *result) {
    char *end;
    long val = strtol(arg, &end, 10);
    
    if (*end != '\0' || val < min || val > max) {
        return -1;
    }
    
    *result = (int)val;
    return 0;
}

/*
================================================================================
                            MAIN
================================================================================
*/

static void print_usage(const char *progname) {
    print_to_fd(STDOUT_FILENO, "Usage:\n");
    print_to_fd(STDOUT_FILENO, "Client mode: ");
    print_to_fd(STDOUT_FILENO, progname);
    print_to_fd(STDOUT_FILENO, " -c <udp_port> <server_name> <tcp_port_name> <repetition_factor> <repetition_delay_ms> <reordering_delay_ms>\n");
    print_to_fd(STDOUT_FILENO, "Server mode: ");
    print_to_fd(STDOUT_FILENO, progname);
    print_to_fd(STDOUT_FILENO, " -c <tcp_port> <server_name> <udp_port_name> <repetition_factor> <repetition_delay_ms> <reordering_delay_ms>\n\n");
    print_to_fd(STDOUT_FILENO, "repetition_factor: 0-7\n");
    print_to_fd(STDOUT_FILENO, "repetition_delay_ms: 0-1000 (ms)\n");
    print_to_fd(STDOUT_FILENO, "reordering_delay_ms: 0-8000 (ms)\n");
}

int main(int argc, char *argv[]) {
    udp_fec_state_t state;
    int is_client;
    uint16_t bind_port;
    const char *peer_name;
    const char *peer_port;
    int repetition_factor, repetition_delay, reordering_delay;
    
    if (argc < 8) {
        print_usage(argv[0]);
        return 1;
    }
    
    if (strcmp(argv[1], "-c") == 0) {
        is_client = 1;
    } else if (strcmp(argv[1], "-s") == 0) {
        is_client = 0;
    } else {
        fprintf(stderr, "Error: First argument must be -c (client) or -s (server)\n");
        print_usage(argv[0]);
        return 1;
    }
    
    /* Parse bind port */
    if (convert_port_name(&bind_port, argv[2]) < 0) {
        fprintf(stderr, "Error: Invalid port number '%s'\n", argv[2]);
        return 1;
    }
    
    peer_name = argv[3];
    peer_port = argv[4];
    
    /* Parse repetition factor (0-7) */
    if (parse_int_arg(argv[5], 0, 7, &repetition_factor) < 0) {
        fprintf(stderr, "Error: repetition_factor must be 0-7, got '%s'\n", argv[5]);
        return 1;
    }
    
    /* Parse repetition delay (0-1000 ms) */
    if (parse_int_arg(argv[6], 0, 1000, &repetition_delay) < 0) {
        fprintf(stderr, "Error: repetition_delay_ms must be 0-1000, got '%s'\n", argv[6]);
        return 1;
    }
    
    /* Parse reordering delay (0-8000 ms) */
    if (parse_int_arg(argv[7], 0, 8000, &reordering_delay) < 0) {
        fprintf(stderr, "Error: reordering_delay_ms must be 0-8000, got '%s'\n", argv[7]);
        return 1;
    }
    
    /* Initialize state */
    init_state(&state);
    state.is_client = is_client;
    state.repetition_factor = repetition_factor;
    state.repetition_delay_ms = repetition_delay;
    state.reordering_delay_ms = reordering_delay;
    
    /* Setup sockets based on mode */
    if (is_client) {
        /* Client mode: bind regular UDP port, get FEC server info */
        state.regular_udp_fd = create_udp_socket(bind_port);
        if (state.regular_udp_fd < 0) {
            cleanup_state(&state);
            return 1;
        }
        
        /* MODIFICATION: Get addrinfo for FEC server */
        state.fec_udp_fd = create_udp_with_addrinfo(peer_name, peer_port, 
                                                    &state.fec_peer_addrinfo);
        if (state.fec_udp_fd < 0) {
            cleanup_state(&state);
            return 1;
        }
        /* MODIFICATION: Removed state.fec_connected = 1; */
        
    } else {
        /* Server mode: bind FEC UDP port, get regular UDP server info */
        state.fec_udp_fd = create_udp_socket(bind_port);
        if (state.fec_udp_fd < 0) {
            cleanup_state(&state);
            return 1;
        }
        
        /* MODIFICATION: Get addrinfo for regular server */
        state.regular_udp_fd = create_udp_with_addrinfo(peer_name, peer_port, 
                                                        &state.fec_peer_addrinfo);
        if (state.regular_udp_fd < 0) {
            cleanup_state(&state);
            return 1;
        }
        /* MODIFICATION: Removed state.regular_connected = 1; */
    }
    
    /* Run main loop */
    int result = main_loop(&state);
    
    /* Cleanup */
    cleanup_state(&state);
    
    return (result < 0) ? 1 : 0;
}
