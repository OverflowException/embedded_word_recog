#include <cdefBF561.h>
#include <ccblkfn.h>
#include "btnled.h"

void initButtons(void)
{
	//Set all pins connected to buttons as input
	*pFIO0_DIR &= ~(0x01E0);
	//Interrupt on rising edge, remember to clear flag value register in ISR. 
	//DO NOT USE FALLING EDGE! OR THERE WILL BE ONE UNKNOWN INTERRUPT IMMEDIATELY AFTER SETTING UP BUTTON INTERRUPTS
	*pFIO0_POLAR &= ~(0x01E0);
	*pFIO0_EDGE |= 0x01E0;
	//Enable input
	*pFIO0_INEN |= 0x01E0;
	ssync();
}   

void initLEDs(void)
{
	//Set all pins connected to LEDs as output
	*pFIO2_DIR |= 0xFFFF;
	ssync();
}

void turnOnLED(u32_t led)
{
	if(led > 15)
		return;
	*pFIO2_FLAG_S = (1 << led);
	ssync();
}

void turnOffLED(u32_t led)
{
	if(led > 15)
		return;
	*pFIO2_FLAG_C = (1 << led);
	ssync();
}

void toggleLED(u32_t led)
{
	if(led > 15)
		return;
	*pFIO2_FLAG_T = (1 << led);
	ssync();
}

void initButtonsInt(void)
{
	//All programmable flags's interrupt channel is mapped to IVG11
	//register interrupt handler to IVG11
	register_handler(ik_ivg11, ButtonsISR);
	//Enable interrupt
	*pFIO0_MASKA_S = 0x01E0;
	
	*pILAT |= EVT_IVG11;	
	ssync();
	*pSICA_IMASK1 |= SIC_MASK(47);
	ssync();
}

void setLEDDisplay(u16_t pattern)
{
	*pFIO2_FLAG_D = pattern;
	ssync();
}

u16_t getLEDDisplay(void)
{
	return *pFIO2_FLAG_D;
}

bool getLEDStatus(u32_t led)
{
	if(led > 15)
		return false;
	return (bool)(*pFIO2_FLAG_D & (1 << led));
}
