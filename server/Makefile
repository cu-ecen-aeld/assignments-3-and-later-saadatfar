CC ?= $(CRPSS_COMPILE)gcc
CFLAGS ?= -g -Wall -Werror
LDFLAGS ?= -pthread -lrt

all: aesdsocket

aesdsocket.o: aesdsocket.c aesdsocket.h
	$(CC) $(CFLAGS) -c aesdsocket.c

aesdsocket: aesdsocket.o
	$(CC) $(INCLUDES) $(LDFLAGS) aesdsocket.o -o aesdsocket

run: aesdsocket
	./aesdsocket

clean:
	rm -f aesdsocket.o aesdsocket

.PHONY: clean all run
