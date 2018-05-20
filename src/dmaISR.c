#include <cdefBF561.h>
#include <ccblkfn.h>
#include "dmacodec.h"
#include "global.h"

EX_INTERRUPT_HANDLER(Sport0RXISR)
{	
	//confirm interrupt handling	
	*pDMA2_0_IRQ_STATUS = 0x0001;
	ssync();
	//startRec = true;
	recAction = end;
}
