#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "defs.h"

void DieWithError(const char *errorMessage) // External error handling function
{
    perror(errorMessage);
    exit(1);
}

bool check_existing_customer(bank_t *bank, string ip, int portb, int portp) 
{
    for (int i = 0; i < bank->num_customers; i++) 
    {
        if (strcmp(bank->customers[i].customer_ip, ip) == 0) 
        {
            if (bank->customers[i].portb == portb || bank->customers[i].portp == portp) 
            {
                return true;
            }
        }
    }
    return false;
}

void handle_open()
{

}

void handle_new_cohort()
{

}

void handle_delete_cohort()
{

}

void handle_exit()
{

}

char** extract_words(char* str)
{
    char **words = malloc(MAX_COMMAND_WORDS * sizeof(char *));
    char *token = strtok(str, " ");
    int i = 0;
    while (token != NULL && i < MAX_COMMAND_WORDS) 
    {
        words[i] = malloc(MAX_WORD_LENGTH);
        strcpy(words[i], token);
        token = strtok(NULL, " ");
        i++;
    }
    return words;
}

int main(int argc, char *argv[]) 
{
    int socket_fd, bank_port;
    char buffer[BUFFERMAX + 1];
    struct sockaddr_in bank_addr, customer_addr;
    socklen_t customer_addr_len;
    int recieve_msg_size;                 // Size of received message
    int send_msg_size;                    // Size of sent message

    bank_t *bank;
    customer_t *customer;
    cohort_t *cohort;

    // check that the correct number of arguments were passed
    if (argc < 2) 
    {
        fprintf(stderr, "Usage: %s <UDP_bank_PORT>\n", argv[0]);
        exit(1);
    }

    // convert the port number from string to integer
    bank_port = atoi(argv[1]);

    // create a socket
    socket_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_fd < 0) 
    {
        DieWithError("bank: socket() failed");
    }

    // set up the bank address structure
    memset(&bank_addr, 0, sizeof(bank_addr)); // Zero out structure
    bank_addr.sin_family = AF_INET;           // Internet address family
    bank_addr.sin_addr.s_addr = INADDR_ANY;   // Any incoming interface
    bank_addr.sin_port = htons(bank_port);  // Local port

    // bind the socket to the bank address
    if (bind(socket_fd, (struct sockaddr *) &bank_addr, sizeof(bank_addr)) < 0) 
    {
        DieWithError("bank: bind() failed");
    }

	printf("bank: Port bank is listening to is: %d\n", bank_port);

    while(1) // Run forever
    { 
        customer_addr_len = sizeof(customer_addr);
        // Block until receive message from a customer
        recieve_msg_size = recvfrom(socket_fd, buffer, BUFFERMAX, 0, (struct sockaddr *)&customer_addr, &customer_addr_len);
        if(recieve_msg_size < 0 )
        {
            DieWithError("bank: recvfrom() failed");
        }
        buffer[recieve_msg_size] = '\0';
        printf("bank: received string `%s` from customer on IP address %s\n", buffer, inet_ntoa(customer_addr.sin_addr));

        char **args = extract_words(buffer);

        if(strcmp(args[0], "open") == 0)
        {
            handle_open();
        }
        else if(strcmp(args[0], "new-cohort") == 0)
        {
            handle_new_cohort();
        }
        else if(strcmp(args[0], "delete-cohort") == 0)
        {
            handle_delete_cohort();
        }
        else if(strcmp(args[0], "exit") == 0)
        {
            handle_exit();
        }
        else
        {
            printf("Try again with a vaild command!\n");
        }

        // Send received datagram back to the customer
        free(args);
        send_msg_size = sendto(socket_fd, buffer, strlen(buffer), 0, (struct sockaddr *)&customer_addr, sizeof(customer_addr));
        if(send_msg_size != strlen(buffer))
        {
            DieWithError("bank: sendto() sent a different number of bytes than expected");
        }
    }
    // NOT REACHED */
    return 0;
}