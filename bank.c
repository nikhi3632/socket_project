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

int handle_open(bank_t* bank, string name, double balance, string ip_address, int portb, int portp)
{
    if (bank->num_customers >= MAX_CUSTOMERS) 
    {
        printf("Bank is full. Cannot open an account for %s.\n", name);
        return FAILURE;
    }
    else if(!is_valid_name(name))
    {
        printf("Name %s is more than 15 characters!\n", name);
        return FAILURE;
    }
    else if(!check_existing_customer(bank, name))
    {
        printf("Customer with name %s already exists!\n", name);
        return FAILURE;
    }
    else if(!are_ports_available(bank, ip_address, portb, portp))
    {
        printf("One or more ports in %d, %d is(are) already assigned!\n", portb, portp);
        return FAILURE;
    }
    else
    {
        customer_t new_customer = {
            .name = name,
            .balance = balance,
            .customer_ip = ip_address,
            .portb = portb,
            .portp = portp,
            .cohort_id = -1  // customer has not been assigned to any cohort yet
        };
        // Add the new customer to the bank's customer list
        bank->customers[bank->num_customers] = new_customer;
        bank->num_customers++;
        print_all_customers(bank);
        return SUCCESS;
    }   
}

int handle_new_cohort(bank_t* bank, string customer_name, int cohort_size, cohort_t* cohort)
{
    // Check if there are enough customers in the bank to form a cohort
    if (bank->num_customers < cohort_size) 
    {
        return FAILURE;
    }
    // Check if the customer already exists in the bank
    int customer_index = find_customer_index(bank, customer_name);
    if (customer_index == -1) 
    {
        printf("Customer doesn't exists in the bank database\n");
        return FAILURE;
    }
    // Create the cohort
    cohort->cohort_id = bank->num_cohorts++;
    cohort->size = cohort_size;
    // Add the customer forming the cohort to the cohort
    cohort->customers[0] = bank->customers[customer_index];
    // Randomly add remaining customers to the cohort
    int* random_customer_indices = generate_distinct_numbers(1, bank->num_customers, cohort_size - 1);
    int j = 0;
    for (int i = 1; i < cohort_size; i++) 
    {
        cohort->customers[i] = bank->customers[random_customer_indices[j++]];
    }
    free(random_customer_indices);
    bank->cohorts[bank->num_cohorts] = *cohort;
    /*TODO: Return and tuple of customer details and it to all the customers in the cohort*/
    return SUCCESS;
}

void handle_delete_cohort()
{
    /*TODO: The bank sends amessage to each member of the cohort containing the customer.
    This causes each customer in the cohort to delete checkpoints associated with the cohort and 
    send an acknowledgement of deletion to the bank and the bank recieves the acknowledgements for deletion from
    all the remaining customers in the cohort. Finally, the bank deletes the cohort from its database*/
}

int handle_exit(bank_t* bank, string customer_name)
{
    int customer_index = find_customer_index(bank, customer_name);
    if (customer_index == -1) 
    {
        printf("Customer doesn't exists in the bank database\n");
        return FAILURE;
    }
    // Remove the customer from the array
    for (int i = customer_index; i < bank->num_customers - 1; i++) {
        bank->customers[i] = bank->customers[i + 1];
    }
    bank->num_customers--;
    // Check if the customer was deleted successfully
    customer_index = find_customer_index(bank, customer_name);
    if (customer_index != -1) 
    {
        printf("Customer still exists and the exit is unsuccessfull from the bank database\n");
        return FAILURE;
    }
    return SUCCESS;
}

int main(int argc, char *argv[]) 
{
    int socket_fd, bank_port;
    char buffer[BUFFERMAX + 1];
    char buffer_copy[BUFFERMAX + 1];
    struct sockaddr_in bank_addr, customer_addr;
    socklen_t customer_addr_len;
    int recieve_msg_size;                 // Size of received message
    int send_msg_size;                    // Size of sent message

    bank_t bank_server;
    cohort_t cohort_;
    memset(&bank_server, 0, sizeof(bank_server));  // Zero out structure
    memset(&cohort_, 0, sizeof(cohort_));          // Zero out structure

    // check that the correct number of arguments were passed
    if (argc < 2) 
    {
        fprintf(stderr, "Usage: %s <UDP_bank_PORT>\n", argv[0]);
        exit(1);
    }

    // convert the port number from string to integer
    bank_port = atoi(argv[1]);
    bank_server.bank_port = bank_port;

    // create a socket
    socket_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_fd < 0) 
    {
        DieWithError("bank: socket() failed");
    }

    // set up the bank address structure
    memset(&bank_addr, 0, sizeof(bank_addr));  // Zero out structure
    bank_addr.sin_family = AF_INET;           // Internet address family
    bank_addr.sin_addr.s_addr = INADDR_ANY;   // Any incoming interface
    bank_addr.sin_port = htons(bank_port);    // Local port

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

        strcpy(buffer_copy, buffer);
        char **args = extract_words(buffer_copy);
        int buffer_words = countWords(buffer);
        
        if(buffer_words == 6 && strcmp(args[0], OPEN) == 0)
        {
            printf("Handling open for customer %s\n", args[1]);
            string customer_name = args[1];
            int customer_balance = atoi(args[2]);
            string customer_ip_address = args[3];
            int customer_portb = atoi(args[4]);
            int customer_portp = atoi(args[5]);
            handle_open(&bank_server, customer_name, customer_balance, 
                        customer_ip_address, customer_portb, customer_portp);
        }
        else if(buffer_words == 3 && strcmp(args[0], NEW_COHORT) == 0)
        {
            printf("Handling new cohort for customer %s\n", args[1]);
            string customer_name = args[1];
            int cohort_size = atoi(args[2]);
            handle_new_cohort(&bank_server, customer_name, cohort_size, &cohort_);
        }
        else if(buffer_words == 2 && strcmp(args[0], DELETE_COHORT) == 0)
        {
            string customer_name = args[1];
            // handle_delete_cohort(&bank_server, customer_name);
        }
        else if(buffer_words == 2 && strcmp(args[0], EXIT_BANK) == 0)
        {
            printf("Handling exit from the bank for customer %s\n", args[1]);
            string customer_name = args[1];
            handle_exit(&bank_server, customer_name);
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