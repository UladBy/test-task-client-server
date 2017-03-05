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
    pthread_t listener_thread;
    pthread_t cleaner_thread;
    struct server_context_s *context;
} client_t;

typedef LIST_HEAD(clients_list_s, _client) clients_list_t;


typedef struct server_context_s {
    clients_list_t clients_list;
    pthread_mutex_t list_lock;
} server_context_t;

static int server_resive(client_t *client);
void* cleaner(void *data);


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

    pthread_create(&client->listener_thread, NULL, server_listener, client);
    pthread_create(&client->cleaner_thread, NULL, cleaner, client);

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
    return 0;
}

int process_stat(client_t *client)
{
    char *id;
    char *file_name;
    int ret;
    FILE *logfd;

    unsigned ping_counter;
    unsigned setstat_counter;
    unsigned message_counter;
    unsigned message_symbols_resived;
    unsigned message_symbols_send;
    int id_len;
    time_t itime;
    char *str_time;

    id = read_string(client->sock);
    if (id == NULL) return -1;
    ret = read_unsigned(client->sock, &ping_counter);
    if (ret < 0) goto end;
    ret = read_unsigned(client->sock, &setstat_counter);
    if (ret < 0) goto end;
    ret = read_unsigned(client->sock, &message_counter);
    if (ret < 0) goto end;
    ret = read_unsigned(client->sock, &message_symbols_resived);
    if (ret < 0) goto end;
    ret = read_unsigned(client->sock, &message_symbols_send);
    if (ret < 0) goto end;

    id_len = strlen(id);
    file_name = calloc(id_len + 7, 1);
    if (file_name == NULL) goto end;
    strcpy(file_name, id);
    strcpy(file_name+id_len, ".log");

    logfd = fopen(file_name, "a");
    itime = time(NULL);
    str_time = ctime(&itime);
    str_time[strlen(str_time)-1] = '\0';
    //delete end of string

    fprintf(logfd, "[%s] %s: ping %u, message %u, setstat %u, "
            "message symbols resived %u,"
            "message_symbols_send %u\n",
            str_time,
            client->name,
            ping_counter, message_counter, setstat_counter,
            message_symbols_resived, message_symbols_send);
    fclose(logfd);

 end:
    free(id);
    return ret;
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
    client->live_counter = 0;
    switch (packet_type) {
    case MESSAGE:
        message = read_string(client->sock);

        printf("message: %s from %s\n", message, client->name);
        server_broadcast_message(client->context, message, client->name);
        free(message);
        break;
    case SETSTAT:
        ret = process_stat(client);
        break;
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
    client_t *client = (client_t*) data;
    server_context_t *ctx = client->context;
    int ret;

    while (client->live_counter < 6) {
        sleep(1);
        client->live_counter++;
    }

    pthread_kill(client->listener_thread, 15);
    close(client->sock);

    pthread_mutex_lock(&ctx->list_lock);
    LIST_REMOVE(client, node);
    for(client_t *client_item = ctx->clients_list.lh_first;
        client_item != NULL;
        client_item = client_item->node.le_next) {

        ret = send_packet_type(client_item->sock, GETSTAT);
        if (ret < 0) continue;
        send_string(client_item->sock, client->name);
    }
    pthread_mutex_unlock(&ctx->list_lock);

    free(client->name);
    free(client);
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
