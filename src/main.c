#include <ccblkfn.h>
#include <fract.h>
#include <float.h>
#include "dmacodec.h"
#include "deftypes.h"
#include "global.h"
#include "dataprocess.h"
#include "btnled.h"
#include "monitor.h"

//Pages containing CCA templates. Low speed pages
section("l2_sram") page_t lsPages[CEPST_TEMPLATE_NUM];
//Pages containing CCA templates. High speed pages
section("L1_data_b") page_t hsPages[2];
//CCA testing buff
section("L1_data_a") cepstra_t cepstraTest;
//Input audio data
section("sdram_bank1") audio_t inAudio;

//Twiddle factors for FFT
section("L1_data_a") complex_fract16 ffttwid[FFT_SIZE / 2];
//Twiddle factors for DCT
section("L1_data_a") complex_fract16 dcttwid[SPECT_W * 4];
section("sdram_bank1") float logLut[FRACT16_NUM];
////////////////

section("L1_data_a") float simiArr[CEPST_TEMPLATE_NUM];
float minSimi = FLT_MAX;
u16_t simiTempIdx = 0;

volatile fsm_t btnFSM;
volatile action_t recAction;

void initFSM(void);
void initPages(void);
void matchTemplates(void);
void showPageInfo();

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
	initButtonsInt();
	
	twidfftrad2_fr16(ffttwid, FFT_SIZE);
	twidfftrad2_fr16(dcttwid, SPECT_W * 4);
	genLogLut(logLut);
	
	initFSM();
	
	initPages();
	
	u16_t tempIdx, currBufIdx, nextBufIdx;
	
	while(true)
	{
		if(btnFSM.currState != btnFSM.prevState)	//button pressed, state change occured
		{
			flushStateMonitor(btnFSM.currState);
			btnFSM.prevState = btnFSM.currState;
		}
		
		recAction = wait;
		while(recAction == wait);	//wait for recording instruction, if get an skip, do nothing, continue next cycle
		if(recAction == start)
		{
			setRec();//toggleLED(6 + btnFSM.currState->mode);
			
			restartAudioIn(inAudio.data);
			while(recAction != end);	//wait for DMA to finish
			clearRec();
			setProc();
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

//Set CL bit, set BI bit, clear  set source page IC bit, start DMA
#define MOVE_PAGE(LSIDX, HSIDX)	P_SET_CL(lsPages[LSIDX]); P_SET_BI(lsPages[LSIDX], HSIDX); P_CLEAR_IC(lsPages[P_GET_TI(hsPages[HSIDX])]); P_SET_IC(lsPages[LSIDX]); startMemDMA((void*)&(lsPages[LSIDX]), (void*)&(hsPages[HSIDX]), sizeof(page_t))
#define OPPO_HSIDX(HSIDX)		(((HSIDX) + 1) & 0x01)
#define GEN_SIMI(HSIDX)			simiArr[P_GET_TI(hsPages[(HSIDX)])] = (genSimilarity(&cepstraTest, &hsPages[(HSIDX)].content))
#define WAIT_MOVE				while(!memDMADone())

void initPages()
{
	u8_t idx;
	//Init all low speed pages' TI
	for(idx = 0; idx < CEPST_TEMPLATE_NUM; ++idx)
		P_SET_TI(lsPages[idx], idx);
	
	//Init all high speed pages' TI
	for(idx = 0; idx < 2; ++idx)
		P_SET_TI(hsPages[idx], idx);
	
}

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

void initFSM()
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

