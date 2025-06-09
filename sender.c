#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include "packet.h"

#define TIMEOUT_INTERVAL 3

int sockfd;
struct sockaddr_in receiver_addr;
socklen_t addrlen = sizeof(receiver_addr);
Packet last_packet;
int waiting_for_ack = 0;
float drop_prob;

void log_event(const char* event, Packet *pkt) {
    time_t now = time(NULL);
    printf("[%ld] %s - type: %d, seq: %d, ack: %d, len: %d\n",
           now, event, pkt->type, pkt->seqNum, pkt->ackNum, pkt->length);
}

void handle_timeout(int sig) {
    if (waiting_for_ack) {
        printf("Timeout occurred. Retransmitting packet %d...\n", last_packet.seqNum);
        sendto(sockfd, &last_packet, sizeof(last_packet), 0,
               (struct sockaddr *)&receiver_addr, addrlen);
        log_event("RETRANSMIT", &last_packet);
        alarm(TIMEOUT_INTERVAL);
    }
}

int maybe_drop(float prob) {
    return ((float)rand() / RAND_MAX) < prob;
}

int main(int argc, char *argv[]) {
    if (argc != 7) {
        fprintf(stderr, "Usage: %s <sender_port> <receiver_ip> <receiver_port> <timeout> <file> <drop_prob>\n", argv[0]);
        exit(1);
    }

    int sender_port = atoi(argv[1]);
    const char *receiver_ip = argv[2];
    int receiver_port = atoi(argv[3]);
    int timeout = atoi(argv[4]);
    const char *filename = argv[5];
    drop_prob = atof(argv[6]);

    signal(SIGALRM, handle_timeout);
    srand(time(NULL));

    // Setup socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in sender_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(sender_port),
        .sin_addr.s_addr = INADDR_ANY
    };
    bind(sockfd, (struct sockaddr *)&sender_addr, sizeof(sender_addr));

    memset(&receiver_addr, 0, sizeof(receiver_addr));
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_port = htons(receiver_port);
    inet_pton(AF_INET, receiver_ip, &receiver_addr.sin_addr);

    // Read file
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("fopen");
        exit(1);
    }

    int seq = 0;
    while (1) {
        Packet pkt = {0};
        pkt.type = TYPE_DATA;
        pkt.seqNum = seq;
        pkt.length = fread(pkt.data, 1, MAX_DATA_SIZE, fp);
        last_packet = pkt;

        sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&receiver_addr, addrlen);
        log_event("SEND", &pkt);
        waiting_for_ack = 1;
        alarm(timeout);

        while (waiting_for_ack) {
            Packet ack;
            ssize_t r = recvfrom(sockfd, &ack, sizeof(ack), 0, NULL, NULL);
            if (r > 0 && ack.type == TYPE_ACK && ack.ackNum == seq) {
                if (maybe_drop(drop_prob)) {
                    printf("ACK %d dropped\n", ack.ackNum);
                    continue;
                }
                log_event("RECV ACK", &ack);
                alarm(0); // Cancel timer
                waiting_for_ack = 0;
            }
        }

        if (pkt.length < MAX_DATA_SIZE)
            break;
        seq++;
    }

    // Send EOT
    Packet eot = { .type = TYPE_EOT, .seqNum = seq };
    sendto(sockfd, &eot, sizeof(eot), 0, (struct sockaddr *)&receiver_addr, addrlen);
    log_event("SEND EOT", &eot);
    fclose(fp);
    close(sockfd);
    return 0;
}

