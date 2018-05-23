#ifndef _GLOBAL_H
#define _GLOBAL_H

#include "deftypes.h"
#include "dbgmacro.h"
#include <filter.h>

//--------------------------------------------------------------------------//
// single vs dual core operation											//
//--------------------------------------------------------------------------//
#define RUN_ON_SINGLE_CORE		// comment out if dual core operation is desired

//--------------------------------------------------------------------------//
// Symbolic constants														//
//--------------------------------------------------------------------------//
// AD1836 reset PF15
#define AD1836_RESET_bit 15

// names for codec registers, used for iCodec1836TxRegs[]
#define DAC_CONTROL_1		0x0000
#define DAC_CONTROL_2		0x1000
#define DAC_VOLUME_0		0x2000
#define DAC_VOLUME_1		0x3000
#define DAC_VOLUME_2		0x4000
#define DAC_VOLUME_3		0x5000
#define DAC_VOLUME_4		0x6000
#define DAC_VOLUME_5		0x7000
#define ADC_0_PEAK_LEVEL	0x8000
#define ADC_1_PEAK_LEVEL	0x9000
#define ADC_2_PEAK_LEVEL	0xA000
#define ADC_3_PEAK_LEVEL	0xB000
#define ADC_CONTROL_1		0xC000
#define ADC_CONTROL_2		0xD000
#define ADC_CONTROL_3		0xE000

// names for slots in ad1836 audio frame
//AD1836 Datasheet pg 10-12
#define INTERNAL_ADC_L0			0
#define INTERNAL_ADC_L1			1
#define INTERNAL_ADC_R0			2
#define INTERNAL_ADC_R1			3

#define INTERNAL_DAC_L0			0
#define INTERNAL_DAC_L1			1
#define INTERNAL_DAC_R0			2
#define INTERNAL_DAC_R1			3

// size of array iCodec1836TxRegs and iCodec1836RxRegs
#define CODEC_1836_REGS_LENGTH	11

// SPI transfer mode
#define TIMOD_DMA_TX 0x0003

// SPORT0 word length
#define SLEN_32	0x001f
#define SLEN_16					0x000F

#define AD1836_RESET_bit 15

extern u16_t audioData[];
extern u32_t audioSampleNum;
extern u32_t audioBufLen;

/*extern float spectroTemplates[][SPECT_H][SPECT_W];
extern float spectroBuff[][SPECT_W];
extern complex_fract16 twids[];*/

extern audio_t inAudio;
extern cepstra_t cepstraTemplates[CEPST_TEMPLATE_NUM];
extern cepstra_t cepstraBuff;


//extern volatile bool dmaDone;
extern volatile bool startRec;
extern volatile fsm_t btnFSM;
extern volatile action_t recAction;

#endif
