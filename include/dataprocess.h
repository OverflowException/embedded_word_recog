#ifndef _DATAPROCESS_H
#define _DATAPROCESS_H

#include "global.h"
#include "deftypes.h"
#include <fract.h>

void findEffData(const u16_t* rawdata, 
				 u16_t* effdata, 
				 u32_t* efflen);
				 
void genCepstra(const fract16* data, 
				u32_t dlen, 
				float cepstra[][CEPST_W], 
				u32_t* slen, 
				const complex_fract16* ffttwid,
				const complex_fract16* dcttwid,
				const float* loglut);
				
float genSimilarity(const cepstra_t* cepstra1, const cepstra_t* cepstra2);



u16_t getAvgAmp(const u16_t* data, u32_t count, u32_t stride);
u32_t getAvgEnergy(const u16_t* data, u32_t count, u32_t stride, u16_t avg);

void genLogLut(float* lut);
void genDistMat(const cepstra_t* cepstra1, const cepstra_t* cepstra2, float distmat[][CEPST_H]);
void dtw(float mat[][CEPST_H], u32_t width, u32_t height);


#endif
