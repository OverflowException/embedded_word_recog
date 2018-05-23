#include <ccblkfn.h>
#include <fract.h>
#include <float.h>
#include "dmacodec.h"
#include "deftypes.h"
#include "global.h"
#include "dataprocess.h"
#include "btnled.h"
#include "monitor.h"

audio_t inAudio;
cepstra_t cepstraTemplates[CEPST_TEMPLATE_NUM];
cepstra_t cepstraBuff;

//Twiddle factors
complex_fract16 ffttwid[FFT_SIZE / 2];
complex_fract16 dcttwid[SPECT_W * 4];
float logLut[FRACT16_NUM];
////////////////

float simiArr[CEPST_TEMPLATE_NUM];
float minSimi = FLT_MAX;
u16_t simiTempIdx = 0;

volatile fsm_t btnFSM;
volatile action_t recAction;
void InitFSM(void);
void flushStateLED(state_t state);

void flushStateMonitor(volatile state_t* state);
inline void setRec(void);
inline void clearRec(void);
inline void setProc(void);
inline void clearProc(void);
inline void showResult(u16_t idx);

int main(void)
{

	adi_ssl_Init();	//Power, EBIU, DMA etc.
	
	startADV7179();
	formatDisplayBuffers();
	setMonitorBg(BLACK);

 	Init1836();
 	
	InitSport0In();
	InitSport0Out();
	InitDMACodecInInterrupts();
	
	InitButtons();
	//InitLEDs();
	InitButtonsInt();
	
	twidfftrad2_fr16(ffttwid, FFT_SIZE);
	twidfftrad2_fr16(dcttwid, SPECT_W * 4);
	genLogLut(logLut);
	
	InitFSM();
	
	
	u16_t tempIdx = 0;
	while(true)
	{
		if(btnFSM.currState != btnFSM.prevState)	//button pressed, state change occured
		{
			flushStateMonitor(btnFSM.currState);//flushStateLED(*btnFSM.currState);
			btnFSM.prevState = btnFSM.currState;
		}
		
		recAction = wait;
		while(recAction == wait);	//wait for recording instruction, if get an skip, do nothing, continue next cycle
		if(recAction == start)
		{
			setRec();//toggleLED(6 + btnFSM.currState->mode);
			
			restartAudioIn(inAudio.data);
			while(recAction != end);	//wait for DMA to finish
			clearRec();//toggleLED(6 + btnFSM.currState->mode);
			setProc();//toggleLED(5 + btnFSM.currState->mode);
			findEffData(inAudio.data, 
						inAudio.data, 
						&inAudio.effLen);
			switch(btnFSM.currState->mode)
			{
			case train:
				tempIdx = btnFSM.currState->word - 1;
				genCepstra((const fract16*)inAudio.data, 
							inAudio.effLen, 
							cepstraTemplates[tempIdx].data, 
							&cepstraTemplates[tempIdx].effHeight, 
							ffttwid,
							dcttwid,
							logLut);
				break;
			case test:
				genCepstra((const fract16*)inAudio.data, 
							inAudio.effLen, 
							cepstraBuff.data, 
							&cepstraBuff.effHeight, 
							ffttwid,
							dcttwid,
							logLut);
				//recognize
				// 2 seperate cycles, for debugging
				for(tempIdx = 0; tempIdx < CEPST_TEMPLATE_NUM; ++tempIdx)
					//To prevent untrained CCA
					simiArr[tempIdx] = cepstraTemplates[tempIdx].effHeight == 0 ? FLT_MAX : genSimilarity(&cepstraBuff, cepstraTemplates + tempIdx);
				
				minSimi = FLT_MAX;
				for(tempIdx = 0; tempIdx < CEPST_TEMPLATE_NUM; ++tempIdx)
					if(simiArr[tempIdx] < minSimi)
					{
						minSimi = simiArr[tempIdx];
						simiTempIdx = tempIdx;
					}
				
				//show result
				showResult(simiTempIdx);//setLEDDisplay((getLEDDisplay() & 0xFFF0) | (1 << simiTempIdx));
				break;
			}
			clearProc();//toggleLED(5 + btnFSM.currState->mode);
		}
	}
	
}


void InitFSM()
{
	btnFSM.stReady.mode = ready;
	btnFSM.stReady.next[3] = &btnFSM.stTrain[0];
	
	btnFSM.stTrain[0].mode = train;
	btnFSM.stTrain[0].word = one;
	btnFSM.stTrain[0].next[1] = &btnFSM.stTrain[3]; 
	btnFSM.stTrain[0].next[2] = &btnFSM.stTrain[1]; 
	btnFSM.stTrain[0].next[3] = &btnFSM.stTest; 
	
	btnFSM.stTrain[1].mode = train;
	btnFSM.stTrain[1].word = two;
	btnFSM.stTrain[1].next[1] = &btnFSM.stTrain[0]; 
	btnFSM.stTrain[1].next[2] = &btnFSM.stTrain[2]; 
	btnFSM.stTrain[1].next[3] = &btnFSM.stTest; 

	btnFSM.stTrain[2].mode = train;
	btnFSM.stTrain[2].word = three;
	btnFSM.stTrain[2].next[1] = &btnFSM.stTrain[1]; 
	btnFSM.stTrain[2].next[2] = &btnFSM.stTrain[3]; 
	btnFSM.stTrain[2].next[3] = &btnFSM.stTest; 
	
	
	btnFSM.stTrain[3].mode = train;
	btnFSM.stTrain[3].word = four;
	btnFSM.stTrain[3].next[1] = &btnFSM.stTrain[2]; 
	btnFSM.stTrain[3].next[2] = &btnFSM.stTrain[0]; 
	btnFSM.stTrain[3].next[3] = &btnFSM.stTest; 
	
	btnFSM.stTest.mode = test;
	btnFSM.stTest.next[3] = &btnFSM.stTrain[0];
	
	btnFSM.currState = btnFSM.prevState = &btnFSM.stReady;
}



void flushStateLED(state_t state)
{
	setLEDDisplay(((1 << (state.word - 1)) << state.mode) | (1 << (state.mode + 7)));
}

