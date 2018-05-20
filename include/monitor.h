#ifndef _MONITOR_H
#define _MONITOR_H

#include <drivers/adi_dev.h>			// Device manager includes
#include <drivers/decoder/adi_adv7183.h>		// AD7183 device driver includes
#include <drivers/encoder/adi_adv717x.h>		// ADV7179 device driver includes
#include <drivers/twi/adi_twi.h>			// TWI device driver includes
#include <adi_ssl_Init.h>
#include <adi_itu656.h>
#include <SDK-ezkitutilities.h>
#include "deftypes.h"

#define NTSC	0
#define PAL 	1

#define FRAME_DATA_LEN	864			// Number of pixels(entire field) per line for PAL video format
#define NUM_LINES		625			// Number of lines per frame    

#define FRAMESIZE	FRAME_DATA_LEN * NUM_LINES

#define	RED		0
#define	GREEN	1
#define	BLUE	2
#define	YELLOW	3
#define	MAGNETA	4
#define CYAN	5
#define WHITE	6
#define BLACK	7

#define SECT_W	160
#define SECT_H	250
#define GAP_H	5
#define	GAP_V	10

#define SECT1_X	15
#define SECT1_Y 35
#define SECT2_X (SECT1_X + SECT_W + GAP_H)
#define SECT2_Y (SECT1_Y)
#define SECT3_X (SECT1_X)
#define SECT3_Y (SECT1_Y + SECT_H + GAP_V)
#define SECT4_X (SECT2_X)
#define SECT4_Y	(SECT3_Y)

void startADV7179(void);
void formatDisplayBuffers(void);
void setMonitorBg(u16_t color);
void CallbackFunction(void *AppHandle, u32 Event, void *pArg);

//void adi_video_RegionSet(char* frame_ptr, unsigned long x, unsigned long y, unsigned long width, unsigned long height, char *ycbcr_data)
//row 40-540
//col 20-340
void applyMask(char* frame_ptr, const u8_t mask[][2], u32_t msize, u32_t x, u32_t y, char* ycbcr_data);
void flushStateMonitor(volatile state_t* state);
void setRec(void);
void clearRec(void);
void setProc(void);
void clearProc(void);
void showResult(u16_t idx);

extern volatile u16_t sFrame0[FRAMESIZE];

extern char ycc_colors[8][4];

#endif
