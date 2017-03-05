#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "packets.h"

int build_string(const char string[], char **buf)
{
    unsigned len = strlen(string);
    unsigned buf_len = len + 4;

    *buf = malloc(buf_len);
    if (*buf == NULL) {
        fprintf(stderr, "[%s:%s:%i]allocation error\n",
                __FILE__, __FUNCTION__, __LINE__);
        return -1;
    }
    *((unsigned *)(*buf)) = len;
    strcpy((*buf)+4, string);
    return buf_len;
}


int send_packet(int sock, unsigned packet_type, char *data, int data_len)
{
    write(sock, &packet_type, PACKET_UNSIGNED_SIZE);
    if (data != NULL) {
        write(sock, data, data_len);
    }

    return 0;
}


int send_unsigned(int sock, unsigned value)
{
    return write(sock, &value, PACKET_UNSIGNED_SIZE);
}


int send_packet_type(int sock, unsigned packet_type)
{
    return send_unsigned(sock, packet_type);
}


int send_string(int sock, const char string[])
{
    unsigned len = strlen(string);
    int ret;

    send_unsigned(sock, len);
    write(sock, string, len);

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
        fprintf(stderr, "[%s:%s:%i]data too small %i\n",
                __FILE__, __FUNCTION__, __LINE__, read_size);

        return -1;
    }

    return 0;
}
