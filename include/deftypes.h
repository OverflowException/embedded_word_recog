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
	u16_t data[AUDIO_BUF_LEN];
	u32_t effLen;
}audio_t;

typedef struct
{
	float data[CEPST_H][CEPST_W];
	u32_t effHeight;
}cepstra_t;

#define	P_GET_VALID(P)	(((P).info & 0x80) & (((P).info & 0x40) << 1))
#define P_GET_TI(P)		((P).info & 0x03)
#define P_GET_BI(P)		(((P).info & 0x04) >> 2)

#define P_SET_CL(P)		((P).info |= 0x80)
#define P_CLEAR_CL(P)	((P).info &= ~(0x80))
#define P_SET_IC(P)		((P).info |= 0x40)
#define	P_CLEAR_IC(P)	((P).info &= ~(0x40))
#define P_SET_BI(P, B)	((P).info |= ((B << 2) & 0x04))
#define P_SET_TI(P,	T)	((P).info |= (T & 0x03))

typedef struct
{
	cepstra_t content;
	/*
		Providing page replacement info
		bit 7 --- CL	clean bit
		bit 6 --- IC	in-cache bit
		bit 2 --- BI	buffer index
		bit 1-0 --- TI	template index
	*/
	u8_t info;
	
}page_t;

#endif
