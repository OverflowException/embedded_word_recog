#ifndef _DMACODEC_H
#define _DMACODEC_H

#include <sys\exception.h>
#include "deftypes.h"

void Init1836(void);

void InitSport0In(void);
void InitSport0Out(void);

void InitDMACodecIn(u16_t* inmem);
void InitDMACodecOut(u16_t* outmem, u32_t snum);

void restartAudioIn(u16_t* inmem);
void restartAudioOut(u16_t* outmem, u32_t snum);

void InitDMACodecInInterrupts(void);

void EnableDMACodecIn(void);
void EnableDMACodecOut(void);

EX_INTERRUPT_HANDLER(Sport0RXISR);

#endif
