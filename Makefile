CC = gcc
CFLAGS = -std=c99

bank_objects = bank.o
customer_objects = customer.o

.PHONY: all clean

all: bank customer

bank: $(bank_objects)
	$(CC) $(CFLAGS) -o $@ $^

customer: $(customer_objects)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(bank_objects) $(customer_objects) bank customer
