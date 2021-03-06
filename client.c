#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "packets.h"

#define PING_PERIOD 2

typedef struct {
    pthread_mutex_t socket_write_lock;
    int sock;
    char is_runing;
    pthread_t pinger_thread;
    pthread_t listener_thread;
    unsigned ping_counter;
    unsigned setstat_counter;
    unsigned message_counter;
    unsigned message_symbols_resived;
    unsigned message_symbols_send;
} client_context_t;


static int client_connect(int sock, const char name[])
{
    int ret;

    ret = send_packet_type(sock, CONNECT);
    if (ret < 0) return -1;
    ret = send_string(sock, name);

    return ret;
}


static int client_send_message(client_context_t *ctx, const char message[])
{
    int ret;

    pthread_mutex_lock(&ctx->socket_write_lock);
    ret = send_packet_type(ctx->sock, MESSAGE);
    if (ret >= 0)
        ret = send_string(ctx->sock, message);
    pthread_mutex_unlock(&ctx->socket_write_lock);

    return ret;
}


static void* pinger(void *data)
{
    int ret = 0;
    client_context_t *ctx = (client_context_t*)data;

    while (ret >= 0 && ctx->is_runing) {
        sleep(PING_PERIOD);

        pthread_mutex_lock(&ctx->socket_write_lock);
        ret = send_packet_type(ctx->sock, PING);
        pthread_mutex_unlock(&ctx->socket_write_lock);

        ctx->ping_counter++;
    }
}


static int client_context_init(client_context_t *context)
{
    pthread_mutex_init(&context->socket_write_lock, NULL);
    context->is_runing = 1;

    context->ping_counter = 0;
    context->setstat_counter = 0;
    context->message_counter = 0;
    context->message_symbols_resived = 0;
    context->message_symbols_send = 0;

    return 0;
}

int send_stat(client_context_t *ctx, const char id[])
{
    int ret;

    pthread_mutex_lock(&ctx->socket_write_lock);
    ret = send_packet_type(ctx->sock, SETSTAT);
    if (ret < 0) goto end;
    ret = send_string(ctx->sock, id);
    if (ret < 0) goto end;
    ret = send_unsigned(ctx->sock, ctx->ping_counter);
    if (ret < 0) goto end;
    ret = send_unsigned(ctx->sock, ctx->setstat_counter);
    if (ret < 0) goto end;
    ret = send_unsigned(ctx->sock, ctx->message_counter);
    if (ret < 0) goto end;
    ret = send_unsigned(ctx->sock, ctx->message_symbols_resived);
    if (ret < 0) goto end;
    ret = send_unsigned(ctx->sock, ctx->message_symbols_send);

 end:
    pthread_mutex_unlock(&ctx->socket_write_lock);

    return ret;
}


static int client_resive(client_context_t *ctx)
{
    int packet_type;
    int ret;
    char *message;
    char *name;

    ret = read_unsigned(ctx->sock, &packet_type);
    if (ret < 0) {
        return -1;
    }
    switch (packet_type) {
    case MESSAGE:
        name = read_string(ctx->sock);
        if (name == NULL) return -1;
        message = read_string(ctx->sock);
        if (message == NULL) return -1;
        printf("client %s send message: %s\n", name, message);
        ctx->message_symbols_resived += strlen(message);
        free(message);
        free(name);
        break;
    case GETSTAT:
        name = read_string(ctx->sock);
        if (name == NULL) return -1;
        ret = send_stat(ctx, name);
        free(name);
        if (ret < 0) return -1;
        break;
    default:
        fprintf(stderr, "[%s:%s:%i]unkown or forbiden packet_type\n",
                __FILE__, __FUNCTION__, __LINE__);
        }
    return 0;
}


static void* client_listener(void *data)
{
    client_context_t *ctx = (client_context_t*)data;
    int ret = 0;

    while (ret >= 0 && ctx->is_runing) {
        ret = client_resive(ctx);
    }

}


int main(int argc, char *argv[])
{
    client_context_t context;
    struct sockaddr_in serv_addr;
    int ret;

    if (argc < 3) {
        printf("usage:\n\t%s address name\n", argv[0]);
        return 1;
    }

    client_context_init(&context);

    if((context.sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Error: Could not create socket \n");
        return 1;
    }

    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(5000);
    ret = inet_pton(AF_INET, argv[1], &serv_addr.sin_addr);
    if(ret <= 0) {
        printf("\n inet_pton error occured\n");
        return 1;
    }

    ret = connect(context.sock, (struct sockaddr *)&serv_addr,
                  sizeof(serv_addr));
    if(ret < 0) {
        printf("\n Error : Connect Failed \n");
        return 1;
    }

    client_connect(context.sock, argv[2]);

    pthread_create(&context.pinger_thread, NULL, pinger, &context);
    pthread_create(&context.listener_thread, NULL, client_listener, &context);

    while (context.is_runing) {
        char message_text[50];
        scanf("%49s", message_text);
        client_send_message(&context, message_text);
        context.message_counter++;
        context.message_symbols_send += strlen(message_text);
    }
    return 0;
}
