#ifndef DEFS_H
#define DEFS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define MAX_CUSTOMERS 1000
#define MAX_NAME_LENGTH 15
#define MAX_HOSTS 10
#define MIN_PORT 9000
#define MAX_PORT 9499
#define BUFFERMAX 255
#define MAX_WORD_LENGTH 20
#define MAX_COMMAND_WORDS 7
#define ITERATIONS 5
#define MAX_COHORT_SIZE 50
#define SUCCESS 0
#define FAILURE -1
#define OPEN "open"
#define NEW_COHORT "new-cohort"
#define DELETE_COHORT "delete-cohort"
#define EXIT_BANK "exit"
// Group 16 , Ports assigned => [9000, 9499]

typedef char* string;

typedef struct Customer {
    string name;
    double balance;
    string customer_ip;
    int portb; // is the port number customer uses for communication with the bank.
    int portp; // is the port number used for communication with other bank customers.
    int cohort_id; // defaults to -1.
} customer_t;

typedef struct Cohort {
    int cohort_id;
    int size;
    customer_t customers[MAX_COHORT_SIZE];
} cohort_t;

typedef struct Bank {
    int num_customers;
    customer_t customers[MAX_CUSTOMERS];
    int num_cohorts;
    cohort_t cohorts[MAX_CUSTOMERS];
    int bank_port;
} bank_t;

void DieWithError(const char *errorMessage) // External error handling function
{
    perror(errorMessage);
    exit(1);
}

int countWords(char *str) {
    int i, words = 1;
    for (i = 0; i < strlen(str); i++) {
        if (str[i] == ' ') {
            words++;
        }
    }
    return words;
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

int* generate_distinct_numbers(int lower_bound, int upper_bound, int k) 
{
    if (k > upper_bound - lower_bound + 1) 
    {
        return NULL;
    }
    int* result = malloc(k * sizeof(int));
    int i = 0;
    while (i < k) 
    {
        int number = rand() % (upper_bound - lower_bound + 1) + lower_bound;
        int j = 0;
        while (j < i && number != result[j]) 
        {
            j++;
        }
        if (j == i) 
        {
            result[i] = number;
            i++;
        }
    }
    return result;
}

void print_all_customers(bank_t* bank) 
{
    printf("All Customers:\n");
    for (int i = 0; i < bank->num_customers; i++) 
    {
        customer_t* customer = &bank->customers[i];
        printf("Name: %s | Balance: %.2f | IP Address: %s | Port_b: %d | Port_p: %d | Cohort ID: %d\n", 
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
            printf("Name: %s | Balance: %.2f | IP Address: %s | Port_b: %d | Port_p: %d\n", 
                    customer->name, customer->balance, customer->customer_ip, customer->portb, customer->portp);
        }
    }
}

void print_customers_by_cohort(bank_t* bank) 
{
    printf("Customers by Cohort:\n");
    for (int i = 0; i < bank->num_cohorts; i++) 
    {
        cohort_t* cohort = &bank->cohorts[i];
        printf("Cohort ID: %d | Cohort Size: %d\n", cohort->cohort_id, cohort->size);
        printf("Customers: ");
        for (int j = 0; j < bank->num_customers; j++) 
        {
            customer_t* customer = &bank->customers[j];
            if (customer->cohort_id == cohort->cohort_id) 
            {
                printf("%s (Balance: %.2f, IP Address: %s, Port_b: %d, Port_p: %d) ", 
                    customer->name, customer->balance, customer->customer_ip, customer->portb, customer->portp);
            }
        }
        printf("\n\n");
    }
}

#endif /* DEFS_H*/
