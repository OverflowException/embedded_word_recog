/*****************************************************************************
 * monitor_test.c
 *****************************************************************************/

#include "monitor.h"

extern volatile u16_t sFrame0[FRAMESIZE];
extern volatile u16_t sFrame1[FRAMESIZE];
extern volatile u16_t sFrame2[FRAMESIZE];
extern volatile u16_t sFrame3[FRAMESIZE];

int main(void)
{
	adi_ssl_Init();	//Power, EBIU, DMA etc.
	
	u32_t Result = 0;
	
	u16_t led_index = 0;
	for(led_index = 0; led_index < 16; ++led_index)
		ezInitLED(led_index);	
	
	startADV7179();
	
	adi_video_FrameFormat((char*)sFrame0, PAL_IL);
	adi_video_FrameFormat((char*)sFrame1, PAL_IL);
	adi_video_FrameFormat((char*)sFrame2, PAL_IL);
	adi_video_FrameFormat((char*)sFrame3, PAL_IL);
	
	char green[4] = {0x36, 0x91, 0x22, 0x91};
	char red[4] = {0x5A, 0x51, 0xF0, 0x51};
	
	adi_video_FrameFill	((char*)sFrame0, PAL_IL, red); 
	adi_video_FrameFill	((char*)sFrame1, PAL_IL, red); 
	adi_video_FrameFill	((char*)sFrame2, PAL_IL, red); 
	adi_video_FrameFill	((char*)sFrame3, PAL_IL, red); 
	
	/*adi_video_RowFill ((char*)sFrame0, PAL_IL, 100, green); 
	adi_video_RowFill ((char*)sFrame1, PAL_IL, 100, green); 
	adi_video_RowFill ((char*)sFrame2, PAL_IL, 100, green); 
	adi_video_RowFill ((char*)sFrame3, PAL_IL, 100, green); 
	
	adi_video_RowFill ((char*)sFrame0, PAL_IL, 200, red); 
	adi_video_RowFill ((char*)sFrame1, PAL_IL, 200, red); 
	adi_video_RowFill ((char*)sFrame2, PAL_IL, 200, red); 
	adi_video_RowFill ((char*)sFrame3, PAL_IL, 200, red);*/
	
	while(1)
	{
		++Result;
	};
	
	return 0;
}
