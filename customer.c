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

state_t local_state; //global variable
state_t secured_state; //global variable
int cohort_size = 0; // global variable
string peer_name; //global variable
new_cohort_response_t new_cohort_response; //global variable
bool is_cohort_formed = false;

void init_props()
{
    for(int i = 0; i < cohort_size; i++)
    {
        if(new_cohort_response.cohort_customers[i].name != NULL)
        {
            local_state.props[i].customer_name = new_cohort_response.cohort_customers[i].name;
            local_state.props[i].last_received = 0;
            local_state.props[i].first_sent = 0;
            local_state.props[i].last_sent = 0;
        }
    }
}

void copy_local2secure()
{
    secured_state.balance = local_state.balance;
    secured_state.ok_checkpoint = local_state.ok_checkpoint;
    secured_state.resume_execution = local_state.resume_execution;
    secured_state.will_rollback = local_state.will_rollback;

    for(int i = 0; i < cohort_size; i++)
    {
        secured_state.props[i].customer_name = local_state.props[i].customer_name;
        secured_state.props[i].first_sent = local_state.props[i].first_sent;
        secured_state.props[i].last_received = local_state.props[i].last_received;
        secured_state.props[i].last_sent = local_state.props[i].last_sent;
    }
}

void copy_secure2local()
{
    local_state.balance = secured_state.balance;
    local_state.ok_checkpoint = secured_state.ok_checkpoint;
    local_state.resume_execution = secured_state.resume_execution;
    local_state.will_rollback = secured_state.will_rollback;

    for(int i = 0; i < cohort_size; i++)
    {
        local_state.props[i].customer_name = secured_state.props[i].customer_name;
        local_state.props[i].first_sent = secured_state.props[i].first_sent;
        local_state.props[i].last_received = secured_state.props[i].last_received;
        local_state.props[i].last_sent = secured_state.props[i].last_sent;
    }
}

void *receive_and_respond_to_peers(void *arg)
{
    int sock_peer = *(int*)arg;
    struct sockaddr_in peer_fromAddr;
    unsigned int peer_fromSize = sizeof(peer_fromAddr);
    int peer_response_string_len;
    
    while(1)
    {
        char *peer_buffer_string = (char *)malloc(BUFFERMAX);
        if((peer_response_string_len = recvfrom(sock_peer, peer_buffer_string, BUFFERMAX, 0, (struct sockaddr*)&peer_fromAddr, &peer_fromSize)) > BUFFERMAX)
        {
            printf("customer: recvfrom() failed\n");
        }
        if(!is_cohort_formed)
        {
            deserialize_new_cohort_response(&new_cohort_response, peer_buffer_string);
            cohort_size = get_cohort_size(&new_cohort_response);
            print_new_cohort_response(&new_cohort_response, cohort_size);
            init_props();
            is_cohort_formed = true;
            peer_buffer_string = ACK;
            if(sendto(sock_peer, peer_buffer_string, strlen(peer_buffer_string), 0, (struct sockaddr *)&peer_fromAddr, sizeof(peer_fromAddr)) != strlen(peer_buffer_string))
            {
                printf("peer customer: sendto() sent a different number of bytes than expected\n");
            }
        }
        char *peer_buffer_string_copy = NULL;
        peer_buffer_string_copy = (char *)malloc(BUFFERMAX);
        strcpy(peer_buffer_string_copy, peer_buffer_string);
        char **args = extract_words(peer_buffer_string_copy);
        char* operation = args[0];
        if(strcmp(operation, TRANSFER) == 0)
        {
            int amount = atoi(args[1]);
            string transfer_customer = args[2];
            int fs_label = atoi(args[3]);
            char msg[BUFFERMAX];
            for(int i = 0; i < cohort_size; i++)
            {
                if(local_state.props[i].customer_name && (strcmp(transfer_customer, local_state.props[i].customer_name) == 0))
                {
                    if(!(local_state.props[i].first_sent == 0 && local_state.props[i].last_received == 0) 
                        && ((fs_label - local_state.props[i].last_received) > 1)) //identify disparity
                    {
                        local_state.ok_checkpoint = false;
                        for(int i = 0; i < cohort_size; i++)
                        {
                            if(local_state.props[i].customer_name && strcmp(local_state.props[i].customer_name, peer_name))
                            {
                                customer_t* send_customer = get_customer_by_name(new_cohort_response, local_state.props[i].customer_name);
                                snprintf(msg, BUFFERMAX, "%s %s %d", PREPARE_ROLLBACK, peer_name, local_state.props[i].last_sent);
                                printf("Rollback message: %s\n", msg);
                                struct sockaddr_in send_addr;
                                memset(&send_addr, 0, sizeof(send_addr)); // Zero out structure
                                send_addr.sin_family = AF_INET;
                                send_addr.sin_addr.s_addr = inet_addr(send_customer->customer_ip);
                                send_addr.sin_port = htons(send_customer->portp);
                                msg[(int)strlen(msg)] = '\0';
                                if(sendto(sock_peer, msg, strlen(msg), 0, (struct sockaddr *)&send_addr, sizeof(send_addr)) != strlen(msg))
                                {
                                    printf("rollback-customer: sendto() sent a different number of bytes than expected\n");
                                }
                                struct sockaddr_in recv_fromAddr;
                                unsigned int recv_fromSize;
                                int recv_respone_string_len;
                                char *buffer_string_recv = NULL;
                                buffer_string_recv = (char *)malloc(BUFFERMAX);
                                if((recv_respone_string_len = recvfrom(sock_peer, buffer_string_recv, BUFFERMAX, 0, (struct sockaddr*)&recv_fromAddr, &recv_fromSize)) > BUFFERMAX)
                                {
                                    printf("rollback-customer: recvfrom() failed\n");
                                }
                                buffer_string_recv[recv_respone_string_len] = '\0';
                                // assume string format
                                char* buffer_string_recv_copy = NULL;
                                buffer_string_recv_copy = (char *)malloc(BUFFERMAX);
                                strcpy(buffer_string_recv_copy, buffer_string_recv);
                                char **args = extract_words(buffer_string_recv_copy);
                                string response = args[1];
                                if(strcmp(response, YES))
                                {
                                    local_state.will_rollback = false;
                                    break;
                                }
                            }
                        }
                        if(local_state.will_rollback)
                        {
                            printf("---Local State Before roll back---");
                            print_state(&local_state);
                            copy_secure2local();
                            for(int i = 0; i < cohort_size; i++)
                            {
                                if(local_state.props[i].customer_name && (strcmp(local_state.props[i].customer_name, peer_name)))
                                {
                                    customer_t* send_customer = get_customer_by_name(new_cohort_response, local_state.props[i].customer_name);
                                    snprintf(msg, BUFFERMAX, "%s", ROLLBACK);
                                    struct sockaddr_in send_addr;
                                    memset(&send_addr, 0, sizeof(send_addr)); // Zero out structure
                                    send_addr.sin_family = AF_INET;
                                    send_addr.sin_addr.s_addr = inet_addr(send_customer->customer_ip);
                                    send_addr.sin_port = htons(send_customer->portp);
                                    msg[(int)strlen(msg)] = '\0';
                                    if(sendto(sock_peer, msg, strlen(msg), 0, (struct sockaddr *)&send_addr, sizeof(send_addr)) != strlen(msg))
                                    {
                                        printf("rollback-customer: sendto() sent a different number of bytes than expected\n");
                                    }
                                }
                            }
                            printf("---Local State after roll back---");
                            print_state(&local_state);
                            printf("Roll-back complete\n");
                        }
                    }
                    else
                    {
                        local_state.balance += amount;
                        local_state.props[i].last_received++;
                        printf("Updated balance of customer %s is %d\n", transfer_customer, local_state.balance);
                        print_state(&local_state);
                    }
                }
            }
        }
        else if(strcmp(operation, TAKE_TENT_CKPT) == 0)
        {
            string pertaining_customer = args[1];
            int lr_label = atoi(args[2]);
            struct sockaddr_in take_ckpt_addr;
            char *take_ckpt_buffer_string = (char *)malloc(BUFFERMAX);
            for(int i =0; i < cohort_size; i++)
            {
                if(strcmp(pertaining_customer, new_cohort_response.cohort_customers[i].name) == 0)
                {
                    memset(&take_ckpt_addr, 0, sizeof(take_ckpt_addr)); // Zero out structure
                    take_ckpt_addr.sin_family = AF_INET;
                    take_ckpt_addr.sin_addr.s_addr = inet_addr(new_cohort_response.cohort_customers[i].customer_ip);
                    take_ckpt_addr.sin_port = htons(new_cohort_response.cohort_customers[i].portp);
                }
            }
            for(int i = 0; i < cohort_size; i++)
            {
                if(strcmp(pertaining_customer, local_state.props[i].customer_name) == 0)
                {
                    if(local_state.ok_checkpoint && (lr_label >= local_state.props[i].first_sent))
                    {
                        snprintf(take_ckpt_buffer_string, BUFFERMAX, "%s %s", peer_name, YES);
                        if(sendto(sock_peer, take_ckpt_buffer_string, strlen(take_ckpt_buffer_string), 0, (struct sockaddr *)&take_ckpt_addr, sizeof(take_ckpt_addr)) != strlen(take_ckpt_buffer_string))
                        {
                            printf("peer customer: sendto() sent a different number of bytes than expected\n");
                        }
                    }
                    else
                    {
                        snprintf(take_ckpt_buffer_string, BUFFERMAX, "%s %s", peer_name, NO);
                        if(sendto(sock_peer, take_ckpt_buffer_string, strlen(take_ckpt_buffer_string), 0, (struct sockaddr *)&take_ckpt_addr, sizeof(take_ckpt_addr)) != strlen(take_ckpt_buffer_string))
                        {
                            printf("peer customer: sendto() sent a different number of bytes than expected\n");
                        }
                    }
                }
            }
            printf("Take tentative checkpoint completed\n");
            printf("---Local State----\n");
            print_state(&local_state);
        }
        else if(strcmp(operation, MAKE_TENT_CKPT_PERM) == 0)
        {
            copy_local2secure();
            printf("Made tentative checkpoint permanent\n");
            printf("---Secured State----\n");
            print_state(&secured_state);
        }
        else if(strcmp(operation, PREPARE_ROLLBACK) == 0)
        {
            string cname = args[1];
            int ls_label = atoi(args[2]);
            for(int i = 0; i < cohort_size; i++)
            {
                if(strcmp(local_state.props[i].customer_name, cname))
                {
                    if(!(local_state.will_rollback && (ls_label > local_state.props[i].last_sent && local_state.resume_execution))) //verify condition.
                    {
                        snprintf(peer_buffer_string, BUFFERMAX, "%s %s", peer_name, YES);
                        printf("Send Rollback ACK: %s\n", peer_buffer_string);
                        if(sendto(sock_peer, peer_buffer_string, strlen(peer_buffer_string), 0, (struct sockaddr *)&peer_fromAddr, sizeof(peer_fromAddr)) != strlen(peer_buffer_string))
                        {
                            printf("peer customer: sendto() sent a different number of bytes than expected\n");
                        }
                    }
                    else
                    {
                        snprintf(peer_buffer_string, BUFFERMAX, "%s %s", peer_name, NO);
                        if(sendto(sock_peer, peer_buffer_string, strlen(peer_buffer_string), 0, (struct sockaddr *)&peer_fromAddr, sizeof(peer_fromAddr)) != strlen(peer_buffer_string))
                        {
                            printf("prepare-rollback customer: sendto() sent a different number of bytes than expected\n");
                        }
                    }
                    break;
                }
            }
        }
        else if(strcmp(operation, ROLLBACK) == 0)
        {
            printf("---Local State Before roll back---");
            print_state(&local_state);
            copy_secure2local();
            printf("---Local State after roll back---");
            print_state(&local_state);
            printf("Roll-back complete\n");
        }
    }
    // return NULL;
}

void *receive_and_respond_to_bank(void *arg)
{
    int sock = *(int*)arg;
    char *bank_buffer_string = (char *)malloc(BUFFERMAX);
    struct sockaddr_in bank_fromAddr;
    unsigned int bank_fromSize = sizeof(bank_fromAddr);
    int bank_response_string_len;
    while(1)
    {
        bank_response_string_len = recvfrom(sock, bank_buffer_string, BUFFERMAX, 0, (struct sockaddr*)&bank_fromAddr, &bank_fromSize);
        if(bank_response_string_len > BUFFERMAX)
        {
            printf("customer: recvfrom() failed\n");
        }
        bank_buffer_string[bank_response_string_len] = '\0';
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

void handle_update(int amount, string operation)
{
    int last_balance = local_state.balance;
    if(strcmp(operation, DEPOSIT) == 0)
    {
        local_state.balance += amount;
    }
    else if(strcmp(operation, WITHDRAW) == 0)
    {
        local_state.balance -= amount;
    }
    int update_balance = local_state.balance;
    printf("Last-Balance: %d, Operation: %s, Amount: %d, Updated-Balance: %d\n", last_balance, operation, amount, update_balance);
}

void handle_transfer(int amount, string customer_name, int sockfd_peer)
{
    customer_t* peer_customer = get_customer_by_name(new_cohort_response, customer_name); //Julio
    if (peer_customer != NULL) 
    {
        string peer_ip = peer_customer->customer_ip;
        int peer_port = peer_customer->portp;
        local_state.balance -= amount;
        int index = -1; 
        for(int i = 0; i < cohort_size; i++)
        {
            if(strcmp(local_state.props[i].customer_name, peer_name) == 0)
            {
                local_state.props[i].first_sent++;
                local_state.props[i].last_sent++;
                index = i;
                break;
            }
        }
        char msg[BUFFERMAX];
        snprintf(msg, BUFFERMAX, "%s %d %s %d", TRANSFER, amount, customer_name, local_state.props[index].first_sent);
        printf("Transfer message: %s\n", msg);
        struct sockaddr_in peer_addr;
        memset(&peer_addr, 0, sizeof(peer_addr)); // Zero out structure
        peer_addr.sin_family = AF_INET;
        peer_addr.sin_addr.s_addr = inet_addr(peer_ip);
        peer_addr.sin_port = htons(peer_port);
        if(sendto(sockfd_peer, msg, strlen(msg), 0, (struct sockaddr *)&peer_addr, sizeof(peer_addr)) != strlen(msg))
        {
            printf("transfer-peer customer: sendto() sent a different number of bytes than expected\n");
        }
    }
}

void handle_lost_transfer(int amount, string customer_name)
{
    customer_t* peer_customer = get_customer_by_name(new_cohort_response, customer_name);
    if (peer_customer != NULL) 
    {
        string peer_ip = peer_customer->customer_ip;
        int peer_port = peer_customer->portp;
        local_state.balance -= amount;
        for(int i = 0; i < cohort_size; i++)
        {
            if(strcmp(local_state.props[i].customer_name, peer_name) == 0)
            {
                local_state.props[i].first_sent++;
                local_state.props[i].last_sent++;
            }
        }
    }
}

void handle_checkpoint(int sockfd_peer)
{
    char msg[BUFFERMAX];
    int last_recv_label = -1;
    for(int i = 0; i < cohort_size; i++)
    {
        if(strcmp(local_state.props[i].customer_name, peer_name) == 0)
        {
            last_recv_label = local_state.props[i].last_received;
        }
    }
    for(int i = 0; i < cohort_size; i++)
    {
        if(strcmp(local_state.props[i].customer_name, peer_name))
        {
            customer_t* send_customer = get_customer_by_name(new_cohort_response, local_state.props[i].customer_name);
            snprintf(msg, BUFFERMAX, "%s %s %d", TAKE_TENT_CKPT, peer_name, last_recv_label);
            printf("Message: %s\n", msg);
            struct sockaddr_in send_addr;
            memset(&send_addr, 0, sizeof(send_addr)); // Zero out structure
            send_addr.sin_family = AF_INET;
            send_addr.sin_addr.s_addr = inet_addr(send_customer->customer_ip);
            send_addr.sin_port = htons(send_customer->portp);
            if(sendto(sockfd_peer, msg, strlen(msg), 0, (struct sockaddr *)&send_addr, sizeof(send_addr)) != strlen(msg))
            {
                printf("send-customer: sendto() sent a different number of bytes than expected\n");
            }
            struct sockaddr_in recv_fromAddr;
            unsigned int recv_fromSize = sizeof(recv_fromAddr);
            int recv_respone_string_len;
            char *buffer_string_recv = (char *)malloc(BUFFERMAX);
            if((recv_respone_string_len = recvfrom(sockfd_peer, buffer_string_recv, BUFFERMAX, 0, (struct sockaddr*)&recv_fromAddr, &recv_fromSize)) > BUFFERMAX)
            {
                printf("recv-customer: recvfrom() failed\n");
            }
            buffer_string_recv[recv_respone_string_len] = '\0';
            // assume string format
            char* buffer_string_recv_copy = (char *)malloc(BUFFERMAX);
            strcpy(buffer_string_recv_copy, buffer_string_recv);
            char **arguments = extract_words(buffer_string_recv_copy);
            string response = arguments[1];
            if(strcmp(response, YES))
            {
                return;
            }
        }
    }
    copy_local2secure();
    for(int i = 0; i < cohort_size; i++)
    {
        if(strcmp(local_state.props[i].customer_name, peer_name))
        {
            customer_t* send_customer = get_customer_by_name(new_cohort_response, local_state.props[i].customer_name);
            snprintf(msg, BUFFERMAX, "%s", MAKE_TENT_CKPT_PERM);
            struct sockaddr_in send_addr;
            memset(&send_addr, 0, sizeof(send_addr)); // Zero out structure
            send_addr.sin_family = AF_INET;
            send_addr.sin_addr.s_addr = inet_addr(send_customer->customer_ip);
            send_addr.sin_port = htons(send_customer->portp);
            if(sendto(sockfd_peer, msg, strlen(msg), 0, (struct sockaddr *)&send_addr, sizeof(send_addr)) != strlen(msg))
            {
                printf("make-tent-ckpt customer: sendto() sent a different number of bytes than expected\n");
            }
        }
    }
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
    unsigned int fromSize = sizeof(fromAddr);;                  
    int response_string_len;                 // Length of received response
    char *buffer_string = NULL;             // String to send to echo bank
    size_t buffer_string_len = BUFFERMAX;   // Length of string to echo
    char* buffer_string_copy = NULL;
    char* buffer_string_new = NULL;

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
        response_string_len = recvfrom(sockfd, buffer_string, BUFFERMAX, 0, (struct sockaddr *) &fromAddr, &fromSize);
        if(response_string_len > BUFFERMAX)
        {
            DieWithError("customer: recvfrom() failed");
        }
        buffer_string[response_string_len] = '\0';
        if(strcmp(operation, OPEN) == 0)
        {
            peer_name = args[1];
            int customer_balance = atoi(args[2]);
            local_state.balance = customer_balance;
            local_state.ok_checkpoint = true;
            local_state.will_rollback = true;
            local_state.resume_execution = true;
        }
        else if(strcmp(operation, NEW_COHORT) == 0)
        {
            is_cohort_formed = false;
            pthread_cancel(thread_id1);
            cohort_size = atoi(args[2]);
            deserialize_new_cohort_response(&new_cohort_response, buffer_string);
            init_props();
            print_new_cohort_response(&new_cohort_response, cohort_size);
            if(new_cohort_response.response_code == FAILURE)
            {
                printf("New cohort operation failed for customer %s\n", peer_name);
            }
            else
            {
                for(int i = 0; i < cohort_size; i++)
                {
                    if(strcmp(new_cohort_response.cohort_customers[i].name, peer_name))
                    {   
                        int cohort_buffer_len = MAX_COHORT_SIZE * sizeof(customer_t) + sizeof(int);
                        char* cohort_buffer_string = (char*) malloc(cohort_buffer_len);
                        serialize_new_cohort_response(&new_cohort_response, cohort_buffer_string);
                        struct sockaddr_in cohort_customer_addr;
                        memset(&cohort_customer_addr, 0, sizeof(cohort_customer_addr)); // Zero out structure
                        cohort_customer_addr.sin_family = AF_INET;
                        cohort_customer_addr.sin_addr.s_addr = inet_addr(new_cohort_response.cohort_customers[i].customer_ip);
                        cohort_customer_addr.sin_port = htons(new_cohort_response.cohort_customers[i].portp);
                        if(sendto(sockfd_peer, cohort_buffer_string, cohort_buffer_len, 0, (struct sockaddr *)&cohort_customer_addr, sizeof(cohort_customer_addr)) != cohort_buffer_len)
                        {
                            printf("cohort customer: sendto() sent a different number of bytes than expected\n");
                        }
                        struct sockaddr_in cohort_fromAddr;
                        unsigned int cohort_fromSize = sizeof(cohort_fromAddr);
                        int cohort_response_string_len;
                        char *buffer_string_cohort = NULL;
                        buffer_string_cohort = (char *)malloc(BUFFERMAX);
                        /*other customer recieves the packet and send ACK*/
                        /*waiting for ACK from other customers*/
                        if((cohort_response_string_len = recvfrom(sockfd_peer, buffer_string_cohort, BUFFERMAX, 0, (struct sockaddr*)&cohort_fromAddr, &cohort_fromSize)) > BUFFERMAX)
                        {
                            printf("customer: recvfrom() failed\n");
                        }
                        buffer_string_cohort[cohort_response_string_len] = '\0';
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
                is_cohort_formed = true;
                pthread_create(&thread_id1, NULL, receive_and_respond_to_peers, (void*)&sockfd_peer);
            }
        }
        else if(strcmp(operation, DEPOSIT) == 0 || strcmp(operation, WITHDRAW) == 0)
        {
            int amount = atoi(args[1]);
            handle_update(amount, operation);
        }
        else if(strcmp(operation, TRANSFER) == 0)
        {
            pthread_cancel(thread_id1);
            int amount = atoi(args[1]);
            string customer_name = args[2];
            printf("Current balance for customer %s is %d\n", peer_name, local_state.balance);
            handle_transfer(amount, customer_name, sockfd_peer); // Julio
            printf("Updated balance for customer %s after transfer is %d\n", peer_name, local_state.balance);
            printf("---Local State after transfer----\n");
            print_state(&local_state);
            pthread_create(&thread_id1, NULL, receive_and_respond_to_peers, (void*)&sockfd_peer);
        }
        else if(strcmp(operation, LOST_TRANSFER) == 0)
        {
            int amount = atoi(args[1]);
            string customer_name = args[2];
            printf("Current balance for customer %s is %d\n", peer_name, local_state.balance);
            handle_lost_transfer(amount, customer_name);
            printf("---Local State after Lost transfer----\n");
            print_state(&local_state);
            printf("Updated balance for customer after lost-transfer %s is %d\n", peer_name, local_state.balance);
        }
        else if(strcmp(operation, CHECKPOINT) == 0)
        {
            pthread_cancel(thread_id1);
            handle_checkpoint(sockfd_peer);
            printf("---Local State----\n");
            print_state(&local_state);
            printf("---Secured State----\n");
            print_state(&secured_state);
            printf("Checkpoint handle complete\n");
            pthread_create(&thread_id1, NULL, receive_and_respond_to_peers, (void*)&sockfd_peer);
        }
        if(bank_addr.sin_addr.s_addr != fromAddr.sin_addr.s_addr)
        {
            DieWithError("customer: Error: received a packet from unknown source.\n");
        }
        pthread_create(&thread_id2, NULL, receive_and_respond_to_bank, (void*)&sockfd);
    }
    // close the sockets
    close(sockfd_peer);
    close(sockfd);
    return 0;
}
