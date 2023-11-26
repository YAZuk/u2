#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <memory.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>


#pragma pack(1)

static const int SIZE_BUFFER = 2;
static const uint32_t TYPE_MESSAGE_AZIMUTH = 1;
static const uint32_t TYPE_MESSAGE_DISTANCE = 2;
static const uint32_t TYPE_MESSAGE_SCALE = 3;
static const uint32_t TYPE_MESSAGE_CONFIRMATION = 4;
static const float CMR_AZIMUTH = 90.0 / pow(2.0, 14.0);
static const float CMR_DISTANCE = 512.0 / pow(2.0, 11.0);
int sockSend = 0;
struct sockaddr_in addressSend;

typedef struct {
    uint32_t type;
} Header;

typedef struct {
    Header header;
    uint32_t id: 8;
    uint32_t : 5;
    uint32_t azimuth: 15;
    uint32_t sign: 1;
    uint32_t: 2;
    uint32_t isNew: 1;
} StructAzimuth;

typedef struct {
    Header header;
    uint32_t id: 8;
    uint32_t : 8;
    uint32_t distance: 12;
    uint32_t : 3;
    uint32_t isNew: 1;
} StructDistance;

typedef struct {
    Header header;
    uint32_t id: 8;
    uint32_t : 9;
    uint32_t scale: 12;
    uint32_t : 2;
    uint32_t isNew: 1;
} StructScale;

typedef struct {
    Header header;
    uint32_t typeConfirmation: 8;
    uint32_t idMessage: 24;
} StructConfirmation;


int sendToU2(uint32_t id, uint32_t typeMessage) {
    fd_set setWrite;
    ssize_t countBytes = 0;
    StructConfirmation confirmation;
    memset(&confirmation, 0, sizeof(StructConfirmation));
    confirmation.header.type = TYPE_MESSAGE_CONFIRMATION;
    confirmation.idMessage = id;
    confirmation.typeConfirmation = typeMessage;


    FD_ZERO(&setWrite);
    FD_SET(sockSend, &setWrite);
    if (select(sockSend + 1, NULL, &setWrite, NULL, NULL) != -1) {
        if (FD_ISSET(sockSend, &setWrite)) {

            fprintf(stdout, "Send confirmation: %u %u\n", confirmation.idMessage, confirmation.typeConfirmation);

            for (int i = 0; i < SIZE_BUFFER; i++)
                *((uint32_t * ) & confirmation + i) = htonl(*((uint32_t * ) & confirmation + i));


            if ((countBytes = sendto(sockSend, &confirmation, sizeof(StructConfirmation), 0,
                                     (struct sockaddr *) &addressSend,
                                     sizeof(struct sockaddr_in))) == -1) {
                perror("Perror:");
                return -1;
            }
        }
    }
    return 0;

}

void *receiveThread(void *arg) {
    int *sock = (int *) arg;
    fd_set setReceive;
    struct sockaddr_in addressClient;
    socklen_t len = 0;
    ssize_t countBytes = 0;
    uint32_t buffer[SIZE_BUFFER];

    for (;;) {
        FD_ZERO(&setReceive);
        FD_SET(*sock, &setReceive);
        if (select(*sock + 1, &setReceive, NULL, NULL, NULL) == -1) {
            fprintf(stdout, "Error select receive\n");
            continue;
        }

        if (FD_ISSET(*sock, &setReceive)) {
            if ((countBytes = recvfrom(*sock, buffer,
                                       SIZE_BUFFER * sizeof(unsigned long), 0,
                                       (struct sockaddr *) &addressClient, &len)) != -1) {
                for (int i = 0; i < SIZE_BUFFER; i++)
                    buffer[i] = ntohl(buffer[i]);

                Header *header = (Header *) &buffer;
                switch (header->type) {
                    case TYPE_MESSAGE_AZIMUTH: {
                        StructAzimuth *azimuth = (StructAzimuth *) &buffer;
                        if (azimuth->sign == 1)
                            fprintf(stdout, "%u) azimuth -> %f\n", azimuth->id,
                                    -180.0 + azimuth->azimuth * CMR_AZIMUTH);
                        else
                            fprintf(stdout, "%u) azimuth -> %f\n", azimuth->id, azimuth->azimuth * CMR_AZIMUTH);

                        sendToU2(azimuth->id, TYPE_MESSAGE_AZIMUTH);
                        break;
                    }
                    case TYPE_MESSAGE_DISTANCE: {
                        StructDistance *distance = (StructDistance *) &buffer;
                        fprintf(stdout, "%u) distance -> %f\n", distance->id, distance->distance * CMR_DISTANCE);
                        sendToU2(distance->id, TYPE_MESSAGE_DISTANCE);
                        break;

                    }
                    default:
                        break;
                }
            }
        }
    }
    return NULL;
}


int main(int argc, char **argv) {


    int errorThread = 0;
    int portReceive = 25001;
    int portSend = 25000;
    int on = 1;
    int socketReceive = 0;
    struct sockaddr_in addressReceive;
    pthread_t threadReceive = 0;
    pthread_t threadSend = 0;

    memset(&addressReceive, 0, sizeof(struct sockaddr_in));
    memset(&addressSend, 0, sizeof(struct sockaddr_in));

    addressReceive.sin_family = AF_INET;
    addressReceive.sin_port = htons(portReceive);

    addressSend.sin_family = AF_INET;
    addressSend.sin_port = htons(portSend);
    addressSend.sin_addr.s_addr = htonl(inet_network("127.0.0.1"));

    if ((socketReceive = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        fprintf(stdout, "Error socket receive\n");
        return -1;
    }

    if ((sockSend = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        fprintf(stdout, "Error socket send\n");
        return -1;
    }


    if (setsockopt(socketReceive, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
        fprintf(stdout, "Error setsockopt socket receive\n");
        return -1;
    }

    if (bind(socketReceive, (struct sockaddr *) &addressReceive, sizeof(struct sockaddr_in)) == -1) {
        fprintf(stdout, "Error bind socket receive\n");
        return -1;
    }


    if ((errorThread = pthread_create(&threadReceive, NULL, receiveThread, &socketReceive)) != 0) {
        fprintf(stdout, "Error pthread_create receive\n");
        return -1;
    }

    pthread_join(threadReceive, NULL);

    return 0;
}
