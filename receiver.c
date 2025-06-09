#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include "packet.h"

void log_event(const char* event, Packet *pkt) {
    time_t now = time(NULL);
    printf("[%ld] %s - type: %d, seq: %d, ack: %d, len: %d\n",
           now, event, pkt->type, pkt->seqNum, pkt->ackNum, pkt->length);
}

int maybe_drop(float prob) {
    return ((float)rand() / RAND_MAX) < prob;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <receiver_port> <drop_prob>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    float drop_prob = atof(argv[2]);
    srand(time(NULL));

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in receiver_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY
    };
    bind(sockfd, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr));

    FILE *fp = fopen("received_output", "wb");
    int expected_seq = 0;

    struct sockaddr_in sender_addr;
    socklen_t addrlen = sizeof(sender_addr);

    while (1) {
        Packet pkt;
        ssize_t r = recvfrom(sockfd, &pkt, sizeof(pkt), 0,
                             (struct sockaddr *)&sender_addr, &addrlen);
        if (r <= 0)
            continue;

        if (maybe_drop(drop_prob)) {
            printf("Packet %d dropped\n", pkt.seqNum);
            continue;
        }

        if (pkt.type == TYPE_DATA && pkt.seqNum == expected_seq) {
            log_event("RECV DATA", &pkt);
            fwrite(pkt.data, 1, pkt.length, fp);

            Packet ack = { .type = TYPE_ACK, .ackNum = expected_seq };
            sendto(sockfd, &ack, sizeof(ack), 0,
                   (struct sockaddr *)&sender_addr, addrlen);
            log_event("SEND ACK", &ack);
            expected_seq++;
        } else if (pkt.type == TYPE_DATA) {
            // duplicate packet
            Packet ack = { .type = TYPE_ACK, .ackNum = pkt.seqNum };
            sendto(sockfd, &ack, sizeof(ack), 0,
                   (struct sockaddr *)&sender_addr, addrlen);
            printf("Duplicate packet %d, ACK resent\n", pkt.seqNum);
        } else if (pkt.type == TYPE_EOT) {
            log_event("RECV EOT", &pkt);
            break;
        }
    }

    fclose(fp);
    close(sockfd);
    return 0;
}

