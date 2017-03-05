#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "packets.h"


int send_unsigned(int sock, unsigned value)
{
    int ret = write(sock, &value, PACKET_UNSIGNED_SIZE);
    if (ret < 0) {
        fprintf(stderr, "[%s:%s:%i]write error\n",
                __FILE__, __FUNCTION__, __LINE__);
    }
    return ret;
}


int send_packet_type(int sock, unsigned packet_type)
{
    return send_unsigned(sock, packet_type);
}


int send_string(int sock, const char string[])
{
    unsigned len = strlen(string);
    int ret;

    ret = send_unsigned(sock, len);
    if (ret < 0) return -1;
    ret = write(sock, string, len);
    if (ret < 0) {
        fprintf(stderr, "[%s:%s:%i]write error\n",
                __FILE__, __FUNCTION__, __LINE__);
    }

    return ret;
}


char* read_string(int sock)
{
    char *string = NULL;
    unsigned len;
    int ret;

    ret = read_unsigned(sock, &len);

    string = calloc(len+1, sizeof(char));
    if (string == NULL) {
        fprintf(stderr, "[%s:%s:%i]allocation error\n",
                __FILE__, __FUNCTION__, __LINE__);
        return NULL;
    }

    ret = read(sock, string, len);
    if (ret < 0) {
        fprintf(stderr, "[%s:%s:%i]read error\n",
                __FILE__, __FUNCTION__, __LINE__);
        free(string);
        return NULL;
    }
    if (ret < len * sizeof(char)) {
        fprintf(stderr, "[%s:%s:%i]WARNING: strig shorter then described\n",
                __FILE__, __FUNCTION__, __LINE__);
    }

    return string;
}


int read_unsigned(int sock, unsigned *data)
{
    int read_size;

    read_size = read(sock, data, PACKET_UNSIGNED_SIZE);
    if (read < 0) {
        fprintf(stderr, "[%s:%s:%i]socket read error\n",
                __FILE__, __FUNCTION__, __LINE__);
        return -1;
    } else if (read_size < PACKET_UNSIGNED_SIZE) {
        fprintf(stderr, "[%s:%s:%i]data too small\n",
                __FILE__, __FUNCTION__, __LINE__);

        return -1;
    }

    return 0;
}
