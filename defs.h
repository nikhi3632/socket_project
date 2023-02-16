#ifndef DEFS_H
#define DEFS_H

#define MAX_CUSTOMERS 1000
#define MAX_NAME_LENGTH 15
#define MAX_HOSTS 10
#define MIN_PORT 9000
#define MAX_PORT 9499
#define BUFFERMAX 255
#define MAX_WORD_LENGTH 20
#define MAX_COMMAND_WORDS 7
#define ITERATIONS 5
// #define MAX_COHORT_SIZE 50 // Group 16 => [9000, 9499]

typedef char* string;

typedef struct Cohort {
  int cohort_id;
  int size;
} cohort_t;
const cohort_t DEFAULT_COHORT = {0, 0};

typedef struct Customer {
    string name;
    double balance;
    string customer_ip;
    /* customers with the same IP address are on the same host
    so check existing customer's port_b and port_p
    before assigning port_b, port_p to new customer */
    int portb; //is the port number customer uses for communication with the bank.
    int portp; //is the port number used for communication with other bank customers.
    cohort_t cohort;
} customer_t;

typedef struct Bank {
    int num_customers;
    customer_t customers[MAX_CUSTOMERS];
    int num_cohorts;
    cohort_t cohorts[MAX_CUSTOMERS];
    string bank_ip;
    int bank_port;
} bank_t;

#endif /* DEFS_H*/