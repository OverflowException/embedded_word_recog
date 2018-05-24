#include "global.h"
#include "dataprocess.h"
#include "dbgmacro.h"
#include <stdlib.h>
#include <filter.h>
#include <stdfix.h>
#include <float.h>
#include <math.h>

u32_t audioSampleNum = AUDIO_SAMPLE_NUM; //Total number of samples
u32_t audioBufLen = AUDIO_BUF_LEN; //Total length of audio buffer (including 3 irrelavent u16_t per sample)

//Avoid data bank collision with 
section("L1_data_b") complex_fract16 fftOut[FFT_SIZE];

float distMat[CEPST_H][CEPST_H];

#define OVERHEAD_SAMPLE_NUM	64
#define TIMING_SAMPLE_NUM	128

void findEffData(const u16_t* rawdata, u16_t* effdata, u32_t* efflen)
{
	//Find the correct timing of raw data
	const u16_t* timingBeg = rawdata + OVERHEAD_SAMPLE_NUM * AUDIO_SAMPLE_LEN;
	rawdata += getAvgEnergy(timingBeg, TIMING_SAMPLE_NUM, AUDIO_SAMPLE_LEN, getAvgAmp(timingBeg, TIMING_SAMPLE_NUM, AUDIO_SAMPLE_LEN)) >  
			   getAvgEnergy(timingBeg + 2, TIMING_SAMPLE_NUM, AUDIO_SAMPLE_LEN, getAvgAmp(timingBeg + 2, TIMING_SAMPLE_NUM, AUDIO_SAMPLE_LEN)) ? 0 : 2;
	
	
	u32_t silSampleNum = AUDIO_SAMPLE_RATE / 1000 * HEAD_SIL_DURATION_MS;	//Total number of silent samples
	u32_t nonSilEnergyWndNum = (audioSampleNum - silSampleNum) / ENERGY_WND_SAMPLE_NUM; //Total number of energy windows in non-silent section
	u32_t energyWndLen = ENERGY_WND_SAMPLE_NUM * AUDIO_SAMPLE_LEN;	//Total length of energy window (including 3 irrelavent u16_t per sample)
	
	//Calculate background noise average amplitute
	u16_t bgAvgAmp = getAvgAmp(rawdata, silSampleNum, AUDIO_SAMPLE_LEN);
	//Calculate background noise average energy, then calculate effective energy threshold
	u32_t bgAvgEnergy = getAvgEnergy(rawdata, silSampleNum, AUDIO_SAMPLE_LEN, bgAvgAmp);
	u32_t energyThresh = bgAvgEnergy * EFF_THRESH_TIMES;	
	
	//Locate beginning of effective data by energy, window by window
	u32_t wndPos, wndCount, effBegIdx, effEndIdx;
	for(wndPos = silSampleNum * AUDIO_SAMPLE_LEN, wndCount = nonSilEnergyWndNum; 
		wndCount > 0; 
		--wndCount, wndPos += energyWndLen)
		if(getAvgEnergy(rawdata + wndPos, ENERGY_WND_SAMPLE_NUM, AUDIO_SAMPLE_LEN, bgAvgAmp) > energyThresh)
		{
			effBegIdx = wndPos;
			break;
		}
	
	//If reached the end, but found no effective beginning
	if(wndCount == 0)
	{
		*efflen = 0;
		return;
	}	
	
	//Locate ending of effective data by energy, window by window
	for(wndPos = audioBufLen - energyWndLen, wndCount = nonSilEnergyWndNum;
		wndCount > 0;
		--wndCount, wndPos -= energyWndLen)
		if(getAvgEnergy(rawdata + wndPos, ENERGY_WND_SAMPLE_NUM, AUDIO_SAMPLE_LEN, bgAvgAmp) > energyThresh)
		{
			effEndIdx = wndPos + energyWndLen;
			break;
		}
		
	//If reached effective beginning, but found no effective end
	if(effEndIdx <= effBegIdx)
	{
		*efflen = 0;
		return;
	}
	
	
	//Copy effective data to effdata and find max value
	//*efflen = (effEndIdx - effBegIdx - 1) / 4 + 1;
	//Potential of using DMA
	u32_t rawIdx, effIdx = 0;
	u16_t max = 0x0000;
	u16_t min = 0xFFFF;
	for(rawIdx = effBegIdx; rawIdx < effEndIdx; rawIdx += 4)
	{
		//copy and shift, to prevent negative value when casting to fract16
		effdata[effIdx] = rawdata[rawIdx] >> 1;
		++effIdx;
	}
	*efflen = effIdx;
}

void genCepstra(const fract16* data, u32_t dlen, float cepstra[][CEPST_W], u32_t* slen, 
				const complex_fract16* ffttwid, const complex_fract16* dcttwid, const float* loglut)
{
	//Only enough data for a single frame of fft, discard
	if(dlen < FFT_SIZE + FFT_SIZE / 2)
	{
		*slen = 0;
		return;
	}
	
	u32_t frameOffset = FFT_SIZE / 2;	//FFT frame offset
	u32_t frameNum = dlen / frameOffset - 1;	//total FFT frame number
	complex_fract16 origin = ccompose_fr16(0, 0);	//complex origin, 0+0i
	int expo = 0;
	u32_t ceRow, ceCol, ceIdx;	//CCA row and column index
	u16_t fftIdx;	//FFT value index and cepstral coefficient index
	u16_t phase, deltaPhase;	//DCT phase and delta phase
	u16_t dctPeriod = SPECT_W * 4;
	
	//Traverse rows of CCA
	for(ceRow = 0; frameNum > 0; --frameNum, ++ceRow)
	{
		
		//Perform FFT
		rfft_fr16(data += frameOffset, fftOut, ffttwid, 1, FFT_SIZE, &expo, 
	#ifdef FFT_STATIC_EXPO
		1
	#else
		2
	#endif
		);
		
		//Traverse elements of CCA
		for(ceCol = 0; ceCol < CEPST_W; ++ceCol)
		{
			
			//Ignore the first cepstral coefficients
			ceIdx = ceCol + 1;
			phase = ceIdx;
			deltaPhase = ceIdx * 2;
			
			cepstra[ceRow][ceCol] = 0.0f;
			//Traverse FFT values
			for(fftIdx = 0; fftIdx < SPECT_W; ++fftIdx)
			{
				if((phase += deltaPhase) >= dctPeriod)
					phase -= dctPeriod;	
				
	#ifdef FFT_STATIC_EXPO
				//cepstra[ceRow][ceCol] = loglut[(u16_t)cdst_fr16(fftOut[fftIdx], origin)] * fr16_to_float(real_fr16(dcttwid[phase]));
				cepstra[ceRow][ceCol] += loglut[cdst_fr16(fftOut[fftIdx], origin)] * fr16_to_float(real_fr16(dcttwid[phase]));
	#else
				cepstra[ceRow][ceCol] += loglut[(u16_t)cdst_fr16(fftOut[fftIdx] * (1 << expo), orgin)] * real_fr16(dcttwid[phase]);
	#endif
			}
		}
	}
	*slen = ceRow;
			
	return;
}

u16_t getAvgAmp(const u16_t* data, u32_t count, u32_t stride)
{
	u32_t sum = 0;
	u32_t c = count;
	for(; c > 0; data += stride, --c)
		sum += *data;
		
	return sum / count;
}

u32_t energy;
s32_t ampOffset;

u32_t getAvgEnergy(const u16_t* data, u32_t count, u32_t stride, u16_t avg)
{
	energy = 0;
	ampOffset = 0;
	u32_t c = count;
	for(; c > 0; data += stride, --c)
	{	
		//Better calculate energy directly with square
		ampOffset = (s32_t)(*data) - avg;
		
	#ifdef ABS_ENERGY
		energy += (ampOffset >= 0 ? ampOffset : (0 - ampOffset));
	#else		
		energy += ampOffset * ampOffset;
	#endif
	}
	
	return (u32_t)energy / count;
		
}

float dtwResult;
float genSimilarity(const cepstra_t* cepstra1, const cepstra_t* cepstra2)
{
	u32_t row, col;
	u32_t height = cepstra1->effHeight;
	u32_t width  = cepstra2->effHeight;
	
	genDistMat(cepstra1, cepstra2, distMat);
	dtw(distMat, width, height);
	
	
	dtwResult = distMat[height - 1][width - 1];
	
	return dtwResult;
}


void genLogLut(float* lut)
{
	u16_t binFract;
	//fract16 ranging from 2^(-15) = 0.000030517578125 to 1-2^(-15) = 0.999969482421875
	for(binFract = 0x0001; binFract < 0x8000; ++binFract)
		lut[binFract] = log10(fr16_to_float((fract16)binFract));
	
	//Minimum log value
	lut[0] = lut[1] - 0.425;
}


void genDistMat(const cepstra_t* cepstra1, const cepstra_t* cepstra2, float distmat[][CEPST_H])
{
	u32_t cepst1Row, cepst2Row, col;
	u32_t height = cepstra1->effHeight;
	u32_t width  = cepstra2->effHeight;
	
	for(cepst1Row = 0; cepst1Row < height; ++cepst1Row)
		for(cepst2Row = 0; cepst2Row < width; ++cepst2Row)
		{
			distmat[cepst1Row][cepst2Row] = 0;
			for(col = 0; col < CEPST_W; ++col)
		#ifdef ABS_DISTANCE
				distmat[cepst1Row][cepst2Row] += fabs(cepstra1->data[cepst1Row][col] - cepstra2->data[cepst2Row][col]);
		#else
				distmat[cepst1Row][cepst2Row] += pow(cepstra1->data[cepst1Row][col] - cepstra2->data[cepst2Row][col], 2);
		#endif
		}
	
}


//Obtain similarity at the last row last column
void dtw(float mat[][CEPST_H], u32_t width, u32_t height)
{
	if(width <= 1 || height <= 1)
		return;
	
	u32_t row, col;
	float distMin;
	
	//The first row
	for(col = 1; col < width; ++col)
		mat[0][col] += mat[0][col - 1];
		
	//The other rows
	for(row = 1; row < height; ++row)
	{
		//The first element of this row
		mat[row][0] += mat[row - 1][0];
		
		//The other elements of this row
		for(col = 1; col < width; ++col)
		{
			distMin = ((mat[row - 1][col] < mat[row][col - 1]) ? mat[row - 1][col] : mat[row][col - 1]);
			mat[row][col] += ((distMin < mat[row - 1][col - 1]) ? distMin : mat[row - 1][col - 1]);
		}
	}
}
