#ifndef _DATAPROCESS_H
#define _DATAPROCESS_H

#include "global.h"
#include "deftypes.h"
#include <fract.h>

void findEffData(const u16_t* rawdata, 
				 u16_t* effdata, 
				 u32_t* efflen);
				 
void genSpectro(const fract16* data, 
				u32_t dlen, 
				float spectro[][SPECT_W], 
				u32_t* slen, 
				const complex_fract16* twidtab);
				
float genSimilarity(const spectro_t* spectro1, const spectro_t* spectro2);



u16_t getAvgAmp(const u16_t* data, u32_t count, u32_t stride);
u32_t getAvgEnergy(const u16_t* data, u32_t count, u32_t stride, u16_t avg);
void normSpectro(float spectro[][SPECT_W], u32_t slen);

void genDistMat(const spectro_t* spectro1, const spectro_t* spectro2, float distmat[][SPECT_H]);
void dtw(float mat[][SPECT_H], u32_t width, u32_t height);


#endif
