#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/* Separate network companion: keeps threads out of the fork-based shell. */
#define MAX_CLIENTS 8
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned long total_requests = 0;
static int active_clients = 0;

static void *handle_client(void *arg) {
    int fd = *(int *)arg;
    free(arg);
    char request[1024];
    ssize_t n = recv(fd, request, sizeof(request)-1, 0);
    if (n > 0) {
        request[n] = '\0';
        pthread_mutex_lock(&lock);
        ++total_requests;
        unsigned long count = total_requests;
        int active = active_clients;
        pthread_mutex_unlock(&lock);
        char response[256];
        int len = snprintf(response, sizeof(response),
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n"
            "{\"service\":\"minishell-status\",\"requests\":%lu,\"active_clients\":%d}\n", count, active);
        if (len > 0) send(fd, response, (size_t)len, MSG_NOSIGNAL);
    }
    close(fd);
    pthread_mutex_lock(&lock);
    --active_clients;
    pthread_mutex_unlock(&lock);
    return NULL;
}
int main(int argc, char **argv) {
    int port = 8080;
    if (argc > 1) {
        char *end;
        long value = strtol(argv[1], &end, 10);
        if (*end || value < 1 || value > 65535) { fprintf(stderr, "invalid port\n"); return 2; }
        port = (int)value;
    }
    signal(SIGPIPE, SIG_IGN);
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) { perror("socket"); return 1; }
    int yes = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons((unsigned short)port)};
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* local-only for safety */
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(listener, 16) < 0) {
        perror("bind/listen"); close(listener); return 1;
    }
    printf("Status server: http://127.0.0.1:%d\n", port); fflush(stdout);
    for (;;) {
        int fd = accept(listener, NULL, NULL);
        if (fd < 0) { if (errno == EINTR) continue; perror("accept"); break; }
        pthread_mutex_lock(&lock);
        if (active_clients >= MAX_CLIENTS) {
            pthread_mutex_unlock(&lock);
            close(fd);
            continue;
        }
        ++active_clients;
        pthread_mutex_unlock(&lock);
        int *client = malloc(sizeof(*client));
        if (!client) { close(fd); pthread_mutex_lock(&lock); --active_clients; pthread_mutex_unlock(&lock); continue; }
        *client = fd;
        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, client) != 0) {
            free(client); close(fd);
            pthread_mutex_lock(&lock); --active_clients; pthread_mutex_unlock(&lock);
        } else pthread_detach(thread);
    }
    close(listener);
    return 1;
}
