#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include "packet.h"

FILE *log_fp;

void print_progress_bar(int received_bytes, int total_bytes) {
    const int bar_width = 50;
    float percentage = (float)received_bytes*1.0 / total_bytes;
    int pos = (int)(bar_width * percentage);

    printf("\r[");
    for(int i = 0; i < bar_width; ++i) {
        if(i < pos) printf("#");
        else printf("-");
    }
    printf("] %3d%%", (int)(percentage * 100));
    fflush(stdout);
}

void log_event(const char *event, Packet *pkt) {
    time_t now = time(NULL);
    fprintf(log_fp, "[%ld] %s - type: %d, seqNum: %d, ackNum: %d, len: %d\n",
            now, event, pkt->type, pkt->seqNum, pkt->ackNum, pkt->length);
    fflush(log_fp);
}

int drop(float prob) {
    return ((float)rand() / RAND_MAX) < prob;
}

int main(int argc, char *argv[]) {
    if(argc != 3) {
        fprintf(stderr, "Usage: %s <receiver_port> <drop_prob>\n", argv[0]);
        exit(1);
    }

    int receiver_port = atoi(argv[1]);
    int total_received = 0, f_size;
    float drop_prob = atof(argv[2]);
    srand(time(NULL));

    log_fp = fopen("udp_logs", "a");
    if (!log_fp) {
        perror("Failed to open log file");
        exit(1);
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in receiver_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(receiver_port),
        .sin_addr.s_addr = INADDR_ANY
    };
    bind(sockfd, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr));
    struct sockaddr_in sender_addr;
    socklen_t addlen = sizeof(sender_addr);

    Packet greet_pkt = {0};
    recvfrom(sockfd, &greet_pkt, sizeof(greet_pkt), 0, (struct sockaddr *)&sender_addr, &addlen);
    if(greet_pkt.type != TYPE_DATA || strcmp(greet_pkt.data, "Greeting") != 0) {
        fprintf(stderr, "Invalid greeting\n");
        exit(1);
    }

    Packet ok = { .type = TYPE_ACK };
    strcpy(ok.data, "OK");
    sendto(sockfd, &ok, sizeof(ok), 0, (struct sockaddr *)&sender_addr, addlen);

    Packet fname = {0};
    recvfrom(sockfd, &fname, sizeof(fname), 0, (struct sockaddr *)&sender_addr, &addlen);
    recvfrom(sockfd, &f_size, sizeof(int), 0, (struct sockaddr *)&sender_addr, &addlen);

    char filename[128];
    snprintf(filename, sizeof(filename), "recv_%s", fname.data);
    FILE *fp = fopen(filename, "wb");
    if(!fp) {
        perror("Failed to open output file.");
        exit(1);
    }

    int expected_seq = 3;
    while(1) {
        Packet pkt = {0};
        ssize_t len = recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&sender_addr, &addlen);

        if(len < 0) continue;

        if(pkt.type == TYPE_DATA) {
            if(pkt.seqNum == expected_seq) {
                fwrite(pkt.data, 1, pkt.length, fp);
                total_received += pkt.length;
                expected_seq++;
            }
            if(drop(drop_prob)) continue;
            Packet ack = { .type = TYPE_ACK, .ackNum = pkt.seqNum };
            sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&sender_addr, addlen);
            log_event("SEND ACK", &ack);
            print_progress_bar(total_received, f_size);
        } else if(pkt.type == TYPE_EOT) {
            log_event("RECV EOT", &pkt);
            break;
        }
    }
    fclose(fp);
    fclose(log_fp);
    close(sockfd);

    printf("\n");

    return 0;
}
