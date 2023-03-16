#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "defs.h"

// int sockfd;
new_cohort_response_t new_cohort_response; //global variable
pthread_mutex_t mutex; // Declare a mutex object

void *receive_and_respond_to_peers(void *arg)
{
    int sock_peer = *(int*)arg;
    char *peer_buffer_string = (char *)malloc(BUFFERMAX);
    struct sockaddr_in peer_fromAddr;
    unsigned int peer_fromSize = sizeof(peer_fromAddr);
    int peer_response_string_len;
    bool is_cohort_formed = false;
    while(1)
    {
        if((peer_response_string_len = recvfrom(sock_peer, peer_buffer_string, BUFFERMAX, 0, (struct sockaddr*)&peer_fromAddr, &peer_fromSize)) > BUFFERMAX)
        {
            printf("customer: recvfrom() failed\n");
        }
        peer_buffer_string[peer_response_string_len] = '\0';
        if(!is_cohort_formed)
        {
            deserialize_new_cohort_response(&new_cohort_response, peer_buffer_string);
            print_new_cohort_response(&new_cohort_response, MAX_COHORT_SIZE);
            // memcpy(&new_cohort_response, peer_buffer_string, sizeof(new_cohort_response_t));
            is_cohort_formed = true;
            peer_buffer_string = ACK;
            if(sendto(sock_peer, peer_buffer_string, strlen(peer_buffer_string), 0, (struct sockaddr *)&peer_fromAddr, sizeof(peer_fromAddr)) != strlen(peer_buffer_string))
            {
                printf("peer customer: sendto() sent a different number of bytes than expected\n");
            }
        }
    }
    // return NULL;
}

void *receive_and_respond_to_bank(void *arg)
{
    int sock = *(int*)arg;
    char *bank_buffer_string = NULL;
    size_t bank_buffer_string_len = BUFFERMAX;
    bank_buffer_string = (char *)malloc(BUFFERMAX);
    struct sockaddr_in bank_fromAddr;
    unsigned int bank_fromSize = sizeof(bank_fromAddr);
    int bank_response_string_len;
    while(1)
    {
        // pthread_mutex_lock(&mutex); // Lock the mutex before accessing shared resource
        bank_response_string_len = recvfrom(sock, bank_buffer_string, BUFFERMAX, 0, (struct sockaddr*)&bank_fromAddr, &bank_fromSize);//mutex lock
        // pthread_mutex_unlock(&mutex); // Unlock the mutex after accessing shared resource
        if(bank_response_string_len > BUFFERMAX)
        {
            printf("customer: recvfrom() failed\n");
        }
        bank_buffer_string[bank_response_string_len] = '\0';
        // printf("bank_buffer_string %s", bank_buffer_string);
        if(strcmp(bank_buffer_string, DELETE_COHORT) == 0)
        {
            bank_buffer_string = ACK;
            if(sendto(sock, bank_buffer_string, strlen(bank_buffer_string), 0, (struct sockaddr *)&bank_fromAddr, sizeof(bank_fromAddr)) != strlen(bank_buffer_string))
            {
                printf("peer customer: sendto() sent a different number of bytes than expected\n");
            }
        }
    }
    // return NULL;
}


int main(int argc, char *argv[]) 
{
    int sockfd;
    int bank_port, sockfd_peer;
    size_t nread;
    struct sockaddr_in bank_addr;
    struct sockaddr_in customer_addr, self_peer_addr;
    char buffer[BUFFERMAX];
    char *bank_IP;
    struct sockaddr_in fromAddr;            // Source address of echo
    unsigned int fromSize;                
    int response_string_len;                 // Length of received response
    char *buffer_string = NULL;             // String to send to echo bank
    size_t buffer_string_len = BUFFERMAX;   // Length of string to echo
    char* buffer_string_copy = NULL;
    char *buffer_string_new = NULL;

    buffer_string = (char *)malloc(BUFFERMAX);
    buffer_string_copy = (char *)malloc(BUFFERMAX);
    buffer_string_new = (char *)malloc(BUFFERMAX);

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
    printf("portb, portp for this customer is %d %d\n", customer_bank_portno, peer_portno);
   
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
    customer_addr.sin_addr.s_addr = INADDR_ANY;
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
    self_peer_addr.sin_addr.s_addr = INADDR_ANY;
    self_peer_addr.sin_port = htons(peer_portno);
    // bind the socket to the peer address
    if (bind(sockfd_peer, (struct sockaddr *)&self_peer_addr, sizeof(self_peer_addr)) < 0) 
    {
        perror("Error binding socket");
        exit(1);
    }

    // pthread_mutex_init(&mutex, NULL); // Initialize the mutex object
    pthread_t thread_id1, thread_id2;
    pthread_create(&thread_id1, NULL, receive_and_respond_to_peers, (void*)&sockfd_peer);
    pthread_create(&thread_id2, NULL, receive_and_respond_to_bank, (void*)&sockfd);

    // connect to the bank
    printf("customer: Echoing strings for %d iterations\n", ITERATIONS);

    for(int i = 0; i < ITERATIONS; i++)
    {
        printf("\nEnter string to echo: \n");
        if((nread = getline(&buffer_string, &buffer_string_len, stdin)) != -1)
        {
            pthread_cancel(thread_id2);
            buffer_string[(int)strlen(buffer_string) - 1 ] = '\0'; // Overwrite newline
            printf("\ncustomer: reads string ``%s''\n", buffer_string);
        }
        else
        {
            DieWithError("customer: error reading string to echo\n");
        }

        strcpy(buffer_string_copy, buffer_string);
        char **args = extract_words(buffer_string_copy);
        char* operation = args[0];

        // Send the string to the bank
        if(sendto(sockfd, buffer_string, strlen(buffer_string), 0, (struct sockaddr *)&bank_addr, sizeof(bank_addr)) != strlen(buffer_string))
        {
       		DieWithError("customer: sendto() sent a different number of bytes than expected");
        }

        // Receive a response
        fromSize = sizeof(fromAddr);
        // pthread_mutex_lock(&mutex); // Lock the mutex before accessing shared resource
        response_string_len = recvfrom(sockfd, buffer_string, BUFFERMAX, 0, (struct sockaddr *) &fromAddr, &fromSize);
        // pthread_mutex_unlock(&mutex); // Unlock the mutex after accessing shared resource
        if(response_string_len > BUFFERMAX) //mutex lock
        {
            DieWithError("customer: recvfrom() failed");
        }
        buffer_string[response_string_len] = '\0';
        if(strcmp(operation, NEW_COHORT) == 0)
        {
            pthread_cancel(thread_id1);
            char* customer_name = args[1];
            int cohort_size = atoi(args[2]);
            deserialize_new_cohort_response(&new_cohort_response, buffer_string);
            print_new_cohort_response(&new_cohort_response, MAX_COHORT_SIZE);
            if(new_cohort_response.response_code == FAILURE)
            {
                printf("New cohort operation failed for customer %s\n", customer_name);
            }
            else
            {
                int cohort_buffer_len = MAX_COHORT_SIZE * sizeof(customer_t) + sizeof(int);
                char* cohort_buffer_string = (char*) malloc(cohort_buffer_len);
                for(int i = 0; i < cohort_size; i++)
                {
                    if(strcmp(new_cohort_response.cohort_customers[i].name, customer_name))
                    {   
                        serialize_new_cohort_response(&new_cohort_response, cohort_buffer_string);
                        struct sockaddr_in cohort_customer_addr;
                        memset(&cohort_customer_addr, 0, sizeof(cohort_customer_addr)); // Zero out structure
                        cohort_customer_addr.sin_family = AF_INET;
                        cohort_customer_addr.sin_addr.s_addr = inet_addr(new_cohort_response.cohort_customers[i].customer_ip);
                        cohort_customer_addr.sin_port = htons(new_cohort_response.cohort_customers[i].portp);
                        if(sendto(sockfd_peer, cohort_buffer_string, strlen(cohort_buffer_string), 0, (struct sockaddr *)&cohort_customer_addr, sizeof(cohort_customer_addr)) != strlen(cohort_buffer_string))
                        {
                            printf("cohort customer: sendto() sent a different number of bytes than expected\n");
                        }
                        struct sockaddr_in cohort_fromAddr;
                        unsigned int cohort_fromSize = sizeof(cohort_fromAddr);
                        int cohort_respone_string_len;
                        char *buffer_string_cohort = NULL;
                        buffer_string_cohort = (char *)malloc(BUFFERMAX);
                        /*other customer recieves the packet and send ACK*/
                        /*waiting for ACK from other customers*/
                        if((cohort_respone_string_len = recvfrom(sockfd_peer, buffer_string_cohort, BUFFERMAX, 0, (struct sockaddr*)&cohort_fromAddr, &cohort_fromSize)) > BUFFERMAX)
                        {
                            printf("customer: recvfrom() failed\n");
                        }
                        buffer_string_cohort[cohort_respone_string_len] = '\0';
                        if(cohort_customer_addr.sin_addr.s_addr != cohort_fromAddr.sin_addr.s_addr)
                        {
                            printf("cohort customer: Error: received a packet from unknown source.\n");
                        }
                        if(strcmp(buffer_string_cohort, ACK)) // recieved ACK
                        {
                            printf("cohort customer: ACK: failed, cannot form a new cohort.\n");
                        }
                    }
                }
            }
            pthread_create(&thread_id1, NULL, receive_and_respond_to_peers, (void*)&sockfd_peer);
        }
        // buffer_string[response_string_len] = '\0';
        if(bank_addr.sin_addr.s_addr != fromAddr.sin_addr.s_addr)
        {
            DieWithError("customer: Error: received a packet from unknown source.\n");
        }
        // printf("buffer_string %s\n", buffer_string);
 		// printf("customer: received string '%s' from bank on IP address %s\n", buffer_string, inet_ntoa(fromAddr.sin_addr));
        pthread_create(&thread_id2, NULL, receive_and_respond_to_bank, (void*)&sockfd);
    }
    // pthread_mutex_destroy(&mutex); // Destroy the mutex object
    // close the sockets
    close(sockfd_peer);
    close(sockfd);
    return 0;
}
