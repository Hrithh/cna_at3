#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "gbn.h"

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
  packets_sent++;

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
  struct pkt sendpkt;
  int i;

  /* if not corrupted and received packet is in order */
  if  ( (!IsCorrupted(packet))  && (packet.seqnum == expectedseqnum) ) {
    if (TRACE > 0)
      printf("----B: packet %d is correctly received, send ACK!\n",packet.seqnum);
    packets_received++;

    /* deliver to receiving application */
    tolayer5(B, packet.payload);

    /* send an ACK for the received packet */
    sendpkt.acknum = expectedseqnum;

    /* update state variables */
    expectedseqnum = (expectedseqnum + 1) % SEQSPACE;
  }
  else {
    /* packet is corrupted or out of order resend last ACK */
    if (TRACE > 0)
      printf("----B: packet corrupted or not expected sequence number, resend ACK!\n");
    if (expectedseqnum == 0)
      sendpkt.acknum = SEQSPACE - 1;
    else
      sendpkt.acknum = expectedseqnum - 1;
  }

  /* create packet */
  sendpkt.seqnum = B_nextseqnum;
  B_nextseqnum = (B_nextseqnum + 1) % 2;

  /* we don't have any data to send.  fill payload with 0's */
  for ( i=0; i<20 ; i++ )
    sendpkt.payload[i] = '0';

  /* computer checksum */
  sendpkt.checksum = ComputeChecksum(sendpkt);

  /* send out packet */
  tolayer3 (B, sendpkt);
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
