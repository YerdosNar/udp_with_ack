#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include "packet.h"

#define TIMEOUT_INTERVAL 3

int waiting_ack = 0;
int sockfd;
struct sockaddr_in receiver_addr;
socklen_t addrlen = sizeof(receiver_addr);
Packet last_packet;
FILE *log_fp;

void print_progress_bar(int sent_bytes, int total_bytes) {
    const int bar_width = 50;
    float percentage = (float)sent_bytes*1.0 / total_bytes;
    int pos = (int)(bar_width * percentage);

    printf("\r[");
    for(int i = 0; i < bar_width; ++i) {
        if(i < pos) printf("#");
        else printf("-");
    }
    printf("] %3d%%", (int)(percentage * 100));
    fflush(stdout);
}

void log_event(const char* event, Packet *pkt) {
    time_t now = time(NULL);
    fprintf(log_fp, "[%ld] %s - type: %d, seqNum: %d, ackNum: %d, len: %d\n",
            now, event, pkt->type, pkt->seqNum, pkt->ackNum, pkt->length);
    fflush(log_fp);
}

void handle_timeout(int sig) {
    if(waiting_ack) {
        fprintf(log_fp, "Timeout occurred. Retransmitting packet %d...\n", last_packet.seqNum);
        fflush(log_fp);
        sendto(sockfd, &last_packet, sizeof(last_packet), 0, (struct sockaddr *)&receiver_addr, addrlen);
        log_event("RETRANSMIT", &last_packet);
        alarm(TIMEOUT_INTERVAL);
    }
}

int drop(float prob) {
    return ((float)rand() / RAND_MAX) < prob;
}

int main(int argc, char *argv[]) {
    if(argc != 7) {
        fprintf(stderr, "Usage: %s <sender_port> <receiver_ip> <receiver_port> <timeout> <filename> <prob>\n", argv[0]);
        exit(1);
    }

    int sender_port = atoi(argv[1]);
    char *receiver_ip = argv[2];
    int receiver_port = atoi(argv[3]);
    int timeout = atoi(argv[4]);
    char *filename = argv[5];
    double ack_drop_prob = atof(argv[6]);

    int total_sent = 0, f_size;

    signal(SIGALRM, handle_timeout);
    srand(time(NULL));

    log_fp = fopen("udp_logs", "a");
    if(!log_fp) {
        perror("Failed to open log file");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    struct sockaddr_in sender_addr;
    memset(&sender_addr, 0, sizeof(sender_addr));
    sender_addr.sin_family = AF_INET;
    sender_addr.sin_addr.s_addr = INADDR_ANY;
    sender_addr.sin_port = htons(sender_port);

    if(bind(sockfd, (struct sockaddr *)&sender_addr, sizeof(sender_addr)) < 0) {
        perror("bind failed");
        exit(1);
    }

    memset(&receiver_addr, 0, sizeof(receiver_addr));
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_port = htons(receiver_port);

    if(inet_pton(AF_INET, receiver_ip, &receiver_addr.sin_addr) <= 0) {
        perror("Invalid receiver IP");
        exit(1);
    }

    Packet greet_pkt = {0};
    greet_pkt.type = TYPE_DATA;
    greet_pkt.seqNum = 0;
    strcpy(greet_pkt.data, "Greeting");
    int sent = sendto(sockfd, &greet_pkt, sizeof(greet_pkt), 0, (struct sockaddr *)&receiver_addr, addrlen);
    if(sent < 0) {
        perror("Sendto failed.");
        exit(1);
    }

    Packet ack_pkt = {0};
    int recv_len = recvfrom(sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&receiver_addr, &addrlen);
    if(recv_len < 0) {
        perror("Recvfrom failed");
        exit(1);
    }
    if(ack_pkt.type != TYPE_ACK || strcmp(ack_pkt.data, "OK") != 0) {
        fprintf(stderr, "Unexpected response. Aborting.\n");
        exit(1);
    }

    Packet fname_pkt = {0};
    fname_pkt.type = TYPE_DATA;
    fname_pkt.seqNum = 1;
    strncpy(fname_pkt.data, filename, MAX_DATA_SIZE);
    sendto(sockfd, &fname_pkt, sizeof(fname_pkt), 0, (struct sockaddr *)&receiver_addr, addrlen);

    FILE *fp = fopen(filename, "rb");
    if(!fp) {
        perror("fopen failed");
        exit(1);
    }
    fseek(fp, 0L, SEEK_END);
    f_size = ftell(fp);
    rewind(fp);
    sendto(sockfd, &f_size, sizeof(int), 0, (struct sockaddr *)&receiver_addr, addrlen);

    int seq = 3;
    while(1) {
        Packet pkt = {0};
        pkt.type = TYPE_DATA;
        pkt.seqNum = seq;
        pkt.length = fread(pkt.data, 1, MAX_DATA_SIZE, fp);
        last_packet = pkt;

        sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&receiver_addr, addrlen);
        waiting_ack = 1;
        alarm(timeout);

        while(waiting_ack) {
            Packet ack;
            recv_len = recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&receiver_addr, &addrlen);
            if(recv_len > 0 && ack.type == TYPE_ACK && ack.ackNum == seq) {
                if(drop(ack_drop_prob)) {
                    fprintf(log_fp, "ACK %d dropped intentionally\n", ack.ackNum);
                    fflush(log_fp);
                    continue;
                }
                log_event("RECV ACK", &ack);
                alarm(0);
                waiting_ack = 0;
            }
        }
        total_sent += pkt.length;
        print_progress_bar(total_sent, f_size);

        if(pkt.length < MAX_DATA_SIZE) break;
        seq++;
    }

    Packet eot = { .type = TYPE_EOT, .seqNum = seq };
    sendto(sockfd, &eot, sizeof(eot), 0, (struct sockaddr *)&receiver_addr, addrlen);
    log_event("SEND EOT", &eot);
    fclose(fp);
    fclose(log_fp);
    close(sockfd);

    printf("\n");

    return 0;
}
