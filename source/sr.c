#include <stdio.h>
#include <string.h> 
#include <stdbool.h>    /* for bool */
#include "emulator.h"
#include "sr.h"

/* Compute the checksum for a packet */
int ComputeChecksum(struct pkt packet) {
    int checksum = 0;
    int i;
    checksum = packet.seqnum;
    checksum += packet.acknum;
    for (i = 0; i < 20; i++)
        checksum += (int)packet.payload[i];
    return checksum;
}

/* Check if a packet is corrupted */
bool IsCorrupted(struct pkt packet) {
    return (packet.checksum != ComputeChecksum(packet));
}

/* Selective-Repeat state definitions */
struct pkt A_window[SR_WINDOW_SIZE];
int       A_acknowledged[SR_WINDOW_SIZE];
int       A_base;
int       A_nextseqnum;

struct pkt B_window[SR_WINDOW_SIZE];
int       B_received[SR_WINDOW_SIZE];
int       B_expected_base;

/* Sender (A) initialization */
void A_init(void)                    
{
  int i;
    A_base        = 0;
    A_nextseqnum  = 0;
    for (i = 0; i < SR_WINDOW_SIZE; i++) {
        A_acknowledged[i] = 0;
        memset(&A_window[i], 0, sizeof(struct pkt));
    }
    if (TRACE > 0)
        printf("----A: SR Sender initialized (base=0, nextseq=0)\n");
}

/* Sender (A) output from layer 5 */
void A_output(struct msg message)    
{
 int i, seq, window_len, idx;
    struct pkt packet;

    seq        = A_nextseqnum;
    window_len = (seq + MAX_SEQ_NUM - A_base) % MAX_SEQ_NUM;

    /* 1) window-full check */
    if (window_len >= SR_WINDOW_SIZE) {
        if (TRACE > 0)
            printf("----A: Send window full, dropping message\n");
        window_full++;
        return;
    }

    /* 2) build packet */
    packet.seqnum  = seq;
    packet.acknum  = NOTINUSE;
    for (i = 0; i < 20; i++)
        packet.payload[i] = message.data[i];
    packet.checksum = ComputeChecksum(packet);

    /* 3) buffer it */
    idx = seq % SR_WINDOW_SIZE;
    A_window[idx]        = packet;
    A_acknowledged[idx]  = 0;

    /* 4) send */
    tolayer3(A, packet);
    if (TRACE > 0)
        printf("----A: Sent packet %d\n", seq);

    /* 5) start timer if this is the base packet */
    if (window_len == 0)
        starttimer(A, TIMEOUT_INTERVAL);

    /* 6) advance next sequence number */
    A_nextseqnum = (seq + 1) % MAX_SEQ_NUM; 
}

void A_input(struct pkt packet)      
{
  int acknum, idx;

    /* 1) Drop corrupted ACKs */
    if (IsCorrupted(packet)) {
        if (TRACE > 0)
            printf("----A: Corrupted ACK received, ignored\n");
        return;
    }

    acknum = packet.acknum;
    idx    = acknum % SR_WINDOW_SIZE;

    /* 2) If this ACK is new, mark it */
    if (A_acknowledged[idx] == 0) {
        A_acknowledged[idx] = 1;
        new_ACKs++;
        total_ACKs_received++;
        if (TRACE > 0)
            printf("----A: ACK %d received and marked\n", acknum);
    } else {
        if (TRACE > 0)
            printf("----A: Duplicate ACK %d, ignored\n", acknum);
        return;
    }

    /* 3) Slide window base forward while in-order ACKs exist */
    while (A_acknowledged[A_base % SR_WINDOW_SIZE]) {
        /* clear the slot and advance base */
        A_acknowledged[A_base % SR_WINDOW_SIZE] = 0;
        A_base = (A_base + 1) % MAX_SEQ_NUM;
        if (TRACE > 0)
            printf("----A: Sliding base to %d\n", A_base);
    }

    /* 4) If there are still outstanding packets, restart timer for new base */
    if ((A_nextseqnum + MAX_SEQ_NUM - A_base) % MAX_SEQ_NUM > 0) {
        stoptimer(A);
        starttimer(A, TIMEOUT_INTERVAL);
    } else {
        stoptimer(A);
    }
}

void A_timerinterrupt(void)          
{
  int idx = A_base % SR_WINDOW_SIZE;

    /* Log and retransmit the base packet */
    if (TRACE > 0)
        printf("----A: Timer expired, resending packet %d\n", A_base);
    tolayer3(A, A_window[idx]);
    packets_resent++;

    /* Restart timer for this packet */
    starttimer(A, TIMEOUT_INTERVAL);
}

/* Receiver (B) stubs */
void B_init(void)                    
{
 int i;
    B_expected_base = 0;
    for (i = 0; i < SR_WINDOW_SIZE; i++) {
        B_received[i] = 0;
        memset(&B_window[i], 0, sizeof(struct pkt));
    }
    if (TRACE > 0)
        printf("----B: SR Receiver initialized (expected_base=0)\n");  
}

/* Receiver (B) input from layer 3 (data packets) */
void B_input(struct pkt packet) 
{
  int seq = packet.seqnum;
    int idx = seq % SR_WINDOW_SIZE;

    /* 1) Ignore corrupted packets */
    if (IsCorrupted(packet)) {
        if (TRACE > 0)
            printf("----B: Corrupted packet %d, ignored\n", seq);
        return;
    }

    /* 2) Check if seq is within the receive window */
    if ((seq >= B_expected_base && seq < B_expected_base + SR_WINDOW_SIZE) ||
        (B_expected_base + SR_WINDOW_SIZE >= MAX_SEQ_NUM &&
         seq < (B_expected_base + SR_WINDOW_SIZE) % MAX_SEQ_NUM)) {

        /* 3) Buffer if new */
        if (!B_received[idx]) {
            B_window[idx] = packet;
            B_received[idx] = 1;
            packets_received++;
            if (TRACE > 0)
                printf("----B: Buffered packet %d\n", seq);
        }

        /* 4) Send ACK for this packet */
        {
            struct pkt ackpkt;
            int j;
            ackpkt.seqnum = 0;           /* not used */
            ackpkt.acknum = seq;
            for (j = 0; j < 20; j++)
                ackpkt.payload[j] = '0';
            ackpkt.checksum = ComputeChecksum(ackpkt);
            tolayer3(B, ackpkt);
            if (TRACE > 0)
                printf("----B: ACK %d sent\n", seq);
        }

        /* 5) Deliver all in-order packets starting at base */
        while (B_received[B_expected_base % SR_WINDOW_SIZE]) {
            int deliver_idx = B_expected_base % SR_WINDOW_SIZE;
            tolayer5(B, B_window[deliver_idx].payload);
            B_received[deliver_idx] = 0;
            if (TRACE > 0)
                printf("----B: Delivered packet %d to layer5\n", B_expected_base);
            B_expected_base = (B_expected_base + 1) % MAX_SEQ_NUM;
        }

    } else {
        /* Out of window: still send ACK */
        struct pkt ackpkt;
        int j;
        ackpkt.seqnum = 0;
        ackpkt.acknum = seq;
        for (j = 0; j < 20; j++)
            ackpkt.payload[j] = '0';
        ackpkt.checksum = ComputeChecksum(ackpkt);
        tolayer3(B, ackpkt);
        if (TRACE > 0)
            printf("----B: Packet %d outside window, ACK resent\n", seq);
    } 
}

/* Receiver (B) output (unused for simplex) */
void B_output(struct msg message) { }

/* Receiver (B) timer interrupt (unused for simplex) */
void B_timerinterrupt(void) { }