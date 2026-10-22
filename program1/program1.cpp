#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define SERVER_PORT "80"
#define SERVER_HOST "www.ecst.csuchico.edu"
#define REQUEST "GET /~kkredo/file.html HTTP/1.0\r\n\r\n"

int lookup_and_connect(const char *host, const char *service);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <chunk_size>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int chunk_size = atoi(argv[1]);
    if (chunk_size <= 4 || chunk_size > 1000) {
        fprintf(stderr, "Error: Chunk size must be between 5 and 1000 bytes.\n");
        exit(EXIT_FAILURE);
    }
    
    int s = lookup_and_connect(SERVER_HOST, SERVER_PORT);
    if (s < 0) {
        exit(EXIT_FAILURE);
    }
    
    if (send(s, REQUEST, strlen(REQUEST), 0) == -1) {
        perror("send");
        close(s);
        exit(EXIT_FAILURE);
    }
    
    char buffer[chunk_size + 1];
    ssize_t bytes_received, total_bytes = 0;
    int h1_count = 0;
    
    while ((bytes_received = recv(s, buffer, chunk_size, 0)) > 0) {
        total_bytes += bytes_received;
        buffer[bytes_received] = '\0'; 
        
        char *pos = buffer;
        while ((pos = strstr(pos, "<h1>")) != NULL) {
            h1_count++;
            pos += 4; 
        }
    }
    
    if (bytes_received < 0) {
        perror("recv");
    }
    
    close(s);
    
    printf("Number of <h1> tags: %d\n", h1_count);
    printf("Number of bytes: %zd\n", total_bytes);
    
    return 0;
}

int lookup_and_connect(const char *host, const char *service) {
    struct addrinfo hints, *res, *rp;
    int s;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(host, service, &hints, &res) != 0) {
        perror("getaddrinfo");
        return -1;
    }
    
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (s == -1) continue;
        
        if (connect(s, rp->ai_addr, rp->ai_addrlen) != -1) {
            freeaddrinfo(res);
            return s;
        }
        
        close(s);
    }
    
    freeaddrinfo(res);
    return -1;
}

