#include "monitor.h"
#include "fonts.h"

char ycc_colors[8][4] = 
{
	{0x5A, 0x51, 0xF0, 0x51},
	{0x36, 0x91, 0x22, 0x91},
	{0xF0, 0x29, 0x6E, 0x29},
	{0x10, 0xD2, 0x92, 0xD2},
	{0xCA, 0x6A, 0xDE, 0x6A},
	{0xA6, 0xAA, 0x10, 0xAA},
	{0x80, 0xEB, 0x80, 0xEB},
	{0x80, 0x10, 0x80, 0x10}
};

section("sdram_bank0") volatile u16 sFrame0[FRAMESIZE];

// 4 output buffers used by AD7179
ADI_DEV_2D_BUFFER Out1_Buffer2D, Out2_Buffer2D, Out3_Buffer2D, Out4_Buffer2D; 

ADI_DEV_DEVICE_HANDLE AD7183DriverHandle;// handle to the ad7183 driver
ADI_DEV_DEVICE_HANDLE AD7179DriverHandle;// handle to the ad7179 driver

adi_twi_pseudo_port TWIPseudo= {ADI_FLAG_PF0,ADI_FLAG_PF1,ADI_TMR_GP_TIMER_3,(ADI_INT_PERIPHERAL_ID)NULL};
ADI_DEV_CMD_VALUE_PAIR PseudoTWIConfig[]={
	{ADI_TWI_CMD_SET_PSEUDO,(void *)(&TWIPseudo)},
	{ADI_DEV_CMD_SET_DATAFLOW_METHOD,(void *)ADI_DEV_MODE_SEQ_CHAINED},
	{ADI_DEV_CMD_SET_DATAFLOW,(void *)TRUE},
	{ADI_DEV_CMD_END,NULL}
};

void startADV7179(void)
{
	u32 Result = 0;
	ezEnableVideoEncoder();// enable AD7179
	//ezDisableVideoEncoder();// enable AD7179

	// open the ad7179 driver
	ezErrorCheck(adi_dev_Open(adi_dev_ManagerHandle,   // device manager handle
                 &ADIADV7179EntryPoint,     // entry point of device driver to open
                 0,                         // the device number (0th AD7179) 
	             (void *)0x7179,            // client handle (0x7179 will be given to the AD7179 encoder driver)
    	         &AD7179DriverHandle,       // location where AD7179 device driver handle will be stored
        	     ADI_DEV_DIRECTION_OUTBOUND,// direction the device is to be opened
            	 adi_dma_ManagerHandle,          // DMA Manager handle
               	 NULL,          		   // DCB handle (NULL cause we want live callbacks)
	             CallbackFunction));         // address of callback function
	
	// configure the AD7179 driver(Set PPI Device number)
	ezErrorCheck(adi_dev_Control( AD7179DriverHandle, ADI_ADV717x_CMD_SET_PPI_DEVICE_NUMBER, (void*)1 ));
	
	//  configure the AD7179 driver(Open PPI Device)
	ezErrorCheck(adi_dev_Control( AD7179DriverHandle, ADI_ADV717x_CMD_SET_PPI_STATUS, (void*)ADI_ADV717x_PPI_OPEN ));

	// Send Pseudo TWI Configuration table to AD7179 driver
	ezErrorCheck(adi_dev_Control(AD7179DriverHandle,ADI_ADV717x_CMD_SET_TWI_CONFIG_TABLE,(void*)PseudoTWIConfig));
	
	// ADV7179 register configuration array for PAL mode
    ADI_DEV_ACCESS_REGISTER ADV7179_Cfg[]={{ADV717x_MR0,	0x05},		// register address, configuration data
    					   {ADV717x_MR1,	0x10},
    					   {ADV717x_MR2,	0x00},
    					   {ADV717x_MR3,	0x00},
    					   {ADV717x_MR4,	0x00},
    					   {ADV717x_TMR0, 	0x08}, 
    					   {ADV717x_TMR1, 	0x00},
    					   {ADI_DEV_REGEND,0	}};	// End of register access

    ezErrorCheck(adi_dev_Control(AD7179DriverHandle, ADI_ADV717x_CMD_SET_SCF_REG, (void *) ADV717x_SCF_VALUE_PAL_BI));
    
    //configure ADV7179 in selected mode
    ezErrorCheck(adi_dev_Control(AD7179DriverHandle, ADI_DEV_CMD_REGISTER_TABLE_WRITE, (void *)ADV7179_Cfg));
    
    
    // populate AD7179 outbound buffers
	Out1_Buffer2D.Data = (void*)sFrame0;// address of the data storage
	Out1_Buffer2D.ElementWidth = sizeof(u32);
	Out1_Buffer2D.XCount = (FRAME_DATA_LEN/2);
	Out1_Buffer2D.XModify = 4;
	Out1_Buffer2D.YCount = NUM_LINES;
	Out1_Buffer2D.YModify = 4;
	Out1_Buffer2D.CallbackParameter = NULL;
	//Out1_Buffer2D.pNext = &Out2_Buffer2D;// point to the next buffer in the chain
	Out1_Buffer2D.pNext = NULL;
    
	
	
	ezErrorCheck(adi_dev_Control(AD7179DriverHandle, ADI_DEV_CMD_SET_DATAFLOW_METHOD, (void *)ADI_DEV_MODE_CHAINED_LOOPBACK));
    ezErrorCheck(adi_dev_Write(AD7179DriverHandle, ADI_DEV_2D, (ADI_DEV_BUFFER *)&Out1_Buffer2D));

	// start outputting video data
	ezErrorCheck(adi_dev_Control(AD7179DriverHandle, ADI_DEV_CMD_SET_DATAFLOW, (void*)TRUE));
    
}

void setMonitorBg(u16_t color)
{
	if(color > 7)
		return;
	
	adi_video_FrameFill	((char*)sFrame0, PAL_IL, ycc_colors[color]); 
}

void formatDisplayBuffers(void)
{
	adi_video_FrameFormat((char*)sFrame0, PAL_IL);
}


void applyMask(char* frame_ptr, const u8_t mask[][2], u32_t msize, u32_t x, u32_t y, char* ycbcr_data)
{
	u32_t index;
	for(index = 0; index < msize; ++index)
		adi_video_PixelSet(frame_ptr, x + mask[index][0], y + mask[index][1], ycbcr_data);
}


void CallbackFunction(void *AppHandle, u32 Event, void *pArg)
{
	/*switch (Event)
	{
		// CASE (buffer processed)
        	case ADI_DEV_EVENT_BUFFER_PROCESSED:
        	// IF (it's an outbound buffer that's been sent out from AD7179 encoder)
    	    	if (AppHandle == (void *)0x7179) {
#ifndef USE_LOOPBACK // chained buffer
		// requeue outbound buffer to ADV7179
		adi_dev_Write(AD7179DriverHandle, ADI_DEV_2D, pArg);
#endif   
       	    	
       	    // ELSE (it's an inbound buffer that's been filled by the AD7183 decoder)
	        } else {
    	        
#ifndef USE_LOOPBACK // chained buffer
		// requeue inbound buffer to ADV7183
		adi_dev_Read(AD7183DriverHandle, ADI_DEV_2D, pArg);
#endif 	    
        	    	
       	    // ENDIF
   	        }
        	
           break;
		
	}*/

	// return
}


//void adi_video_RegionSet(char* frame_ptr, unsigned long x, unsigned long y, unsigned long width, unsigned long height, char *ycbcr_data)
//row 40-540
//col 20-340
void flushStateMonitor(volatile state_t* state)
{
	adi_video_RegionSet((char*)sFrame0, SECT1_X, SECT1_Y, SECT_W, SECT_H, ycc_colors[BLACK]);
	if(state->mode == test)
		applyMask((char*)sFrame0, mask_test, msize_test, SECT1_X, SECT1_Y, ycc_colors[GREEN]);
		//adi_video_RegionSet((char*)sFrame0, SECT1_X, SECT1_Y, SECT_W, SECT_H, ycc_colors[BLUE]);	//test: left-up corner BLUE
	else
		applyMask((char*)sFrame0, mask_train, msize_train, SECT1_X, SECT1_Y, ycc_colors[GREEN]);
		//adi_video_RegionSet((char*)sFrame0, SECT1_X, SECT1_Y, SECT_W, SECT_H, ycc_colors[YELLOW]);	//train: left-up corner YELLOW
		
	
	showResult(state->word - 1);
	//adi_video_RegionSet((char*)sFrame0, 180, 40, 160, 250, ycc_colors[state->word - 1]);
}

void setRec(void)
{
	applyMask((char*)sFrame0, mask_rec, msize_rec, SECT3_X, SECT3_Y, ycc_colors[RED]);
	//adi_video_RegionSet((char*)sFrame0, SECT4_X, SECT4_Y, SECT_W, SECT_H, ycc_colors[RED]);
}

void clearRec(void)
{
	adi_video_RegionSet((char*)sFrame0, SECT3_X, SECT3_Y, SECT_W, SECT_H, ycc_colors[BLACK]);
}

void setProc(void)
{
	applyMask((char*)sFrame0, mask_proc, msize_proc, SECT3_X, SECT3_Y, ycc_colors[RED]);
	//adi_video_RegionSet((char*)sFrame0, SECT3_X, SECT3_Y, SECT_W, SECT_H, ycc_colors[RED]);
}

void clearProc(void)
{
	adi_video_RegionSet((char*)sFrame0, SECT3_X, SECT3_Y, SECT_W, SECT_H, ycc_colors[BLACK]);
}

void showResult(u16_t idx)	//starting from 0
{
	adi_video_RegionSet((char*)sFrame0, SECT2_X, SECT2_Y, SECT_W, SECT_H, ycc_colors[BLACK]);
	switch(idx + 1)
	{
	case one:
		applyMask((char*)sFrame0, mask_one, msize_one, SECT2_X, SECT2_Y, ycc_colors[WHITE]);
		break;
	case two:
		applyMask((char*)sFrame0, mask_two, msize_two, SECT2_X, SECT2_Y, ycc_colors[WHITE]);
		break;
	case three:
		applyMask((char*)sFrame0, mask_three, msize_three, SECT2_X, SECT2_Y, ycc_colors[WHITE]);
		break;
	case four:
		applyMask((char*)sFrame0, mask_four, msize_four, SECT2_X, SECT2_Y, ycc_colors[WHITE]);
		break;
	default:
		break;
	}
}

