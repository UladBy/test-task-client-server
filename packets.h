#ifndef PACKETS_H
#define PACKETS_H

#include <sys/queue.h>

#define PACKET_UNSIGNED_SIZE 4
/* for mashine independese we predefin int size */

enum {
    CONNECT = 1,
    MESSAGE,
    PING,
    GETSTAT,
    SETSTAT
} packet_type_t;

char* read_string(int sock);
int read_unsigned(int sock, unsigned *data);
int send_packet_type(int sock, unsigned packet_type);
int send_string(int sock, const char string[]);
int send_unsigned(int sock, unsigned value);

#endif /* PACKETS_H */
