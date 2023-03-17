#ifndef DEFS_H
#define DEFS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/fcntl.h>

#define MAX_CUSTOMERS 10
#define MAX_NAME_LENGTH 15
#define MAX_HOSTS 10
#define MIN_PORT 9000
#define MAX_PORT 9499
#define BUFFERMAX 10000
#define MAX_WORD_LENGTH 20
#define MAX_COMMAND_WORDS 7
#define ITERATIONS 1000
#define MAX_COHORT_SIZE 4
#define SUCCESS 0
#define FAILURE -1
#define OPEN "open"
#define NEW_COHORT "new-cohort"
#define DELETE_COHORT "delete-cohort"
#define EXIT_BANK "exit"
#define DEPOSIT "deposit"
#define WITHDRAW "withdraw"
#define TRANSFER "transfer"
#define LOST_TRANSFER "lost-transfer"
#define CHECKPOINT "checkpoint"
#define ROLLBACK "rollback"
#define TAKE_TENT_CKPT "take-tent-ckpt"
#define MAKE_TENT_CKPT_PERM "make-tent-ckpt-perm"
#define PREPARE_ROLLBACK "prepare-rollback"
#define YES "YES"
#define NO "NO"
#define ACK "ACK"

/* Group 16 , Ports assigned => [9000, 9499]
Note that we are not implementing any "replay" operations, 
therefore on recovery we simply resume from a consistent global state 
and that all operations after the checkpoint are "lost". */

typedef char* string;

typedef struct Customer {
    string name;
    int balance;
    string customer_ip;
    int portb; // is the port number customer uses for communication with the bank.
    int portp; // is the port number used for communication with other bank customers.
    int cohort_id; // defaults to -1.
} customer_t;

typedef struct NewCohortResponse {
    int response_code;
    customer_t cohort_customers[MAX_COHORT_SIZE];
} new_cohort_response_t;

typedef struct Cohort {
    int cohort_id;
    int size;
    int is_deleted;
    customer_t customers[MAX_COHORT_SIZE];
} cohort_t;

typedef struct Bank {
    int num_customers;
    customer_t customers[MAX_CUSTOMERS];
    int num_cohorts;
    cohort_t cohorts[MAX_CUSTOMERS];
    int bank_port;
} bank_t;

typedef struct Properties {
    string customer_name;
    int last_received;
    int last_sent;
    int first_sent;
} properties_t;

typedef struct State {
   int balance;
   bool ok_checkpoint;
   bool will_rollback;
   bool resume_execution;
   properties_t props[MAX_COHORT_SIZE];
} state_t;

void DieWithError(const char *errorMessage) // External error handling function
{
    perror(errorMessage);
    exit(1);
}

int set_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    flags |= O_NONBLOCK;
    if (fcntl(sockfd, F_SETFL, flags) == -1) {
        return -1;
    }
    return 0;
}

int countWords(char *str) 
{
    int i, words = 1;
    for (i = 0; i < strlen(str); i++) 
    {
        if (str[i] == ' ') 
        {
            words++;
        }
    }
    return words;
}

char** extract_words(char* str)
{
    char **words = (char**)malloc(MAX_COMMAND_WORDS * sizeof(char *));
    char *token = strtok(str, " ");
    int i = 0;
    while (token != NULL && i < MAX_COMMAND_WORDS) 
    {
        words[i] = (char*)malloc(MAX_WORD_LENGTH);
        strcpy(words[i], token);
        token = strtok(NULL, " ");
        i++;
    }
    return words;
}

bool is_valid_name(string name)
{
    return strlen(name) <= MAX_NAME_LENGTH;
}

bool are_ports_available(bank_t *bank, string ip, int portb, int portp)
{
    /* Check if the ports fall in the assigned range to the group.
        When on same host (i.e IP) Check if the specified ports are already assigned to a customer. */
    if (portb < MIN_PORT || portb > MAX_PORT || portp < MIN_PORT || portp > MAX_PORT)
    {
        return false;
    }
    for (int i = 0; i < bank->num_customers; i++) 
    {
        customer_t* customer = &bank->customers[i];
        if (strcmp(customer->customer_ip, ip) == 0)
        {   
            bool ports_assigned = customer->portb == portb || 
                                  customer->portp == portp || 
                                  customer->portb == portp || 
                                  customer->portp == portb ;
            if (ports_assigned) 
            {
                return false;
            }
        }
    }
    return true;
}

bool check_existing_customer(bank_t *bank, string name)
{
    for(int i = 0; i < bank->num_customers; i++)
    {
        if(strcasecmp(bank->customers[i].name, name) == 0)
        {
            return true;
        }
    }
    return false;
}

int find_customer_index(bank_t *bank, string customer_name) 
{
    for (int i = 0; i < bank->num_customers; i++) 
    {
        if (strcmp(bank->customers[i].name, customer_name) == 0) 
        {
            return i;
        }
    }
    return -1;
}

void print_state(state_t* state)
{
    printf("Balance: %d| ok_checkpoint: %d | will_rollback :%d | resume_execution :%d\n", 
        state->balance, state->ok_checkpoint, state->will_rollback, state->resume_execution);
    for(int i = 0; i < MAX_COHORT_SIZE; i++)
    {
        printf("Props %d| Customer_name: %s | first_sent: %d | last_received : %d | last_sent: %d\n" , i, 
            state->props[i].customer_name, state->props[i].first_sent, state->props[i].last_received, state->props[i].last_sent);
    }
}

customer_t* get_all_customers_in_cohort(bank_t *bank, string customer_name)
{
    for (int i = 0; i < bank->num_cohorts; i++) 
    {
        cohort_t *cohort_info = &bank->cohorts[i];
        for(int j = 0; j < cohort_info->size; j++)
        {
            if(strcmp(cohort_info->customers[j].name, customer_name) == 0)
            {
                int id = cohort_info->cohort_id;
                if(id != -1)
                {
                    return cohort_info->customers;
                }
            }
        }
    }
    
    // Return NULL if no matching customer is found
    return NULL;
}

customer_t* get_customer_by_name(new_cohort_response_t new_cohort_response, string customer_name) {
    for (int i = 0; i < MAX_COHORT_SIZE; i++) {
        if (strcmp(new_cohort_response.cohort_customers[i].name, customer_name) == 0) {
            customer_t* customer = malloc(sizeof(customer_t));
            *customer = new_cohort_response.cohort_customers[i];
            return customer;
        }
    }
    return NULL; // customer with the given name not found
}

void print_all_customers(bank_t* bank) 
{
    printf("All Customers:\n");
    for (int i = 0; i < bank->num_customers; i++) 
    {
        customer_t* customer = &bank->customers[i];
        printf("Name: %s | Balance: %d | IP Address: %s | Port_b: %d | Port_p: %d | Cohort ID: %d\n", 
                customer->name, customer->balance, customer->customer_ip, customer->portb, customer->portp, 
                customer->cohort_id);
    }
}

void print_customers_in_cohort(bank_t* bank, int cohort_id) 
{
    printf("Customers in Cohort %d:\n", cohort_id);
    for (int i = 0; i < bank->num_customers; i++) 
    {
        customer_t* customer = &bank->customers[i];
        if (customer->cohort_id == cohort_id) 
        {
            printf("Name: %s | Balance: %d | IP Address: %s | Port_b: %d | Port_p: %d\n", 
                    customer->name, customer->balance, customer->customer_ip, customer->portb, customer->portp);
        }
    }
}

void print_cohort(cohort_t cohort)
{
    printf("Cohort ID: %d | Cohort Size: %d\n", cohort.cohort_id, cohort.size);
    printf("Customers: ");
    for (int j = 0; j < cohort.size; j++) 
    {
        customer_t customer = cohort.customers[j];
        {
            printf("%s (Balance: %d, IP Address: %s, Port_b: %d, Port_p: %d) ", 
                customer.name, customer.balance, customer.customer_ip, customer.portb, customer.portp);
        }
    }
}

void print_bank_cohorts(bank_t* bank)
{
    printf("Customers by Cohort:\n");
        cohort_t* cohort = &bank->cohorts[bank->num_cohorts-1];
        printf("Cohort ID: %d | Cohort Size: %d\n", cohort->cohort_id, cohort->size);
        printf("Customers: ");
        for (int j = 0; j < bank->num_customers; j++) 
        {
            customer_t* customer = &bank->customers[j];
            if (customer->cohort_id == cohort->cohort_id) 
            {
                printf("%s (Balance: %d, IP Address: %s, Port_b: %d, Port_p: %d) ", 
                    customer->name, customer->balance, customer->customer_ip, customer->portb, customer->portp);
            }
        }
}

void print_new_cohort_response(new_cohort_response_t* new_cohort_response, int cohort_size)
{
    for (int i = 0; i < cohort_size; i++) 
    {
        customer_t *cohort_customers = &new_cohort_response->cohort_customers[i];
        printf("New cohort Customer %d\n%s(Balance: %d, IP Address: %s, Port_b: %d, Port_p: %d Cohort_id: %d)\n ", 
                    i+1, cohort_customers->name, cohort_customers->balance, cohort_customers->customer_ip,
                     cohort_customers->portb, cohort_customers->portp, cohort_customers->cohort_id);
    }
}

int get_cohort_size(new_cohort_response_t* new_cohort_response)
{
    int sz = 0;
    for (int i = 0; i < MAX_COHORT_SIZE; i++) 
    {
        customer_t *cohort_customers = &new_cohort_response->cohort_customers[i];
        if(cohort_customers->name)
        {
            sz++;
        }
    }
    return sz;
}

// Serialization function
void serialize_new_cohort_response(const new_cohort_response_t *response, char *buffer) {
    int offset = 0;
    memcpy(buffer + offset, &(response->response_code), sizeof(int));
    offset += sizeof(int);
    for (int i = 0; i < MAX_COHORT_SIZE; i++) {
        customer_t customer = response->cohort_customers[i];
        memcpy(buffer + offset, &(customer.balance), sizeof(double));
        offset += sizeof(double);
        memcpy(buffer + offset, &(customer.portb), sizeof(int));
        offset += sizeof(int);
        memcpy(buffer + offset, &(customer.portp), sizeof(int));
        offset += sizeof(int);
        memcpy(buffer + offset, &(customer.cohort_id), sizeof(int));
        offset += sizeof(int);
        int name_length = customer.name ? strlen(customer.name) + 1 : 0;
        memcpy(buffer + offset, &(name_length), sizeof(int));
        offset += sizeof(int);
        if (name_length > 0) {
            memcpy(buffer + offset, customer.name, name_length);
            offset += name_length;
        }
        int ip_length = customer.customer_ip ? strlen(customer.customer_ip) + 1 : 0;
        memcpy(buffer + offset, &(ip_length), sizeof(int));
        offset += sizeof(int);
        if (ip_length > 0) {
            memcpy(buffer + offset, customer.customer_ip, ip_length);
            offset += ip_length;
        }
    }
}

// Deserialization function
void deserialize_new_cohort_response(new_cohort_response_t *response, const char *buffer) {
    int offset = 0;
    memcpy(&(response->response_code), buffer + offset, sizeof(int));
    offset += sizeof(int);
    for (int i = 0; i < MAX_COHORT_SIZE; i++) {
        customer_t customer;
        memcpy(&(customer.balance), buffer + offset, sizeof(double));
        offset += sizeof(double);
        memcpy(&(customer.portb), buffer + offset, sizeof(int));
        offset += sizeof(int);
        memcpy(&(customer.portp), buffer + offset, sizeof(int));
        offset += sizeof(int);
        memcpy(&(customer.cohort_id), buffer + offset, sizeof(int));
        offset += sizeof(int);
        int name_length, ip_length;
        memcpy(&(name_length), buffer + offset, sizeof(int));
        offset += sizeof(int);
        if (name_length > 0) {
            customer.name = malloc(name_length);
            memcpy(customer.name, buffer + offset, name_length);
            offset += name_length;
        } else {
            customer.name = NULL;
        }
        memcpy(&(ip_length), buffer + offset, sizeof(int));
        offset += sizeof(int);
        if (ip_length > 0) {
            customer.customer_ip = malloc(ip_length);
            memcpy(customer.customer_ip, buffer + offset, ip_length);
            offset += ip_length;
        } else {
            customer.customer_ip = NULL;
        }
        response->cohort_customers[i] = customer;
    }
}

void print_customers_by_cohort(bank_t* bank) 
{
    printf("Customers by Cohort:\n");
    for (int i = 0; i < bank->num_cohorts; i++) 
    {
        cohort_t cohort = bank->cohorts[i];
        printf("Cohort ID: %d | Cohort Size: %d | Cohort is_deleted: %d\n", cohort.cohort_id, cohort.size, cohort.is_deleted);
        printf("Customers: ");
        for (int j = 0; j < bank->num_customers; j++) 
        {
            customer_t* customer = &bank->customers[j];
            if (customer->cohort_id == cohort.cohort_id) 
            {
                printf("%s (Balance: %d, IP Address: %s, Port_b: %d, Port_p: %d) \n", 
                    customer->name, customer->balance, customer->customer_ip, customer->portb, customer->portp);
            }
        }
    }
}

#endif /* DEFS_H*/
