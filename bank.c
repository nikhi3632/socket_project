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
    else if(check_existing_customer(bank, name))
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

new_cohort_response_t handle_new_cohort(bank_t* bank, string customer_name, int cohort_size) 
{
    new_cohort_response_t resp;
    memset(&resp, 0, sizeof(new_cohort_response_t));  // Zero out structure
    // Check if the customer already exists in the bank
    resp.response_code = FAILURE;
    int customer_index = find_customer_index(bank, customer_name);
    if (customer_index == -1) 
    {
        printf("Customer doesn't exists in the bank database\n");
        return resp;
    }
    // Check if there are enough customers in the bank to form a cohort
    if (cohort_size < 2 || cohort_size > MAX_COHORT_SIZE) 
    {
        // Invalid cohort size
        return resp;
    }
    if (customer_index >= bank->num_customers) 
    {
        // Invalid customer index
        return resp;
    }
    if (bank->customers[customer_index].cohort_id != -1) 
    {
        // Customer is already in a cohort
        return resp;
    }
    if (bank->num_customers < cohort_size) 
    {
        // Insufficient customers in the bank
        return resp;
    }
    // Create a list of candidate customers (excluding the current customer)
    int candidate_customers[MAX_CUSTOMERS];
    int num_candidates = 0;
    for (int i = 0; i < bank->num_customers; i++) 
    {
        if (i != customer_index && bank->customers[i].cohort_id == -1) 
        {
            candidate_customers[num_candidates] = i;
            num_candidates++;
        }
    }
    if (num_candidates < cohort_size - 1) 
    {
        // Insufficient eligible customers
        return resp;
    }
    // Randomly select n-1 customers from the candidate list
    int selected_customers[cohort_size-1];
    for (int i = 0; i < cohort_size-1; i++)
    {
        int j = rand() % num_candidates;
        selected_customers[i] = candidate_customers[j];
        candidate_customers[j] = candidate_customers[num_candidates-1];
        num_candidates--;
    }
    cohort_t cohort = {0};
    // Add the current customer to the selected customers to form the cohort
    bank->num_cohorts++;
    cohort.cohort_id = bank->num_cohorts;
    printf("Cohort ID: %d\n", cohort.cohort_id);
    cohort.is_deleted = false;
    cohort.size = cohort_size;

    bank->customers[customer_index].cohort_id = cohort.cohort_id;
    cohort.customers[0] = bank->customers[customer_index];
    // Update the cohort IDs of the selected customers
    for (int i = 0; i < cohort_size-1; i++) 
    {
        bank->customers[selected_customers[i]].cohort_id = cohort.cohort_id;
    }

    // Update the cohort ID of the current customer
   
    int j = 0;
    for (int i = 1; i < cohort_size; i++) 
    {
        cohort.customers[i] = bank->customers[selected_customers[j++]];
    }

    // Get the new cohort
    bank->cohorts[bank->num_cohorts-1] = cohort;
    // Prepare the response
    resp.response_code = SUCCESS;
    for (int i = 0; i < cohort_size; i++) 
    {
        resp.cohort_customers[i] = cohort.customers[i];
    }
    // print_customers_by_cohort(bank);
    print_new_cohort_response(&resp, cohort_size);
    return resp;
}

int handle_delete_cohort(bank_t *bank, string customer_name)
{
    for (int i = 0; i < bank->num_cohorts; i++) 
    {
        cohort_t *cohort_info = &bank->cohorts[i];
        for(int j = 0; j < cohort_info->size; j++)
        {
            if(strcmp(cohort_info->customers[j].name, customer_name) == 0)
            {
                cohort_info->is_deleted = true;
                print_customers_by_cohort(bank);
                return SUCCESS;
            }
        }
    }
    return FAILURE;
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
    print_all_customers(bank);
    return SUCCESS;
}

int main(int argc, char *argv[]) 
{
    int socket_fd, bank_port;
    char buffer[BUFFERMAX];
    char buffer_copy[BUFFERMAX];
    struct sockaddr_in bank_addr, customer_addr;
    socklen_t customer_addr_len;
    int recieve_msg_size;                 // Size of received message
    int send_msg_size;                    // Size of sent message

    bank_t bank_server;
    new_cohort_response_t new_cohort_response_;
    memset(&bank_server, 0, sizeof(bank_server));                           // Zero out structure
    memset(&new_cohort_response_, 0, sizeof(new_cohort_response_));          // Zero out structure

    // check that the correct number of arguments were passed
    if (argc < 2) 
    {
        fprintf(stderr, "Usage: %s <UDP_bank_PORT>\n", argv[0]);
        exit(1);
    }

    // convert the port number from string to integer
    bank_port = atoi(argv[1]);
    bank_server.bank_port = bank_port;
    bank_server.num_cohorts = 0;
    bank_server.num_customers = 0;

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
            new_cohort_response_ = handle_new_cohort(&bank_server, customer_name, cohort_size);
            int buffer_len = MAX_COHORT_SIZE * sizeof(customer_t) + sizeof(int);
            char* buffer_ = (char*) malloc(buffer_len);
            serialize_new_cohort_response(&new_cohort_response_, buffer_);
            // Send the buffer
            int n = sendto(socket_fd, buffer_, buffer_len, 0, (struct sockaddr *)&customer_addr, sizeof(customer_addr));
            if(n == -1)
            {
                DieWithError("bank: sendto() sent a different number of bytes than expected to customer on new-cohort");
            }
        }
        else if(buffer_words == 2 && strcmp(args[0], DELETE_COHORT) == 0)
        {
            printf("Deleting_cohort\n");
            string customer_name = args[1];
            customer_t *cohort_customers = get_all_customers_in_cohort(&bank_server, customer_name);
            if (cohort_customers != NULL) 
            {
                int num_customers = get_cohort_size(&new_cohort_response_);
                for (int i = 0; i < num_customers; i++) 
                {
                    customer_t current_customer = cohort_customers[i];
                    if(strcmp(customer_name, current_customer.name))
                    {
                        char *current_buffer_string = (char *)malloc(BUFFERMAX);
                        struct sockaddr_in current_customer_addr;
                        memset(&current_customer_addr, 0, sizeof(current_customer_addr)); // Zero out structure
                        current_customer_addr.sin_family = AF_INET;
                        current_customer_addr.sin_addr.s_addr = inet_addr(current_customer.customer_ip);
                        current_customer_addr.sin_port = htons(current_customer.portb);
                        current_buffer_string = DELETE_COHORT;
                        if(sendto(socket_fd, current_buffer_string, strlen(current_buffer_string), 0, (struct sockaddr *)&current_customer_addr, sizeof(current_customer_addr)) != strlen(current_buffer_string))
                        {
                            printf("current customer: sendto() sent a different number of bytes than expected\n");
                        }
                        /*waiting for ACK from other customers*/
                        struct sockaddr_in current_customer_fromAddr;
                        unsigned int current_fromSize = sizeof(current_customer_fromAddr);
                        char *buffer_peer_string_cohort = (char *)malloc(BUFFERMAX);
                        int current_peer_response_string_len;
                        if((current_peer_response_string_len = recvfrom(socket_fd, buffer_peer_string_cohort, BUFFERMAX, 0, (struct sockaddr*)&current_customer_fromAddr, &current_fromSize)) > BUFFERMAX)
                        {
                            printf("current customer: recvfrom() failed\n");
                        }
                        buffer_peer_string_cohort[current_peer_response_string_len] = '\0';
                        if(current_customer_addr.sin_addr.s_addr != current_customer_fromAddr.sin_addr.s_addr)
                        {
                            printf("current customer: Error: received a packet from unknown source.\n");
                        }
                        if(strcmp(buffer_peer_string_cohort, ACK)) // recieved ACK
                        {
                            printf("current customer: ACK: failed, cannot perform delete cohort operation.\n");
                        }
                    }
                }
                int delete_status = handle_delete_cohort(&bank_server, customer_name);
                if(delete_status == SUCCESS)
                {
                    printf("Customer %s deleted from cohort successfully\n", customer_name);
                }
                else
                {
                    printf("Delete cohort failed for customer %s\n", customer_name);
                }
            }
            else 
            {
                /* Handle case where no customers were found in the cohort */
                DieWithError("cannot perform delete cohort operation.\n");
            }
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
        send_msg_size = sendto(socket_fd, buffer, strlen(buffer), 0, (struct sockaddr *)&customer_addr, sizeof(customer_addr));
        if(send_msg_size != strlen(buffer))
        {
            DieWithError("bank: sendto() sent a different number of bytes than expected");
        }
    }
    // NOT REACHED */
    return 0;
}