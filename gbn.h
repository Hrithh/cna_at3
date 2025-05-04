#ifndef GBN_H
#define GBN_H

/* emulator.c depends on this to know the loss/corrupt direction */
#define BIDIRECTIONAL 0    /* 0 = A->B only */

/* Selective-Repeat implements exactly the same transport API */
#include "sr.h"

#endif /* GBN_H */
