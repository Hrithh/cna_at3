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

#endif  /* SR_H */