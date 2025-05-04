#ifndef SR_H
#define SR_H

/* Sender (A) API */
extern void A_init(void);
extern void A_output(struct msg message);
extern void A_input(struct pkt packet);
extern void A_timerinterrupt(void);

/* Receiver (B) API */
extern void B_init(void);
extern void B_input(struct pkt packet);
extern void B_output(struct msg message);
extern void B_timerinterrupt(void);

/* --- Selective Repeat Parameters & State --- */
#define SR_WINDOW_SIZE 6        /* size of sender & receiver window */
#define MAX_SEQ_NUM    32       /* sequence number space (0-31) */

#define NOTINUSE       (-1)     /* ‘acknum’ field value when unused */
#define TIMEOUT_INTERVAL 15.0    /* timeout interval for each SR packet */

/* Sender‐side state */
extern struct pkt A_window[SR_WINDOW_SIZE];   /* buffer of sent but unacked packets */
extern int       A_acknowledged[SR_WINDOW_SIZE]; /* ack flags for each slot */
extern int       A_base;                        /* lowest unacked seq# */
extern int       A_nextseqnum;                  /* next seq# to use */

/* Receiver‐side state */
extern struct pkt B_window[SR_WINDOW_SIZE];   /* buffer of out‐of‐order packets */
extern int       B_received[SR_WINDOW_SIZE];  /* flags: packet received in slot */
extern int       B_expected_base;             /* lowest seq# not yet delivered */

#endif  /* SR_H */