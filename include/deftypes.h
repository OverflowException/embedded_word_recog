#ifndef _DEFTYPES_H
#define _DEFTYPES_H

#include "dbgmacro.h"

typedef unsigned char u8_t;
typedef unsigned short u16_t;
typedef unsigned int u32_t;

typedef signed char s8_t;
typedef signed short s16_t;
typedef signed int s32_t;

typedef enum
{
	wait,
	start,
	end,
	skip
}action_t;

typedef enum
{
	test = 0,
	train = 8,
	ready
}mode_t;

typedef enum
{
	one = 1,
	two = 2,
	three = 3,
	four = 4
}word_t;


typedef struct _state
{
	mode_t mode;
	word_t word;
	volatile struct _state *next[4]; //4 buttons
}state_t;

typedef struct _fsm
{
	volatile state_t stReady;
	volatile state_t stTrain[4]; //4 numbers to train
	volatile state_t stTest;
	
	volatile state_t* prevState;
	volatile state_t* currState;
}fsm_t;

typedef struct
{
	float data[SPECT_H][SPECT_W];
	u32_t effHeight;
}spectro_t;

typedef struct
{
	u16_t data[AUDIO_BUF_LEN];
	u32_t effLen;
}audio_t;

#endif
