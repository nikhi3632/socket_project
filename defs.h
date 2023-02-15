#ifndef DEFS_H
#define DEFS_H

#define MAX_CUSTOMERS 1000
#define MAX_NAME_LENGTH 15
#define MAX_ADDRESS_LENGTH 15
// #define MAX_COHORT_SIZE 50 // Group 16 => [9000, 9499]
#define MAX_HOSTS 10
#define MIN_PORT 9000
#define MAX_PORT 9499

const cohort_t DEFAULT_COHORT = {0, 0};

#include <arpa/inet.h>  // for echoBankAddr

typedef struct Cohort {
  int cohort_id;
  int size;
} cohort_t;

typedef struct Customer {
    char name[MAX_NAME_LENGTH + 1];
    char ip_address[MAX_ADDRESS_LENGTH + 1]; /* customers with the same IP address are on the same host
                                            so check existing customer's port_b and port_p
                                            before assigning port_b, port_p to new customer */
    double balance;
    int portb; //is the port number customer uses for communication with the bank.
    int portp; //is the port number used for communication with other bank customers.
    cohort_t cohort;
} customer_t;

typedef struct Bank {
    int num_customers;
    customer_t customers[MAX_CUSTOMERS];
    int num_cohorts;
    cohort_t cohorts[MAX_CUSTOMERS];
    struct sockaddr_in echoBankAddr; // Bank details to Use internet addr family, Set banks's IP address, Set bank's port
} bank_t;

#endif /* DEFS_H*/