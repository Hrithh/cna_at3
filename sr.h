extern void A_init(void);
extern void B_init(void);
extern void A_input(struct pkt);
extern void B_input(struct pkt);
extern void A_output(struct msg);
extern void A_timerinterrupt(void);

/* included for extension to bidirectional communication */
#define BIDIRECTIONAL 0       /*  0 = A->B  1 =  A<->B */
extern void B_output(struct msg);
extern void B_timerinterrupt(void);

/* --- Selective Repeat Definitions --- */

#define SR_WINDOW_SIZE 8         // SR window size
#define MAX_SEQ_NUM 32           // Sequence number space (0–31 for 5-bit)

#define TIMEOUT_INTERVAL 15.0    // Timeout for each packet

// Sender-side variables
struct pkt A_window[SR_WINDOW_SIZE];       // Packets currently in window
int A_acknowledged[SR_WINDOW_SIZE];        // ACK status (0 = no, 1 = yes)
int A_base;                                // Base of sender window
int A_nextseqnum;                          // Next sequence number to send

// Receiver-side variables
struct pkt B_window[SR_WINDOW_SIZE];       // Received packet buffer
int B_received[SR_WINDOW_SIZE];            // Received flags (0 = no, 1 = yes)
int B_expected_base;                       // Base of receiver window