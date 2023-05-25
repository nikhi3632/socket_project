CC = gcc
CFLAGS = -std=c99
LFLAGS = -lpthread

bank_objects = bank.o
customer_objects = customer.o

.PHONY: all clean

all: bank customer

bank: $(bank_objects)
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $^

customer: $(customer_objects)
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(bank_objects) $(customer_objects) bank customer
