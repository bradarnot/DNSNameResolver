CC = gcc
CFLAGS = -c -g -Wall -Wextra
LFLAGS = -Wall -Wextra -pthread
EXE = multi-lookup

.PHONY: all clean

all: $(EXE)

$(EXE): multi-lookup.o queue.o util.o
	$(CC) $(LFLAGS) $^ -o $@

queue.o: queue.c queue.h
	$(CC) $(CFLAGS) $<

util.o: util.c util.h
	$(CC) $(CFLAGS) $<

multi-lookup.o: multi-lookup.c
	$(CC) $(CFLAGS) $<

clean:
	rm -f multi-lookup 
	rm -f *.o
	rm -f *~
	rm -f result.txt
