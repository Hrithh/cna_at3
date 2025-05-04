#include "emulator.h"
#include "sr.h"

/* Sender (A) stubs */
void A_init(void)                    { }
void A_output(struct msg message)    { }
void A_input(struct pkt packet)      { }
void A_timerinterrupt(void)          { }

/* Receiver (B) stubs */
void B_init(void)                    { }
void B_input(struct pkt packet)      { }
void B_output(struct msg message)    { }
void B_timerinterrupt(void)          { }
