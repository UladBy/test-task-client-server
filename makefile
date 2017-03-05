
all: server client

server: server.o tools.o
	gcc -o $@ $^ -pthread

client: client.o tools.o
	gcc -o $@ $^ -pthread

%.o : %.c packets.h
	gcc -c $(CFLAGS) $< -o $@

clean:
	rm -f *.o server client

.PHONY: clean all
