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

//CCA templates
section("l2_sram") cepstra_t cepstraTemplates[CEPST_TEMPLATE_NUM];
//High speed CCA ping-pong buffs
section("L1_data_b") cepstra_t cepstraBuff[2];
//CCA testing buff
section("L1_data_a") cepstra_t cepstraTest;


//Twiddle factors for FFT
section("L1_data_a") complex_fract16 ffttwid[FFT_SIZE / 2];
//Twiddle factors for DCT
section("L1_data_a") complex_fract16 dcttwid[SPECT_W * 4];
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

 	init1836();
 	
	initSport0In();
	initSport0Out();
	initDMACodecInInterrupts();
	
	initButtons();
	//InitLEDs();
	initButtonsInt();
	
	twidfftrad2_fr16(ffttwid, FFT_SIZE);
	twidfftrad2_fr16(dcttwid, SPECT_W * 4);
	genLogLut(logLut);
	
	InitFSM();
	
	
	u16_t tempIdx, currBufIdx, nextBufIdx;
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
							cepstraTest.data, 
							&cepstraTest.effHeight, 
							ffttwid,
							dcttwid,
							logLut);
			
				//Transfer the first CCA from L2 to L1
				startMemDMA((void*)cepstraTemplates, (void*)cepstraBuff, sizeof(cepstra_t));
				for(tempIdx = 0; tempIdx < CEPST_TEMPLATE_NUM; ++tempIdx)
				{
					//Wait for DMA to finish
					while(!memDMADone());
					
					//Start next buffer transmission
					currBufIdx = tempIdx & 0x0001;
					nextBufIdx = (tempIdx + 1) & 0x0001;
					startMemDMA((void*)&cepstraTemplates[tempIdx + 1], (void*)&cepstraBuff[nextBufIdx], sizeof(cepstra_t));
					
					//To prevent untrained CCA
					simiArr[tempIdx] = cepstraBuff[currBufIdx].effHeight == 0 ? 
										FLT_MAX : genSimilarity(&cepstraTest, &cepstraBuff[currBufIdx]);
				}
				
				//Match templates
				minSimi = FLT_MAX;
				for(tempIdx = 0; tempIdx < CEPST_TEMPLATE_NUM; ++tempIdx)
					if(simiArr[tempIdx] < minSimi)
					{
						minSimi = simiArr[tempIdx];
						simiTempIdx = tempIdx;
					}
				
				//show result
				showResult(simiTempIdx);
				break;
			}
			clearProc();
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

