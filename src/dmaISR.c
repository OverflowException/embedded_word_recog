#include <cdefBF561.h>
#include <ccblkfn.h>
#include "dmacodec.h"
#include "global.h"

EX_INTERRUPT_HANDLER(Sport0RXISR)
{	
	//CLear DMA_DONE bit to confirm interrupt handling. DMA_DONE bit is write-one-to-clear
	*pDMA2_0_IRQ_STATUS = 0x0001;
	ssync();
	//startRec = true;
	recAction = end;
}
