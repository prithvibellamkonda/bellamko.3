CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -lrt -pthread


all: master bin_adder

.PHONY: clean backup

clean:
	rm -rf *~ *.o *.log master bin_adder

backup:
	rm -rf backup
	mkdir backup
	cp Makefile master.c bin_adder.c integers.txt backup/
