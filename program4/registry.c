// registry.c
// EECE 446 - Program 4
// Spring 2025
// Author: Erik Flores-Siemsen

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>

#define MAX_PEERS 5
#define MAX_FILES 10
#define MAX_FILENAME_LEN 100
#define MAX_LINE 1024

struct peer_entry {
    uint32_t id;
    int sockfd;
    char files[MAX_FILES][MAX_FILENAME_LEN];
    int file_count;
    struct sockaddr_in addr;
};

struct peer_entry peers[MAX_PEERS];
int peer_count = 0;

int find_peer_index_by_sock(int sockfd) {
    for (int i = 0; i < peer_count; i++) {
        if (peers[i].sockfd == sockfd) {
            return i;
        }
    }
    return -1;
}

void remove_peer(int index) {
    close(peers[index].sockfd);
    if (index != peer_count - 1) {
        peers[index] = peers[peer_count - 1];
    }
    peer_count--;
}

void handle_join(int sockfd, uint8_t *buffer, ssize_t nbytes) {
    if (nbytes < 5 || peer_count >= MAX_PEERS) {
        return;
    }

    uint32_t id;
    memcpy(&id, &buffer[1], 4);
    id = ntohl(id);

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    getpeername(sockfd, (struct sockaddr *)&addr, &len);

    peers[peer_count].id = id;
    peers[peer_count].sockfd = sockfd;
    peers[peer_count].addr = addr;
    peers[peer_count].file_count = 0;
    peer_count++;

    printf("TEST] JOIN %u\n", id);
}

void handle_publish(int sockfd, uint8_t *buffer, ssize_t nbytes) {
    int idx = find_peer_index_by_sock(sockfd);
    if (idx < 0 || nbytes < 5) {
        return;
    }

    uint32_t count;
    memcpy(&count, &buffer[1], 4);
    count = ntohl(count);

    char *ptr = (char *)&buffer[5];
    int added = 0;

    for (uint32_t i = 0; i < count && added < MAX_FILES; i++) {
        if (ptr >= (char *)buffer + nbytes) {
            break;
        }
        strncpy(peers[idx].files[added], ptr, MAX_FILENAME_LEN);
        peers[idx].files[added][MAX_FILENAME_LEN - 1] = '\0';
        ptr += strlen(ptr) + 1;
        added++;
    }

    peers[idx].file_count = added;

    printf("TEST] PUBLISH %d", added);
    for (int i = 0; i < added; i++) {
        printf(" %s", peers[idx].files[i]);
    }
    printf("\n");
}

void handle_search(int sockfd, uint8_t *buffer, ssize_t nbytes) {
    if (nbytes < 2) {
        return;
    }

    char *filename = (char *)&buffer[1];
    uint32_t id = 0, ip = 0;
    uint16_t port = 0;
    uint8_t response[10];

    for (int i = 0; i < peer_count; i++) {
        for (int j = 0; j < peers[i].file_count; j++) {
            if (strcmp(peers[i].files[j], filename) == 0) {
                id = peers[i].id;
                ip = ntohl(peers[i].addr.sin_addr.s_addr);
                port = ntohs(peers[i].addr.sin_port);
                goto found;
            }
        }
    }

found:
    *(uint32_t *)&response[0] = htonl(id);
    *(uint32_t *)&response[4] = htonl(ip);
    *(uint16_t *)&response[8] = htons(port);
    send(sockfd, response, 10, 0);

    struct in_addr addr;
    addr.s_addr = htonl(ip);
    char ipstr[INET_ADDRSTRLEN] = "0.0.0.0";
    if (ip != 0) {
        inet_ntop(AF_INET, &addr, ipstr, sizeof(ipstr));
    }

    printf("TEST] SEARCH %s %u %s:%u\n", filename, id, ipstr, port);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(atoi(argv[1]));

    if (bind(listen_sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(listen_sock, 5) < 0) {
        perror("listen");
        return 1;
    }

    fd_set all_sockets;
    FD_ZERO(&all_sockets);
    FD_SET(listen_sock, &all_sockets);
    int maxfd = listen_sock;

    while (1) {
        fd_set call_set = all_sockets;
        if (select(maxfd + 1, &call_set, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        for (int fd = 0; fd <= maxfd; fd++) {
            if (!FD_ISSET(fd, &call_set)) {
                continue;
            }

            if (fd == listen_sock) {
                struct sockaddr_in client;
                socklen_t len = sizeof(client);
                int newfd = accept(listen_sock, (struct sockaddr *)&client, &len);
                if (newfd >= 0) {
                    FD_SET(newfd, &all_sockets);
                    if (newfd > maxfd) {
                        maxfd = newfd;
                    }
                }
            } else {
                uint8_t buffer[MAX_LINE];
                ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
                if (n <= 0) {
                    int idx = find_peer_index_by_sock(fd);
                    if (idx >= 0) {
                        remove_peer(idx);
                    }
                    FD_CLR(fd, &all_sockets);
                    close(fd);
                    continue;
                }

                switch (buffer[0]) {
                    case 0: {
                        handle_join(fd, buffer, n);
                        break;
                    }
                    case 1: {
                        handle_publish(fd, buffer, n);
                        break;
                    }
                    case 2: {
                        handle_search(fd, buffer, n);
                        break;
                    }
                }
            }
        }
    }

    return 0;
}
