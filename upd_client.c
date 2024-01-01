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
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>

#define SERVER_PORT "4950"
#define BUFFER_SIZE 500

struct Packet {
    int seq_no;
    int size;
    char data[BUFFER_SIZE];
    struct timespec send_time; // Added for timestamping
};

int clientSocket;
struct addrinfo serverAddress, *serverInfo, *p;
struct sockaddr_storage serverStorage;
socklen_t serverStorageLen = sizeof(struct sockaddr_storage);
int returnValue;

int dataSize;
int bytesRead;
struct stat fileStat;
int fileDescriptor;
off_t fileSize;

struct Packet packets[5];
int tempSeqNo = 1;
int acksCount;
int tempAck;
int acks[5];
int packetsCount = 5;

// Added variables for RTT calculation
long totalRtt = 0;
int packetsTransferred = 0;

void *getServerIPAddress(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}



void *receiveAcks(void* vargp) {
    for (int i = 0; i < packetsCount; i++) {
        RECEIVE:
        if((returnValue = recvfrom(clientSocket, &tempAck, sizeof(int), 0, (struct sockaddr*) &serverStorage, &serverStorageLen)) < 0) {
            perror("UDP Client: recvfrom");
            exit(1);
        } 

        if (acks[tempAck] == 1) {
            goto RECEIVE; 
        }

        printf("Acknowledgment Received: %d\n", tempAck);
        acks[tempAck] = 1;
        acksCount++;

        // Calculate RTT for the acknowledged packet
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);

        long rtt_ns = (current_time.tv_sec - packets[i].send_time.tv_sec) * 1e9 +
                       (current_time.tv_nsec - packets[i].send_time.tv_nsec);

        totalRtt += rtt_ns;
        packetsTransferred++;
    }
    return NULL;
}

int main(int argc, char* argv[]) {

    if (argc != 2) {
        fprintf(stderr, "UDP Client: usage: Client hostname\n");
        exit(1);
    }

    memset(&serverAddress, 0, sizeof serverAddress);
    serverAddress.ai_family = AF_UNSPEC;
    serverAddress.ai_socktype = SOCK_DGRAM;

    if ((returnValue = getaddrinfo(argv[1], SERVER_PORT, &serverAddress, &serverInfo)) != 0) {
        fprintf(stderr, "UDP Client: getaddrinfo: %s\n", gai_strerror(returnValue));
        return 1;
    }

    for(p = serverInfo; p != NULL; p = p->ai_next) {
        if ((clientSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("UDP Client: socket");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "UDP Client: Failed to create socket\n");
        return 2;
    }

    pthread_t threadId;
    struct timespec sleepTime, remainingTime;
    sleepTime.tv_sec = 0;
    sleepTime.tv_nsec = 300000000L;

    FILE * inputFile = fopen("input_video.mp4", "rb");
    
    if (inputFile == NULL) {
        perror("Error in opening the video file.\n");
        return 0;
    }

    fileDescriptor = fileno(inputFile);
    fstat(fileDescriptor, &fileStat);
    fileSize = fileStat.st_size;
    printf("Size of Video File: %d bytes\n",(int) fileSize);

    FILESIZETRANSFER:
    if(sendto(clientSocket, &fileSize, sizeof(off_t), 0, p->ai_addr, p->ai_addrlen) < 0) {
        goto FILESIZETRANSFER;
    }

    dataSize = 1;

    while (dataSize > 0) {
        tempSeqNo = 0;
        for (int i = 0; i < packetsCount; i++) {
            dataSize = fread(packets[i].data, 1, BUFFER_SIZE, inputFile);
            packets[i].seq_no = tempSeqNo;
            packets[i].size = dataSize;
            tempSeqNo++;

            if (dataSize == 0) {
                printf("End of file reached.\n");
                packets[i].size = -1;
                packetsCount = i + 1;
                break;
            }
        }

        for (int i = 0; i < packetsCount; i++) {
            printf("Sending packet %d\n", packets[i].seq_no);

            // Timestamp the packet before sending
            clock_gettime(CLOCK_MONOTONIC, &packets[i].send_time);

            if(sendto(clientSocket, &packets[i], sizeof(struct Packet), 0, p->ai_addr, p->ai_addrlen) < 0) {
                perror("UDP Client: sendto");
                exit(1);
            }            
        }

        for (int i = 0; i < packetsCount; i++) {
            acks[i] = 0;
        }

        acksCount = 0;

        pthread_create(&threadId, NULL, receiveAcks, NULL);
        nanosleep(&sleepTime, &remainingTime);

        RESEND:
        for (int i = 0; i < packetsCount; i++) {
            if (acks[i] == 0) {
                printf("Sending missing packet: %d\n", packets[i].seq_no);
                
                // Timestamp the resent packet
                clock_gettime(CLOCK_MONOTONIC, &packets[i].send_time);

                if(sendto(clientSocket, &packets[i], sizeof(struct Packet), 0, p->ai_addr, p->ai_addrlen) < 0) {
                    perror("UDP Client: sendto");
                    exit(1);
                }
            }
        }

        if (acksCount != packetsCount) {
            nanosleep(&sleepTime, &remainingTime);
            goto RESEND;
        }

        pthread_join(threadId, NULL);
    }

    freeaddrinfo(serverInfo);
    printf("\nFile transfer completed successfully!\n");

    // Calculate RTT for each packet and print
    for (int i = 0; i < packetsCount; i++) {
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);

        long rtt_ns = (current_time.tv_sec - packets[i].send_time.tv_sec) * 1e9 +
                       (current_time.tv_nsec - packets[i].send_time.tv_nsec);

        printf("Packet %d RTT: %ld ns\n", packets[i].seq_no, rtt_ns);
    }
     long averageRtt = totalRtt / packetsTransferred;
    printf("\nAverage RTT for %d packets: %ld ns (%.6f seconds)\n", packetsTransferred, averageRtt, averageRtt / 1e9);

    close(clientSocket);
    return 0;
}

