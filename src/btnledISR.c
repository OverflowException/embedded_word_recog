#include <cdefBF561.h>
#include <ccblkfn.h>
#include "btnled.h"
#include "global.h"

#define BTN0_PR	(1 << BTN0_PF)
#define BTN1_PR	(1 << BTN1_PF)
#define BTN2_PR	(1 << BTN2_PF)
#define BTN3_PR	(1 << BTN3_PF)


EX_INTERRUPT_HANDLER(ButtonsISR)
{
	//Read flag data register
	u16_t btnInfo = *pFIO0_FLAG_D & 0x01E0;
	//Becuase this is a rising edge interrupt, clear flag data register
	*pFIO0_FLAG_C = 0x01E0;
	
	recAction = skip;
	
	//FSM state transfer
	switch(btnInfo)
	{
	case (1 << BTN0_PF):	//btn0
		recAction = start;
		btnFSM.currState = (btnFSM.currState->next[0] != NULL ? btnFSM.currState->next[0] : btnFSM.currState);
		break;
	case (1 << BTN1_PF):	//btn1
		btnFSM.currState = (btnFSM.currState->next[1] != NULL ? btnFSM.currState->next[1] : btnFSM.currState);
		break;
	case (1 << BTN2_PF):	//btn2
		btnFSM.currState = (btnFSM.currState->next[2] != NULL ? btnFSM.currState->next[2] : btnFSM.currState);
		break;
	case (1 << BTN3_PF):	//btn3
		btnFSM.currState = (btnFSM.currState->next[3] != NULL ? btnFSM.currState->next[3] : btnFSM.currState);
		break;
	default:
		setLEDDisplay(0xFFFF);
		break;
	}
}
