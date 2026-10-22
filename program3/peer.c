/*****************************************************************************
 * EECE 446 - Program 2/3
 * Spring 2025
 * 
 * Command line usage:
 *   ./peer <registryHost> <registryPort> <peerID>
 *
 * Authors: Erik Flores-Siemsen
 * Date   : April 2025
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>       // for strcmp since no memset
#include <unistd.h>       // close, read, write
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>        // getaddrinfo
#include <arpa/inet.h>    // ntohl, htonl, inet_ntop
#include <dirent.h>       // opendir, readdir
#include <sys/stat.h>     // stat

//send_all sends all length bytes through the socket and returns what was actually sent
ssize_t send_all(int sockfd, const void *buffer, size_t length)
{
    size_t totalSent = 0;
    const char *buf = (const char *)buffer;

    while (totalSent < length) {
        ssize_t sent = send(sockfd, buf + totalSent, length - totalSent, 0);
        if (sent < 0) {
            perror("send error");
            return -1;
        }
        if (sent == 0) {
            break;
        }
        totalSent += sent;
    }
    return (ssize_t)totalSent;
}

//read file names and count how many there are
int read_directory_filenames(const char *dirname, char names[][256], int maxFiles)
{
    DIR *dp = opendir(dirname);
    if (!dp) {
        // incase couldnt open directory 
        return 0;
    }

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL) {
        if (count >= maxFiles) {
            break; 
        }

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char fullpath[512];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirname, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) == 0 && S_ISREG(st.st_mode)) {
            snprintf(names[count], 256, "%s", entry->d_name);
            ++count;
        }
    }
    closedir(dp);
    return count;
}


int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <registryHost> <registryPort> <peerID>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *registryHost = argv[1];
    const char *registryPort = argv[2];
    uint32_t peerID = 0;

    {
        char *endptr = NULL;
        peerID = (uint32_t)strtoul(argv[3], &endptr, 10);
        if (*endptr != '\0') {
            fprintf(stderr, "Invalid peerID: %s\n", argv[3]);
            exit(EXIT_FAILURE);
        }
    }

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints)); 
    hints.ai_family   = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_STREAM;  // TCP

    int rc = getaddrinfo(registryHost, registryPort, &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        exit(EXIT_FAILURE);
    }

    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        perror("socket");
        freeaddrinfo(res);
        exit(EXIT_FAILURE);
    }

    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        freeaddrinfo(res);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(res);

    while (1) {
        printf("Enter a command: ");
        fflush(stdout);

        char cmd[64];
        if (!fgets(cmd, sizeof(cmd), stdin)) {
            break;
        }
        size_t len = strlen(cmd);
        if (len > 0 && cmd[len - 1] == '\n') {
            cmd[len - 1] = '\0';
        }
        if (strcasecmp(cmd, "EXIT") == 0) {
            close(sockfd);
            printf("Exiting peer.\n");
            break;
        }
        else if (strcasecmp(cmd, "JOIN") == 0) {
            char buffer[5];
            buffer[0] = 0;
            uint32_t netID = htonl(peerID);
            buffer[1] = ((char *)&netID)[0];
            buffer[2] = ((char *)&netID)[1];
            buffer[3] = ((char *)&netID)[2];
            buffer[4] = ((char *)&netID)[3];

            ssize_t bytesSent = send_all(sockfd, buffer, sizeof(buffer));
            if (bytesSent < 0) {
                fprintf(stderr, "JOIN request send error.\n");
            } else {
                printf("JOIN sent to registry.\n");
            }
        }
        else if (strcasecmp(cmd, "PUBLISH") == 0) {
            #define MAX_FILES 100
            char fileNames[MAX_FILES][256];
            int fileCount = read_directory_filenames("SharedFiles", fileNames, MAX_FILES);

            char buffer[1200];
            int offset = 0;

            buffer[offset++] = 1;

            uint32_t netCount = htonl((uint32_t)fileCount);
            buffer[offset++] = ((char *)&netCount)[0];
            buffer[offset++] = ((char *)&netCount)[1];
            buffer[offset++] = ((char *)&netCount)[2];
            buffer[offset++] = ((char *)&netCount)[3];

            for (int i = 0; i < fileCount; i++) {
                size_t fn_len = strlen(fileNames[i]) + 1; 
                if (offset + fn_len >= (int)sizeof(buffer)) {
                    fprintf(stderr, "Too many files or filenames too large!\n");
                    fileCount = i; 
                    break;
                }
                memcpy(&buffer[offset], fileNames[i], fn_len);
                offset += fn_len;
            }

            ssize_t bytesSent = send_all(sockfd, buffer, offset);
            if (bytesSent < 0) {
                fprintf(stderr, "PUBLISH request send error.\n");
            } 
            else {
                printf("PUBLISH sent (%d files announced).\n", fileCount);
            }
        }
        else if (strcasecmp(cmd, "SEARCH") == 0) {
            printf("Enter a file name: ");
            fflush(stdout);

            char filename[256];
            if (!fgets(filename, sizeof(filename), stdin)) {
                fprintf(stderr, "Error reading file name.\n");
                continue;
            }
            len = strlen(filename);
            if (len > 0 && filename[len - 1] == '\n') {
                filename[len - 1] = '\0';
            }
            if (strlen(filename) == 0) {
                fprintf(stderr, "SEARCH filename cannot be empty.\n");
                continue;
            }

            char buffer[300];
            int offset = 0;
            buffer[offset++] = 2;
            size_t fn_len = strlen(filename) + 1;
            if (fn_len > (sizeof(buffer) - offset)) {
                fprintf(stderr, "Filename is too large.\n");
                continue;
            }
            memcpy(&buffer[offset], filename, fn_len);
            offset += fn_len;

            if (send_all(sockfd, buffer, offset) < 0) {
                fprintf(stderr, "SEARCH request send error.\n");
                continue;
            }

            uint8_t resp[10];
            ssize_t totalRecv = 0;
            while (totalRecv < 10) {
                ssize_t got = recv(sockfd, resp + totalRecv, 10 - totalRecv, 0);
                if (got < 0) {
                    perror("recv error");
                    totalRecv = -1;
                    break;
                }
                if (got == 0) {
                    
                    fprintf(stderr, "Registry closed connection unexpectedly.\n");
                    totalRecv = -1;
                    break;
                }
                totalRecv += got;
            }
            if (totalRecv < 10) {
                //error or no data
                continue;
            }
            //not sure if this is what you meant but the memcpy does only a single field
            uint32_t foundID = 0;
            memcpy(&foundID, &resp[0], 4);
            foundID = ntohl(foundID);

            uint32_t foundIP = 0;
            memcpy(&foundIP, &resp[4], 4);
            foundIP = ntohl(foundIP);

            uint16_t foundPort = 0;
            memcpy(&foundPort, &resp[8], 2);
            foundPort = ntohs(foundPort);

            if (foundID == 0 && foundIP == 0 && foundPort == 0) {
                printf("File not indexed by registry\n");
            } 
            else {
                struct in_addr ip_addr;
                ip_addr.s_addr = htonl(foundIP);
                char ipStr[INET_ADDRSTRLEN] = {0};
                if (inet_ntop(AF_INET, &ip_addr, ipStr, sizeof(ipStr)) == NULL) {
                    snprintf(ipStr, sizeof(ipStr), "InvalidIP");
                }

                printf("File found at\n");
                printf("Peer %u\n", foundID);
                printf("%s:%u\n", ipStr, foundPort);
            }
        }
        else if(strcasecmp(cmd, "FETCH") == 0) {
            printf("Enter a file name: ");
fflush(stdout);

char filename[256];
if (!fgets(filename, sizeof(filename), stdin)) {
    fprintf(stderr, "Error reading file name.\n");
    continue;
}
len = strlen(filename);
if (len > 0 && filename[len - 1] == '\n') {
    filename[len - 1] = '\0';
}
if (strlen(filename) == 0) {
    fprintf(stderr, "FETCH filename cannot be empty.\n");
    continue;
}

char buffer[300];
int offset = 0;
buffer[offset++] = 2; // 
size_t fn_len = strlen(filename) + 1;
memcpy(&buffer[offset], filename, fn_len);
offset += fn_len;

if (send_all(sockfd, buffer, offset) < 0) {
    fprintf(stderr, "SEARCH request send error.\n");
    continue;
}

uint8_t resp[10];
ssize_t totalRecv = 0;
while (totalRecv < 10) {
    ssize_t got = recv(sockfd, resp + totalRecv, 10 - totalRecv, 0);
    if (got <= 0) {
        perror("recv error");
        totalRecv = -1;
        break;
    }
    totalRecv += got;
}
if (totalRecv < 10) {
    continue;
}

uint32_t foundID = ntohl(*(uint32_t*)&resp[0]);
uint32_t foundIP = ntohl(*(uint32_t*)&resp[4]);
uint16_t foundPort = ntohs(*(uint16_t*)&resp[8]);

if (foundID == 0 && foundIP == 0 && foundPort == 0) {
    printf("File not indexed by registry\n");
    continue;
}

int peerSock = socket(AF_INET, SOCK_STREAM, 0);
if (peerSock < 0) {
    perror("socket");
    continue;
}

struct sockaddr_in peerAddr;
peerAddr.sin_family = AF_INET;
peerAddr.sin_port = htons(foundPort);
peerAddr.sin_addr.s_addr = htonl(foundIP);

if (connect(peerSock, (struct sockaddr*)&peerAddr, sizeof(peerAddr)) < 0) {
    perror("connect to peer");
    close(peerSock);
    continue;
}

char fetchReq[300];
fetchReq[0] = 3; 
memcpy(&fetchReq[1], filename, fn_len);
if (send_all(peerSock, fetchReq, fn_len + 1) < 0) {
    perror("send FETCH");
    close(peerSock);
    continue;
}

uint8_t responseCode = 255;
if (recv(peerSock, &responseCode, 1, 0) <= 0) {
    fprintf(stderr, "Peer did not respond to FETCH.\n");
    close(peerSock);
    continue;
}

if (responseCode != 0) {
    printf("Peer was unable to send the file.\n");
    close(peerSock);
    continue;
}

char outPath[512];
snprintf(outPath, sizeof(outPath), "SharedFiles/%s", filename);
FILE *outFile = fopen(outPath, "wb");
if (!outFile) {
    perror("fopen");
    close(peerSock);
    continue;
}

char recvBuf[1024];
ssize_t bytes;
while ((bytes = recv(peerSock, recvBuf, sizeof(recvBuf), 0)) > 0) {
    fwrite(recvBuf, 1, bytes, outFile);
}
fclose(outFile);
close(peerSock);

printf("File %s saved in SharedFiles/\n", filename);

        }
        else {
            printf("Unrecognized command. Available commands:\n");
            printf("  JOIN\n  PUBLISH\n  SEARCH\n FETCH\n EXIT\n");
        }
    }

    return 0;
}

