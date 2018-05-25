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

//Pages containing CCA templates. Low speed pages
section("l2_sram") page_t lsPages[CEPST_TEMPLATE_NUM];
//Pages containing CCA templates. High speed pages
section("L1_data_b") page_t hsPages[2];
//CCA testing buff
section("L1_data_a") cepstra_t cepstraTest;


//Twiddle factors for FFT
section("L1_data_a") complex_fract16 ffttwid[FFT_SIZE / 2];
//Twiddle factors for DCT
section("L1_data_a") complex_fract16 dcttwid[SPECT_W * 4];
float logLut[FRACT16_NUM];
////////////////

section("L1_data_a") float simiArr[CEPST_TEMPLATE_NUM];
float minSimi = FLT_MAX;
u16_t simiTempIdx = 0;

volatile fsm_t btnFSM;
volatile action_t recAction;

void InitFSM(void);
void matchTemplates(void);

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
	
	//Set all low speed pages' TI bits
	for(tempIdx = 0; tempIdx < CEPST_TEMPLATE_NUM; ++tempIdx)
		P_SET_TI(lsPages[tempIdx], tempIdx);
	
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
							lsPages[tempIdx].content.data, 
							&lsPages[tempIdx].content.effHeight, 
							ffttwid,
							dcttwid,
							logLut);
				P_CLEAR_CL(lsPages[tempIdx]);
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
				/*startMemDMA((void*)cepstraTemplates, (void*)cepstraBuff, sizeof(cepstra_t));
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
				}*/
				
				matchTemplates();
				//Find min
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

/*
#define	P_GET_VALID(P)	((P).info & 0xc0)
#define P_GET_TI(P)		((P).info & 0x03)
#define P_GET_BI(P)		(((P).info & 0x04) >> 2)

#define P_SET_CL(P)		((P).info |= 0x80)
#define P_CLEAR_CL(P)	((P).info &= ~(0x80))
#define P_SET_IC		((P).info |= 0x40)
#define	P_CLEAR_IC		((P).info &= ~(0x40))
#define P_SET_BI(P, B)	((P).info |= ((B << 2) & 0x04))
#define P_SET_TI(P,	T)	((P).info |= (T & 0x03))
*/


//Set CL bit, set BI bit, clear  set source page IC bit, start DMA
#define MOVE_PAGE(LSIDX, HSIDX)	P_SET_CL(lsPages[LSIDX]); P_SET_BI(lsPages[LSIDX], HSIDX); P_CLEAR_IC(lsPages[P_GET_TI(hsPages[HSIDX])]); P_SET_IC(lsPages[LSIDX]); startMemDMA((void*)&(lsPages[LSIDX]), (void*)&(hsPages[HSIDX]), sizeof(page_t))
#define OPPO_HSIDX(HSIDX)		(((HSIDX) + 1) & 0x01)
#define GEN_SIMI(HSIDX)			simiArr[P_GET_TI(hsPages[(HSIDX)])] = (genSimilarity(&cepstraTest, &hsPages[(HSIDX)].content))
#define WAIT_MOVE				while(!memDMADone())

void matchTemplates()
{
	//Contain invalid page indices
	u8_t ivPageIds[CEPST_TEMPLATE_NUM];
	//Total number of invalid pages
	u8_t ivPageNum = 0;
	
	u8_t lsIdx, hsIdx, ivIdx, vIdx;
	//Traverse all low speed pages, check validity, record invalid page indices
	for(lsIdx = 0; lsIdx < CEPST_TEMPLATE_NUM; ++lsIdx)
		if(!P_GET_VALID(lsPages[lsIdx]))
			ivPageIds[ivPageNum++] = lsIdx;
		else
			vIdx = lsIdx;
			
	switch(ivPageNum)
	{
		//All pages are valid
		case 4:
			MOVE_PAGE(0, 0);
			for(lsIdx = 1; lsIdx < CEPST_TEMPLATE_NUM; ++lsIdx)
			{
				WAIT_MOVE;
				hsIdx = lsIdx & 0x01;
				MOVE_PAGE(lsIdx, hsIdx);
				GEN_SIMI(OPPO_HSIDX(hsIdx));
			}
			GEN_SIMI(1);
			break;
		
		//3 invalid pages
		case 3:
			//See which high speed page is valid
			hsIdx = P_GET_BI(lsPages[vIdx]);
			for(ivIdx = 0; ivIdx < 3; ++ivIdx)
			{
				MOVE_PAGE(ivPageIds[ivIdx], (hsIdx = OPPO_HSIDX(hsIdx)));
				GEN_SIMI(OPPO_HSIDX(hsIdx));
				WAIT_MOVE;
			}
			GEN_SIMI(hsIdx);			
			break;
		
		//2 invalid pages
		case 2:
			GEN_SIMI(0);
			for(ivIdx = 0; ivIdx < 2; ++ivIdx)
			{
				MOVE_PAGE(ivPageIds[ivIdx], ivIdx);
				GEN_SIMI(OPPO_HSIDX(ivIdx));
				WAIT_MOVE;
			}
			GEN_SIMI(1);
			break;
		
		//This should never happen
		default:
			vIdx = 0;
			break;
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
