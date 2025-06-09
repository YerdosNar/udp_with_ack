#ifndef PACKET_H
#define PACKET_H

#define MAX_DATA_SIZE 1000

#define TYPE_DATA 1
#define TYPE_ACK 2
#define TYPE_EOT 3

typedef struct {
    int type;              // 1 = DATA, 2 = ACK, 3 = EOT
    int seqNum;
    int ackNum;
    int length;            // Length of actual data
    char data[MAX_DATA_SIZE]; // Payload
} Packet;

#endif

