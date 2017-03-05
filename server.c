#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <signal.h>
#include <pthread.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/queue.h>


#include "packets.h"

struct server_context_s;

typedef struct _client {
    LIST_ENTRY(_client) node;
    int sock;
    int live_counter;
    char *name;
    pthread_t thread;
    struct server_context_s *context;
} client_t;

typedef LIST_HEAD(clients_list_s, _client) clients_list_t;


typedef struct server_context_s {
    clients_list_t clients_list;
    pthread_mutex_t list_lock;
} server_context_t;

static int server_resive(client_t *client);

static int server_broadcast_message(server_context_t *ctx,
                                    const char message[],
                                    const char client_name[])
{
    char *name_buf;
    char *message_buf;
    int name_buf_len;
    int message_buf_len;

    int ret;
    client_t *client;

    message_buf_len = build_string(message, &message_buf);
    if (message_buf_len < 0) {
        return -1;
    }

    pthread_mutex_lock(&ctx->list_lock);
    client = ctx->clients_list.lh_first;
    while (client != NULL) {
        name_buf_len = build_string(client_name, &name_buf);
        if (name_buf_len < 0) {
            return -1;
        }

        ret = send_packet(client->sock, MESSAGE, name_buf, name_buf_len);
        write(client->sock, message_buf, message_buf_len);

        free(name_buf);
        client = client->node.le_next;
    }
    pthread_mutex_unlock(&ctx->list_lock);

    free(message_buf);

    return ret;

}

static void* server_listener(void *data)
{
    client_t *client =  (client_t*)data;
    unsigned packet_type;
    int ret = 0;

    while (ret >= 0) {
        ret = server_resive(client);
    }
}

static int connect_client(int sock, server_context_t *ctx)
{
    int packet_type;
    struct _client *client;
    int ret;

    ret = read_unsigned(sock, &packet_type);
    if (ret < 0) {
        return -1;
    }

    if (packet_type != CONNECT) {
        printf("[%s:%s:%i]wrong packet_type %i\n",
               __FILE__, __FUNCTION__, __LINE__,
               packet_type);
        return -1;
    }

    client = malloc(sizeof(client_t));
    if (client == NULL) {
        fprintf(stderr, "[%s:%s:%i]allocation error\n",
                __FILE__, __FUNCTION__, __LINE__);
        return -1;
    }
    client->name = read_string(sock);
    if (client->name == NULL) {
        free(client);
    }

    client->sock = sock;
    client->live_counter = 0;
    client->context = ctx;

    pthread_mutex_lock(&ctx->list_lock);
    LIST_INSERT_HEAD(&ctx->clients_list, client, node);
    pthread_mutex_unlock(&ctx->list_lock);

    pthread_create(&client->thread, NULL, server_listener, client);

    return 0;
}


void print_client(client_t *client)
{
    printf("name: %s\n", client->name);
    printf("live_counter: %i\n", client->live_counter);
}

void print_all_clients(server_context_t *ctx)
{
    struct _client *client;

    pthread_mutex_lock(&ctx->list_lock);
    client = ctx->clients_list.lh_first;
    while (client != NULL) {
        print_client(client);
        client = client->node.le_next;
    }
    pthread_mutex_unlock(&ctx->list_lock);
}

int process_ping(client_t *client)
{
    fprintf(stderr, "[%s:%s:%i]%s PING \n", __FILE__, __FUNCTION__, __LINE__, client->name);
    client->live_counter = 0;
    return 0;
}

static int server_resive(client_t *client)
{
    int packet_type;
    int ret;
    char *message;

    ret = read_unsigned(client->sock, &packet_type);
    if (ret < 0) {
        return -1;
    }
    switch (packet_type) {
    case MESSAGE:
        message = read_string(client->sock);
        client->live_counter = 0;
        printf("message: %s from %s\n", message, client->name);
        server_broadcast_message(client->context, message, client->name);
        free(message);
        break;
    /* case SETSTAT: */
    /*     ret = recive_stat(sock); */
    /*     break; */
    case PING:
        ret = process_ping(client);
        break;
        
    default:
        fprintf(stderr, "[%s:%s:%i]unkown or forbiden packet_type\n",
                __FILE__, __FUNCTION__, __LINE__);
        
        }
}


void* cleaner(void *data)
{
    server_context_t *ctx = (server_context_t*)data;
    client_t *client; 
    while (1) {
        sleep(1);
        pthread_mutex_lock(&ctx->list_lock);
        client = ctx->clients_list.lh_first;
        while (client != NULL) {
            if (client->live_counter < 6) {
                client->live_counter++;
            } else {
                pthread_kill(client->thread, 15);
                close(client->sock);
                LIST_REMOVE(client, node);
                free(client);
            }
            client = client->node.le_next;
        }
        pthread_mutex_unlock(&ctx->list_lock);
    }
}

int server_context_init(server_context_t *ctx)
{
    LIST_INIT(&ctx->clients_list);
    pthread_mutex_init(&ctx->list_lock, NULL);

    return 0;
}

int main(int argc, char *argv[])
{
    int port = 5000;
    int sock;
    int ret;
    int newsock;
    char buffer[10];
    server_context_t context;
    struct sockaddr_in serv_addr;
    pthread_t cleanner_thread;
    pthread_t listener_thread;

    server_context_init(&context);
    pthread_create(&cleanner_thread, NULL, cleaner, &context);
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("socket() failed: %d\n", errno);
        return EXIT_FAILURE;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    ret = bind(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (ret < 0) {
        printf("bind() failed: %d\n", errno);
        return EXIT_FAILURE;
    }
    listen(sock, 10);

    while (1) {
        newsock = accept(sock, (struct sockaddr *) NULL, NULL);
        if (newsock < 0) {
            printf("accept() failed: %d\n", errno);
            return EXIT_FAILURE;
        }
        ret = connect_client(newsock, &context);
        print_all_clients(&context);
    }

    return 0;
}
