#ifndef PACKET_H
#define PACKET_H

#define MAX_DATA_SIZE 1000 // why 1000, not 1024? To avoid fragmentation issues.

// types
#define TYPE_DATA 1
#define TYPE_ACK 2
#define TYPE_EOT 3

typedef struct {
    int type;
    int seqNum;
    int ackNum;
    int length;
    char data[MAX_DATA_SIZE];
} Packet;

#endif
