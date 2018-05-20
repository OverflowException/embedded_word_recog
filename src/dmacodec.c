#include <cdefBF561.h> 
#include "global.h"
#include "deftypes.h"
#include "dmacodec.h"

#include <sys\exception.h>
#include <ccblkfn.h>
#include <sysreg.h>


volatile u16_t sCodec1836TxRegs[CODEC_1836_REGS_LENGTH]= 
{	
	DAC_CONTROL_1	| 0x010,	// 16 bit data width
	DAC_CONTROL_2	| 0x000,
	DAC_VOLUME_0	| 0x3FF,
	DAC_VOLUME_1	| 0x3FF,
	DAC_VOLUME_2	| 0x3FF,
	DAC_VOLUME_3	| 0x3FF,
	DAC_VOLUME_4	| 0x000,
	DAC_VOLUME_5	| 0x000,
	ADC_CONTROL_1	| 0x020,    // left gain 12dB
	ADC_CONTROL_2	| 0x020,	// 16 bit data width
	ADC_CONTROL_3	| 0x00A
};

void Init1836(void)
{	
	volatile int wait_reset;
	volatile int wait_dma_finish;

	// reset codec
	// set PF15 as output
	*pFIO0_DIR |= (1 << AD1836_RESET_bit); 
	ssync();
	// clear bit to enable AD1836
	*pFIO0_FLAG_S = (1 << AD1836_RESET_bit);	
	ssync();

	// wait to recover from reset
	for (wait_reset=0; wait_reset<0xf000; wait_reset++);

	// Enable PF4
	*pSPI_FLG = FLS4;
	// Set baud rate SCK = HCLK/(2*SPIBAUD)	
	*pSPI_BAUD = 16;
	// configure spi port
	// SPI DMA write, 16-bit data, MSB first, SPI Master
	*pSPI_CTL = TIMOD_DMA_TX | SIZE | MSTR;
	
	// Set up DMA2 channel 4 to SPI transmit
	*pDMA2_4_PERIPHERAL_MAP = 0x4000;
	
	// Configure DMA2 channel4
	// 16-bit transfers
	*pDMA2_4_CONFIG = WDSIZE_16;
	// Start address of data buffer
	*pDMA2_4_START_ADDR = (void *)sCodec1836TxRegs;
	// DMA inner loop count
	*pDMA2_4_X_COUNT = CODEC_1836_REGS_LENGTH;
	// Inner loop address increment
	*pDMA2_4_X_MODIFY = 2;
	
	// enable DMAs
	*pDMA2_4_CONFIG = (*pDMA2_4_CONFIG | DMAEN);
	// enable spi
	*pSPI_CTL = (*pSPI_CTL | SPE);
	ssync();
	
	// wait until dma transfers for spi are finished 
	for (wait_dma_finish=0; wait_dma_finish<0xaff; wait_dma_finish++);
	
	// disable spi
	*pSPI_CTL = 0x0000;
}

void InitSport0In(void)
{
	// Sport0 receive configuration
	// External CLK, External Frame sync, MSB first, Active Low
	// 16-bit data, Stereo frame sync enable
	*pSPORT0_RCR1 = RFSR | RCKFE | LRFS;
	*pSPORT0_RCR2 = RSFSE | RXSE | SLEN_16;
}

void InitSport0Out(void)
{
	// Sport0 transmit configuration
	// External CLK, External Frame sync, MSB first, Active Low
	// 16-bit data, Secondary side enable, Stereo frame sync enable
	*pSPORT0_TCR1 = TCKFE | TFSR | LTFS;
	*pSPORT0_TCR2 = TSFSE | TXSE | SLEN_16;
}

void InitDMACodecIn(u16_t* inmem)
{
	//DMA2 channel 0 default peripheral SPORT0 RX
	//Stop mode, 2D, interrupt on outerloop completion, 16 bits tranfer, to memory
	*pDMA2_0_CONFIG = DI_EN | WDSIZE_16 | DMA2D | WNR;
	ssync();
	*pDMA2_0_START_ADDR = (void*)inmem;
	*pDMA2_0_X_COUNT = AUDIO_SAMPLE_LEN * AUDIO_DURATION_S;
	*pDMA2_0_X_MODIFY = 2;
	*pDMA2_0_Y_COUNT = AUDIO_SAMPLE_RATE;
	*pDMA2_0_Y_MODIFY = 2;
	ssync();
}

void InitDMACodecOut(u16_t* outmem, u32_t snum)
{
	//DMA2 channel 1 default peripheral SPORT0 TX
	*pDMA2_1_START_ADDR = (void *)outmem;
	//if snum is over 16 bits, use 2D DMA
	if(snum > 0xFFFF)
	{
		*pDMA2_1_CONFIG = WDSIZE_16 | DMA2D;
		*pDMA2_1_Y_COUNT = AUDIO_SAMPLE_LEN * (u16_t)(snum >> 16);
		*pDMA2_1_Y_MODIFY = 2;
	}
	//if snum is within 16 bits, use 1D DMA
	else
	{
		*pDMA2_1_CONFIG = WDSIZE_16;
	}
	
	*pDMA2_1_X_COUNT = (u16_t)(snum << 16 >> 16);
	*pDMA2_1_X_MODIFY = 2;
	ssync();
	
}

//Do not change the order of this restart configuration. This is mythology.
void restartAudioIn(u16_t* inmem)
{
	*pDMA2_0_START_ADDR = (void*)inmem;
	*pDMA2_0_X_COUNT = AUDIO_SAMPLE_LEN * AUDIO_DURATION_S;
	*pDMA2_0_X_MODIFY = 2;
	*pDMA2_0_Y_COUNT = AUDIO_SAMPLE_RATE;
	*pDMA2_0_Y_MODIFY = 2;
	ssync();	
	*pSPORT0_RCR1 	|= RSPEN;
	ssync();
	*pDMA2_0_CONFIG = DI_EN | WDSIZE_16 | DMA2D | WNR | DMAEN;
	ssync();
}

void restartAudioOut(u16_t* outmem, u32_t snum)
{
	*pSPORT0_TCR1 	|= TSPEN;
	ssync();
	//DMA2 channel 1 default peripheral SPORT0 TX
	*pDMA2_1_START_ADDR = (void *)outmem;
	//*pDMA2_1_X_COUNT = (u16_t)(snum /*<< 16 >> 16*/);
	*pDMA2_1_X_COUNT = 65000;
	*pDMA2_1_X_MODIFY = 2;
	ssync();
	*pDMA2_1_CONFIG = 0x1000 | WDSIZE_16 | RESTART| DMAEN;
	ssync();
	/*//if snum is over 16 bits, use 2D DMA
	if(snum > 0xFFFF)
	{
		*pDMA2_1_Y_COUNT = AUDIO_SAMPLE_LEN * (u16_t)(snum >> 16);
		*pDMA2_1_Y_MODIFY = 2;
		*pDMA2_1_CONFIG = WDSIZE_16 | DMA2D | DMAEN;
		ssync();
	}
	//if snum is within 16 bits, use 1D DMA
	else
	{
		*pDMA2_1_CONFIG = WDSIZE_16 | DMAEN;
		ssync();
	}*/
}

void InitDMACodecInInterrupts(void)
{
	// assign interrupt channel 23 (DMA2_0) to IVG9 
	*pSICA_IAR2 = Peripheral_IVG(23,9);	
	
	// assign ISRs to interrupt vectors
	// Sport0 RX ISR -> IVG 9
	register_handler(ik_ivg9, Sport0RXISR);		

	// clear pending IVG9 interrupts
	*pILAT |= EVT_IVG9;		
	ssync();
	
	// enable Sport0 RX interrupt
	*pSICA_IMASK0 |= SIC_MASK(23);
	ssync();	
}


void EnableDMACodecIn(void)
{
	//Enable DMA 2 channel 0
	*pDMA2_0_CONFIG	|= DMAEN;
	//EnableSport0 RX
	*pSPORT0_RCR1 	|= RSPEN;
	ssync();
}

void EnableDMACodecOut(void)
{
	//Enable DMA 2 channel 1
	*pDMA2_1_CONFIG	|= DMAEN;
	//Enable Sport0 TX
	*pSPORT0_TCR1 	|= TSPEN;
	ssync();
}
