#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "sr.h"

/* ******************************************************************
   Go Back N protocol.  Adapted from J.F.Kurose
   ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.2

   Network properties:
   - one way network delay averages five time units (longer if there
   are other messages in the channel for GBN), but can be larger
   - packets can be corrupted (either the header or the data portion)
   or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
   (although some can be lost).

   Modifications:
   - removed bidirectional GBN code and other code not used by prac.
   - fixed C style to adhere to current programming style
   - added GBN implementation
**********************************************************************/

#define RTT  16.0       /* round trip time.  MUST BE SET TO 16.0 when submitting assignment */
#define WINDOWSIZE 6    /* the maximum number of buffered unacked packet
                          MUST BE SET TO 6 when submitting assignment */
#define SEQSPACE 7      /* the min sequence space for GBN must be at least windowsize + 1 */
#define NOTINUSE (-1)   /* used to fill header fields that are not being used */

/* generic procedure to compute the checksum of a packet.  Used by both sender and receiver
   the simulator will overwrite part of your packet with 'z's.  It will not overwrite your
   original checksum.  This procedure must generate a different checksum to the original if
   the packet is corrupted.
*/
int ComputeChecksum(struct pkt packet)
{
  int checksum = 0;
  int i;

  checksum = packet.seqnum;
  checksum += packet.acknum;
  for ( i=0; i<20; i++ )
    checksum += (int)(packet.payload[i]);

  return checksum;
}

bool IsCorrupted(struct pkt packet)
{
  if (packet.checksum == ComputeChecksum(packet))
    return (false);
  else
    return (true);
}


/********* Sender (A) variables and functions ************/

//static struct pkt buffer[WINDOWSIZE];  /* array for storing packets waiting for ACK */
//static int windowfirst, windowlast;    /* array indexes of the first/last packet awaiting ACK */
//static int windowcount;                /* the number of packets currently awaiting an ACK */
//static int A_nextseqnum;               /* the next sequence number to be used by the sender */

// Defined in sr.h
extern struct pkt A_window[SR_WINDOW_SIZE];
extern int A_acknowledged[SR_WINDOW_SIZE];
extern int A_base;
extern int A_nextseqnum;

/* called from layer 5 (application layer), passed the message to be sent to other side */
void A_output(struct msg message)
{
  if ((A_nextseqnum + MAX_SEQ_NUM - A_base) % MAX_SEQ_NUM >= SR_WINDOW_SIZE) {
    if (TRACE > 0)
      printf("----A: Send window full. Message dropped or buffered elsewhere.\n");
    window_full++;
    return;
  }

  struct pkt sendpkt;
  sendpkt.seqnum = A_nextseqnum;
  sendpkt.acknum = NOTINUSE;

  for (int i = 0; i < 20; i++)
    sendpkt.payload[i] = message.data[i];

  sendpkt.checksum = ComputeChecksum(sendpkt);

  // Store the packet in the window buffer
  int index = A_nextseqnum % SR_WINDOW_SIZE;
  A_window[index] = sendpkt;
  A_acknowledged[index] = 0;

  // Send the packet
  tolayer3(A, sendpkt);
  if (TRACE > 0)
    printf("----A: Sent packet %d\n", sendpkt.seqnum);
  //packets_sent++;

  // Start timer for this packet
  starttimer(A, TIMEOUT_INTERVAL);

  // Advance sequence number
  A_nextseqnum = (A_nextseqnum + 1) % MAX_SEQ_NUM;
}


/* called from layer 3, when a packet arrives for layer 4
   In this practical this will always be an ACK as B never sends data.
*/
void A_input(struct pkt packet)
{
  if (IsCorrupted(packet)) {
    if (TRACE > 0)
      printf("----A: Corrupted ACK received. Ignored.\n");
    return;
  }

  int acknum = packet.acknum;
  int index = acknum % SR_WINDOW_SIZE;

  if (A_acknowledged[index] == 0) {
    A_acknowledged[index] = 1;
    if (TRACE > 0)
      printf("----A: ACK %d marked as received\n", acknum);
    new_ACKs++;

    // Stop timer (optional: depending on your timer design)
    stoptimer(A);

    // Slide window if base is acknowledged
    while (A_acknowledged[A_base % SR_WINDOW_SIZE] == 1) {
      A_acknowledged[A_base % SR_WINDOW_SIZE] = 0;
      A_base = (A_base + 1) % MAX_SEQ_NUM;
      if (TRACE > 0)
        printf("----A: Sliding window base to %d\n", A_base);
    }

    total_ACKs_received++;
  } else {
    if (TRACE > 0)
      printf("----A: Duplicate ACK %d received. Ignored.\n", acknum);
  }
}

/* called when A's timer goes off */
void A_timerinterrupt(void)
{
  if (TRACE > 0)
    printf("----A: Timer expired. Resending base packet %d\n", A_base);

  int index = A_base % SR_WINDOW_SIZE;
  tolayer3(A, A_window[index]);
  packets_resent++;

  // Restart timer for this packet
  starttimer(A, TIMEOUT_INTERVAL);
}



void A_init(void)
{
  A_base = 0;
  A_nextseqnum = 0;

  for (int i = 0; i < SR_WINDOW_SIZE; i++) {
    A_acknowledged[i] = 0;
    memset(&A_window[i], 0, sizeof(struct pkt));
  }

  if (TRACE > 0)
    printf("----A: Selective Repeat Sender initialized\n");
}



/********* Receiver (B)  variables and procedures ************/

//static int expectedseqnum; /* the sequence number expected next by the receiver */
//static int B_nextseqnum;   /* the sequence number for the next packets sent by B */


/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(struct pkt packet)
{
  struct pkt ackpkt;
  int seq = packet.seqnum;
  int index = seq % SR_WINDOW_SIZE;

  if (IsCorrupted(packet)) {
    if (TRACE > 0)
      printf("----B: Corrupted packet %d. Ignored.\n", seq);
    return;
  }

  // Check if seq is within receiver window
  if ((seq >= B_expected_base && seq < B_expected_base + SR_WINDOW_SIZE) ||
      (B_expected_base + SR_WINDOW_SIZE >= MAX_SEQ_NUM &&
       (seq < (B_expected_base + SR_WINDOW_SIZE) % MAX_SEQ_NUM))) {

    // Buffer if not already received
    if (!B_received[index]) {
      B_window[index] = packet;
      B_received[index] = 1;

      if (TRACE > 0)
        printf("----B: Buffered packet %d\n", seq);
    }

    // Send ACK
    ackpkt.seqnum = 0;  // Not used
    ackpkt.acknum = seq;
    for (int i = 0; i < 20; i++)
      ackpkt.payload[i] = '0';
    ackpkt.checksum = ComputeChecksum(ackpkt);
    tolayer3(B, ackpkt);

    if (TRACE > 0)
      printf("----B: ACK %d sent\n", seq);

    // Deliver any in-order packets starting from base
    while (B_received[B_expected_base % SR_WINDOW_SIZE]) {
      tolayer5(B, B_window[B_expected_base % SR_WINDOW_SIZE].payload);
      B_received[B_expected_base % SR_WINDOW_SIZE] = 0;
      B_expected_base = (B_expected_base + 1) % MAX_SEQ_NUM;
    }

  } else {
    // Packet is out of window but send ACK anyway
    ackpkt.seqnum = 0;
    ackpkt.acknum = seq;
    for (int i = 0; i < 20; i++)
      ackpkt.payload[i] = '0';
    ackpkt.checksum = ComputeChecksum(ackpkt);
    tolayer3(B, ackpkt);

    if (TRACE > 0)
      printf("----B: Packet %d outside window. Sent ACK anyway.\n", seq);
  }
}

/* the following routine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init(void)
{
  B_expected_base = 0;

  for (int i = 0; i < SR_WINDOW_SIZE; i++) {
    B_received[i] = 0;
    memset(&B_window[i], 0, sizeof(struct pkt));
  }

  if (TRACE > 0)
    printf("----B: Selective Repeat Receiver initialized\n");
}

/******************************************************************************
 * The following functions need be completed only for bi-directional messages *
 *****************************************************************************/

/* Note that with simplex transfer from a-to-B, there is no B_output() */
void B_output(struct msg message)
{
}

/* called when B's timer goes off */
void B_timerinterrupt(void)
{
}
