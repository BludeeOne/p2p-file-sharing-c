#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#define CHUNK_MAX_SIZE 1000
#define PORT "80"

void print_usage(char *program_name) {
    fprintf(stderr, "Usage: %s <chunk_size>\n", program_name);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    int chunk_size = atoi(argv[1]);
    if (chunk_size <= 0 || chunk_size > CHUNK_MAX_SIZE) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo("www.ecst.csuchico.edu", PORT, &hints, &res) != 0) {
        perror("getaddrinfo");
        return EXIT_FAILURE;
    }

    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) {
        perror("socket");
        freeaddrinfo(res);
        return EXIT_FAILURE;
    }

    if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("connect");
        close(sockfd);
        freeaddrinfo(res);
        return EXIT_FAILURE;
    }

    freeaddrinfo(res);
    const char *request = "GET /~kkredo/file.html HTTP/1.0\r\n\r\n";
    if (send(sockfd, request, strlen(request), 0) == -1) {
        perror("send");
        close(sockfd);
        return EXIT_FAILURE;
    }

    char buffer[CHUNK_MAX_SIZE + 1];
    int h1_count = 0;
    int total_bytes = 0;

    while (1) {
        int bytes_received = recv(sockfd, buffer, chunk_size, 0);
        if (bytes_received == -1) {
            perror("recv");
            close(sockfd);
            return EXIT_FAILURE;
        } else if (bytes_received == 0) {
            break;  
        }

        total_bytes += bytes_received;
        buffer[bytes_received] = '\0';

        char *ptr = buffer;
        while ((ptr = strstr(ptr, "<h1>")) != NULL) {
            h1_count++;
            ptr += 4; 
        }
    }
    printf("Number of <h1> tags: %d\n", h1_count);
    printf("Number of bytes: %d\n", total_bytes);

    close(sockfd);
    return EXIT_SUCCESS;
}
