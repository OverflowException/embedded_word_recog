#ifndef _DMACODEC_H
#define _DMACODEC_H

#include <sys\exception.h>
#include "deftypes.h"

void init1836(void);

void initSport0In(void);
void initSport0Out(void);

void initDMACodecIn(u16_t* inmem);
void initDMACodecOut(u16_t* outmem, u32_t snum);
void startMemDMA(void* src, void* dest, u16_t size);

void restartAudioIn(u16_t* inmem);
void restartAudioOut(u16_t* outmem, u32_t snum);

void initDMACodecInInterrupts(void);

void enableDMACodecIn(void);
void enableDMACodecOut(void);
inline bool memDMADone(void){ return (*pMDMA1_D0_IRQ_STATUS) & DMA_DONE; }

EX_INTERRUPT_HANDLER(Sport0RXISR);

#endif
