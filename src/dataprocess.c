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

complex_fract16 fftOut[FFT_SIZE];
float distMat[SPECT_H][SPECT_H];

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

void genSpectro(const fract16* data, u32_t dlen, float spectro[][SPECT_W], u32_t* slen, const complex_fract16* twidtab)
{
	//Only enough data for a single frame of fft, discard
	if(dlen < FFT_SIZE + FFT_SIZE / 2)
	{
		*slen = 0;
		return;
	}
	
	u32_t frameOffset = FFT_SIZE / 2;	//FFT frame offset
	u32_t frameNum = dlen / frameOffset - 1;	//total FFT frame number
	complex_fract16 origin = ccompose_fr16(0, 0);
	int expo = 0;
	u32_t spectRow, spectCol;	//spectrogram row and column index
	for(spectRow = 0; frameNum > 0; --frameNum, ++spectRow)
	{
		rfft_fr16(data += frameOffset, fftOut, twidtab, 1, FFT_SIZE, &expo, 
	#ifdef FFT_STATIC_EXPO
		1
	#else
		2
	#endif
		);	//dynamic scaling, since input signal value is relatively small
		//copy into spectrogram, DC component ignored
		for(spectCol = 0; spectCol < SPECT_W; ++spectCol)
	#ifdef FFT_STATIC_EXPO
			spectro[spectRow][spectCol] = fr16_to_float(cdst_fr16(fftOut[spectCol + 1], origin));
	#else
			spectro[spectRow][spectCol] = fr16_to_float(cdst_fr16(fftOut[spectCol + 1], origin)) * (1 << expo);
	#endif
	}
	*slen = spectRow;
	
#ifdef NORM_LOG
	//normalize spectrogram
	normSpectro(spectro, *slen);
#endif
			
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

float logValMax = FLT_MIN;
float logValMin = FLT_MAX;

void normSpectro(float spectro[][SPECT_W], u32_t slen)
{
	float logVal;
	
	u32_t row, col;
	for(row = 0; row < slen; ++row)
		for(col = 0; col < SPECT_W; ++col)
		{
			logVal = spectro[row][col];
			logValMax = logVal > logValMax ? logVal : logValMax;
			logValMin = logVal < logValMin ? logVal : logValMin;
		}
	
	float scale = (NORM_MAX - NORM_MIN) / (logValMax - logValMin);
	float dist = NORM_MAX - scale * logValMax;
	
	for(row = 0; row < slen; ++row)
		for(col = 0; col < SPECT_W; ++col)
			spectro[row][col] = log10(scale * spectro[row][col] + dist);
}


float dtwResult;
float genSimilarity(const spectro_t* spectro1, const spectro_t* spectro2)
{
	u32_t row, col;
	u32_t height = spectro1->effHeight;
	u32_t width  = spectro2->effHeight;
	
	//Alloc distance matrix
	
	/*distMat = (float**)malloc(height * sizeof(float*));
	for(row = 0; row < height; ++row)
		distMat[row] = (float*)malloc(width * sizeof(float));*/
	
	genDistMat(spectro1, spectro2, distMat);
	dtw(distMat, width, height);
	
	
	dtwResult = distMat[height - 1][width - 1];
	//Free dist matrix
	/*for(row = 0; row < height; ++row)
		free(distMat[row]);
	free(distMat);*/
	
	return dtwResult;
}

void genDistMat(const spectro_t* spectro1, const spectro_t* spectro2, float distmat[][SPECT_H])
{
	u32_t spect1Row, spect2Row, col;
	u32_t height = spectro1->effHeight;
	u32_t width  = spectro2->effHeight;
	
	for(spect1Row = 0; spect1Row < height; ++spect1Row)
		for(spect2Row = 0; spect2Row < width; ++spect2Row)
		{
			distmat[spect1Row][spect2Row] = 0;
			for(col = 0; col < SPECT_W; ++col)
		#ifdef ABS_DISTANCE
				distmat[spect1Row][spect2Row] += fabs(spectro1->data[spect1Row][col] - spectro2->data[spect2Row][col]);
		#else
				distmat[spect1Row][spect2Row] += pow(spectro1->data[spect1Row][col] - spectro2->data[spect2Row][col], 2);
		#endif
		}
	
}


//Obtain similarity at the last row last column
void dtw(float mat[][SPECT_H], u32_t width, u32_t height)
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
