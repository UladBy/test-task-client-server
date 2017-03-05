
all: server clinet

server: server.o tools.o
	gcc -o $@ $^ -pthread

clinet: client.o tools.o
	gcc -o $@ $^ -pthread

%.o : %.c packets.h
	gcc -c $(CFLAGS) $< -o $@

