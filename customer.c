#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "defs.h"

int main(int argc, char *argv[]) 
{
    int sockfd, bank_port, sockfd_peer;
    size_t nread;
    struct sockaddr_in bank_addr;
    struct sockaddr_in customer_addr, self_peer_addr;
    char buffer[BUFFERMAX + 1];
    char *bank_IP;
    struct sockaddr_in fromAddr;            // Source address of echo
    unsigned int fromSize;                  // In-out of address size for recvfrom()
    int respone_string_len;                 // Length of received response
    char *buffer_string = NULL;             // String to send to echo bank
    size_t buffer_string_len = BUFFERMAX;   // Length of string to echo

    // check that the correct number of arguments were passed
    if (argc < 5) 
    {
        fprintf(stderr, "Usage: %s <bank_IP> <UDP_bank_PORT> <UDP_customer_PORT> <UDP_COMMUNICATION_PORT>\n", argv[0]);
        exit(1);
    }

    // convert the port numbers from strings to integers
    bank_IP = argv[1];
    bank_port = atoi(argv[2]);
    int customer_bank_portno = atoi(argv[3]);
    int peer_portno = atoi(argv[4]);

    printf("customer: Arguments passed: bank IP %s, port %d\n", bank_IP, bank_port);
    printf("portp for this customer is %d\n", customer_bank_portno);

   
    // Set up the bank address struct
    memset(&bank_addr, 0, sizeof(bank_addr));
    bank_addr.sin_family = AF_INET;
    bank_addr.sin_addr.s_addr = inet_addr(bank_IP);
    bank_addr.sin_port = htons(bank_port);

     // create a socket
    sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) 
    {
        DieWithError("customer: socket() failed");
    }
    // Set up the customer address struct
    memset(&customer_addr, 0, sizeof(customer_addr)); // Zero out structure
    customer_addr.sin_family = AF_INET;
    customer_addr.sin_addr.s_addr = inet_addr(argv[1]);
    customer_addr.sin_port = htons(customer_bank_portno);
    // bind the socket to the customer address
    if (bind(sockfd, (struct sockaddr *)&customer_addr, sizeof(customer_addr)) < 0) 
    {
        perror("Error binding socket");
        exit(1);
    }

    sockfd_peer = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd_peer < 0) 
    {
        DieWithError("peer: socket() failed");
    }
    // Set up the self to peer address struct
    memset(&self_peer_addr, 0, sizeof(self_peer_addr)); // Zero out structure
    self_peer_addr.sin_family = AF_INET;
    self_peer_addr.sin_addr.s_addr = inet_addr(argv[1]);
    self_peer_addr.sin_port = htons(peer_portno);
    // bind the socket to the peer address
    if (bind(sockfd_peer, (struct sockaddr *)&self_peer_addr, sizeof(self_peer_addr)) < 0) 
    {
        perror("Error binding socket");
        exit(1);
    }

    // connect to the bank
    printf("customer: Echoing strings for %d iterations\n", ITERATIONS);

    for(int i = 0; i < ITERATIONS; i++)
    {
        printf("\nEnter string to echo: \n");
        if((nread = getline(&buffer_string, &buffer_string_len, stdin)) != -1)
        {
            buffer_string[(int)strlen(buffer_string) - 1 ] = '\0'; // Overwrite newline
            printf("\ncustomer: reads string ``%s''\n", buffer_string);
        }
        else
        {
            DieWithError("customer: error reading string to echo\n");
        }

        // Send the string to the bank
        if(sendto(sockfd, buffer_string, strlen(buffer_string), 0, (struct sockaddr *)&bank_addr, sizeof(bank_addr)) != strlen(buffer_string))
        {
       		DieWithError("customer: sendto() sent a different number of bytes than expected");
        }

        // Receive a response
        fromSize = sizeof(fromAddr);

        if((respone_string_len = recvfrom(sockfd, buffer_string, BUFFERMAX, 0, (struct sockaddr *) &fromAddr, &fromSize)) > BUFFERMAX)
        {
            DieWithError("customer: recvfrom() failed");
        }

        buffer_string[respone_string_len] = '\0';

        if(bank_addr.sin_addr.s_addr != fromAddr.sin_addr.s_addr)
        {
            DieWithError("customer: Error: received a packet from unknown source.\n");
        }
 		printf("customer: received string ``%s'' from bank on IP address %s\n", buffer_string, inet_ntoa(fromAddr.sin_addr));
    }
    // close the socket
    close(sockfd);

    return 0;
}
