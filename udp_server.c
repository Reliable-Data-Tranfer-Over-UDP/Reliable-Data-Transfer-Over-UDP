#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>

#define SERVER_PORT "4950"
#define BUFFER_SIZE 500

struct Packet {
    int seq_no;
    int size;
    char data[BUFFER_SIZE];
};

int serverSocket; 
struct addrinfo serverAddress, *serverInfo, *p; 
struct sockaddr_storage clientAddress;
socklen_t clientAddressLen = sizeof(struct sockaddr_storage);
char clientIP[INET6_ADDRSTRLEN];
int returnValue;

int fileSize;
int remainingBytes = 0;
int receivedBytes = 0;

int packetsCount = 5;
struct Packet tempPacket;
struct Packet packets[5];
int acksCount;
int acks[5];
int tempAck;


void *getIPAddress(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void *receivePackets(void *vargp) {
    for (int i = 0; i < packetsCount; i++) {
        RECEIVE:
        if((returnValue = recvfrom(serverSocket, &tempPacket, sizeof(struct Packet), 0, (struct sockaddr *)&clientAddress, &clientAddressLen)) < 0) {
            perror("UDP Server: recvfrom");
            exit(1);
        }

        if (packets[tempPacket.seq_no].size != 0) {
            packets[tempPacket.seq_no] = tempPacket;
            tempAck = tempPacket.seq_no;
            acks[tempAck] = 1;
            if(sendto(serverSocket, &tempAck, sizeof(int), 0, (struct sockaddr *)&clientAddress, clientAddressLen) < 0){
                perror("UDP Server: sendto");
                exit(1);
            }
            printf("Duplicate Acknowledgment Sent: %d\n", tempAck);
            goto RECEIVE;
        }

        if (tempPacket.size == -1) {
            printf("Last packet found\n");
            packetsCount = tempPacket.seq_no + 1;
        }

        if (returnValue > 0) {
            printf("Packet Received: %d\n", tempPacket.seq_no);
            packets[tempPacket.seq_no] = tempPacket;
        }
    }
    return NULL;
}

int main(void) {
    memset(&serverAddress, 0, sizeof serverAddress);
    serverAddress.ai_family = AF_UNSPEC;
    serverAddress.ai_socktype = SOCK_DGRAM;
    serverAddress.ai_flags = AI_PASSIVE;

    if ((returnValue = getaddrinfo(NULL, SERVER_PORT, &serverAddress, &serverInfo)) != 0) {
        fprintf(stderr, "UDP Server: getaddrinfo: %s\n", gai_strerror(returnValue));
        return 1;
    }

    for(p = serverInfo; p != NULL; p = p->ai_next) {
        if ((serverSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("UDP Server: socket");
            continue;
        }

        if (bind(serverSocket, p->ai_addr, p->ai_addrlen) == -1) {
            close(serverSocket);
            perror("UDP Server: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "UDP Server: Failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(serverInfo);
    printf("UDP Server: Waiting to receive datagrams...\n");

    pthread_t threadId;
    struct timespec sleepTime, remainingTime;
    sleepTime.tv_sec = 0;
    sleepTime.tv_nsec = 30000000L; 

    FILE * outputFile = fopen("output_video.mp4", "wb");
    
    if ((returnValue = recvfrom(serverSocket, &fileSize, sizeof(off_t), 0, (struct sockaddr *)&clientAddress, &clientAddressLen)) < 0) {
        perror("UDP Server: recvfrom");
        exit(1);
    }
    printf("Size of Video File to be received: %d bytes\n", fileSize);

    returnValue = 1;
    remainingBytes = fileSize;

    while (remainingBytes > 0 || (packetsCount == 5)) {
        memset(packets, 0, sizeof(packets));
        for (int i = 0; i < 5; i++) {
            packets[i].size = 0; 
        }

        for (int i = 0; i < 5; i++) {
            acks[i] = 0; 
        }

        pthread_create(&threadId, NULL, receivePackets, NULL);
        nanosleep(&sleepTime, &remainingTime);

        acksCount = 0;

        RESEND_ACK:
        for (int i = 0; i < packetsCount; i++) {
            tempAck = packets[i].seq_no;
            if (acks[tempAck] != 1) {
                if (packets[i].size != 0) {
                    acks[tempAck] = 1;
                    if(sendto(serverSocket, &tempAck, sizeof(int), 0, (struct sockaddr *)&clientAddress, clientAddressLen) > 0) {
                        acksCount++;
                        printf("Acknowledgment sent: %d\n", tempAck);
                    }
                }
            }
        }

        nanosleep(&sleepTime, &remainingTime);
        nanosleep(&sleepTime, &remainingTime);

        if (acksCount < packetsCount) {
            goto RESEND_ACK;
        }

        pthread_join(threadId, NULL);

        for (int i = 0; i < packetsCount; i++) {
            if (packets[i].size != 0 && packets[i].size != -1) {
                printf("Writing packet: %d\n", packets[i].seq_no);
                fwrite(packets[i].data, 1, packets[i].size, outputFile);
                remainingBytes -= packets[i].size;
                receivedBytes += packets[i].size;
            }
        }

        printf("Received data: %d bytes\nRemaining data: %d bytes\n", receivedBytes, remainingBytes);
    }

    printf("\nUDP Server: Received video file from client %s\n", inet_ntop(clientAddress.ss_family, getIPAddress((struct sockaddr *)&clientAddress), clientIP, sizeof clientIP));
    printf("File transfer completed successfully!\n");
    close(serverSocket);
    return 0;
}

