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

int build_string(const char string[], char **buf);
int send_packet(int sock, unsigned packet_type, char *data, int data_len);
char* read_string(int sock);
int read_unsigned(int sock, unsigned *data);

#endif /* PACKETS_H */
