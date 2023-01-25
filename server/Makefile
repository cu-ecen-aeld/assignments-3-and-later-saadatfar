all: aesdsocket

aesdsocket.o: aesdsocket.c aesdsocket.h
	$(CC) $(CCFLAGS) -c aesdsocket.c

aesdsocket: aesdsocket.o
	$(CC) $(LDFLAGS) -pthread aesdsocket.o -o aesdsocket

run: aesdsocket
	./aesdsocket

clean:
	rm -f aesdsocket.o aesdsocket

.PHONY: clean all run
