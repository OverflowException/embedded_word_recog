#ifndef _DBGMACRO_H
#define _DBGMACRO_H


#define AUDIO_SAMPLE_RATE 48000	//codec sample rate
#define AUDIO_SAMPLE_LEN 4		//codes sample size, each sample consists of 4 unsigned short
#define AUDIO_DURATION_S	1	//audio duration, in seconds

#define AUDIO_SAMPLE_NUM	(AUDIO_SAMPLE_RATE * AUDIO_DURATION_S)
#define AUDIO_BUF_LEN  (AUDIO_SAMPLE_NUM * AUDIO_SAMPLE_LEN)

#define HEAD_SIL_DURATION_MS	100	//leading silent duration, in miliseconds
#define ENERGY_WND_SAMPLE_NUM	512		//sample number in energy window
#define EFF_THRESH_TIMES	5


#define FFT_SIZE	1024
#define SPECT_W	100
#define CEPST_W	30
#define CEPST_H	(AUDIO_SAMPLE_NUM / (FFT_SIZE / 2) - 1)	//= 92 when AUDIO_SAMPLE_NUM = 48000, FFT_SIZE = 1024
#define CEPST_TEMPLATE_NUM	4


//Calculate energy with absolute value, power = abs(amplitude - amplitude_average), faster but less sensitive
//if not defined calculate energy directly by definition, power = (amplitude - amplitude_average)^2,slower but more sensitive
#define ABS_ENERGY


//Calculate Euclidian distance between vectors with abs, instead of power
#define ABS_DISTANCE

//static exponent for FFT, which is suitable for high amplitute
#define FFT_STATIC_EXPO


#define FRACT16_NUM	65536

#endif
