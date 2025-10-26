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

int main (int argc, char **argv){
    
    if (argc != 2){
        help(argv);
        exit(1);
    }
    

    char *port_name = argv[1];
    uint16_t port;

    if (convert_port_name(&port, port_name) != 0){
        fprintf(stderr, "Error: Invalid port number '%s'. Must be an integer between 0 and 65535.\n", argv[1]);
        exit(1);
    }

    int sockfd;
    struct sockaddr_in serv_addr;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "Error creating socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "Error binding socket: %s\n", strerror(errno));
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    char buffer[BUF_SIZE];
    struct sockaddr_storage sender_addr; // Use storage to hold sender's address
    socklen_t sender_addr_len = sizeof(sender_addr);

    // Main loop
    while (1) {
        ssize_t n_read = recvfrom(sockfd, buffer, BUF_SIZE, 0, (struct sockaddr *)&sender_addr, &sender_addr_len);
        
        if (n_read < 0) {
            fprintf(stderr, "Warning: recvfrom error: %s\n", strerror(errno));
            continue;
        }
        // DEBUG
        if (n_read < BUF_SIZE) {
            buffer[n_read] = '\0';
        } else {
            buffer[BUF_SIZE - 1] = '\0'; // Or handle oversize case
        }
        // DEBUG
        //printf("%s\n", buffer);

        // Send the received packet back to the sender 
        ssize_t n_sent = sendto(sockfd, buffer, n_read, 0, (struct sockaddr *)&sender_addr, sender_addr_len);
        //DEBUG
        //long ans = n_read - n_sent;
        //printf("%ld\n", ans);
        

        if (n_sent < 0) {
            fprintf(stderr, "Warning: sendto error: %s\n", strerror(errno));
        } else if (n_sent != n_read) {
            fprintf(stderr, "Warning: Partial send (%zd bytes of %zd)\n", n_sent, n_read);
        }
    }

    // Cleanup
    close(sockfd);
    return 0;
}