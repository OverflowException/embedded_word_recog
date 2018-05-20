/*********************************************************************************

Copyright(c) 2006 Analog Devices, Inc. All Rights Reserved.

This software is proprietary and confidential.  By using this software you agree
to the terms of the associated Analog Devices License Agreement.

***********************************************************************
			
********* MACROS ************************************************************************/
#define USE_LOOPBACK	// chained or chained with loopback buffers
#define   PAL_FRAME     // Video frame is of PAL format
//#define READ_REG_VALUE	// read/print register value enable/disable
/*********************************************************************

Include files

*********************************************************************/
#include <drivers/adi_dev.h>			// Device manager includes
#include <drivers/decoder/adi_adv7183.h>		// AD7183 device driver includes
#include <drivers/encoder/adi_adv717x.h>		// ADV7179 device driver includes
#include <drivers/twi/adi_twi.h>			// TWI device driver includes
#include <adi_ssl_Init.h>
#include <SDK-ezkitutilities.h>
#include "stdio.h"

/*********************************************************************

Prototypes

*********************************************************************/


/*****************************************************************************

Static data

*****************************************************************************/
#define NTSC	0
#define PAL 	1

#if defined(PAL_FRAME)
#define FRAME_DATA_LEN	864			// Number of pixels(entire field) per line for PAL video format
#define NUM_LINES		625			// Number of lines per frame    
#else   // its NTSC Frame
#define FRAME_DATA_LEN	858		// Number of pixels(entire field) per line for NYSC video format
#define NUM_LINES		525			// Number of lines per frame    
#endif 

#define FRAMESIZE	FRAME_DATA_LEN * NUM_LINES

/*********************************************************************

memory for buffers and data

*********************************************************************/
// Define the DMA buffers for each frame.
// Because of SDRAM performance, each frame must be in a different bank.
section("sdram_bank0") volatile u16 sFrame0[FRAMESIZE];
section("sdram_bank1") volatile u16 sFrame1[FRAMESIZE];
section("sdram_bank2") volatile u16 sFrame2[FRAMESIZE];
section("sdram_bank3") volatile u16 sFrame3[FRAMESIZE];

// 4 input buffers used by AD7183
ADI_DEV_2D_BUFFER In1_Buffer2D, In2_Buffer2D, In3_Buffer2D, In4_Buffer2D;
 
// 4 output buffers used by AD7179
ADI_DEV_2D_BUFFER Out1_Buffer2D, Out2_Buffer2D, Out3_Buffer2D, Out4_Buffer2D; 


// Pseudo TWI Configuration 
// Pseudo TWI will be used to access ADV7183 and ADV7179 registers.
// BF561 Ports (PF0=SCL,PF1=SDA) & Timer(Timer 3) used for Pseudo TWI
adi_twi_pseudo_port TWIPseudo= {ADI_FLAG_PF0,ADI_FLAG_PF1,ADI_TMR_GP_TIMER_3,(ADI_INT_PERIPHERAL_ID)NULL};
ADI_DEV_CMD_VALUE_PAIR PseudoTWIConfig[]={
	{ADI_TWI_CMD_SET_PSEUDO,(void *)(&TWIPseudo)},
	{ADI_DEV_CMD_SET_DATAFLOW_METHOD,(void *)ADI_DEV_MODE_SEQ_CHAINED},
	{ADI_DEV_CMD_SET_DATAFLOW,(void *)TRUE},
	{ADI_DEV_CMD_END,NULL}
};

/*********************************************************************

memory for initialization of system services and device manager

*********************************************************************/



/*********************************************************************

handles to device drivers

*********************************************************************/
ADI_DEV_DEVICE_HANDLE AD7183DriverHandle;// handle to the ad7183 driver
ADI_DEV_DEVICE_HANDLE AD7179DriverHandle;// handle to the ad7179 driver

/*********************************************************************

static function prototypes

*********************************************************************/
static void InitSystemServices(void); // system services initialization
static void CallbackFunction( void *AppHandle, u32 Event,void *pArg);// device driver callback function
static void StartADV7179(void); // Configure ADV7179 driver, buffer etc
static void StartADV7183(void); // Configure ADV7183 driver, buffer etc
static void Set7179ToNTSC(void); //Configure AD7179 to NTSC mode
static void Set7179ToPAL(void);//Configure AD7179 to PAL mode
static void Read7179Regs(void);// Read AD7179 register
static void Read7183StatusReg(void); // Read AD7183 Status register

/*********************************************************************
*
*	Function:	main
*	Description:	Using the AD7183 and AD7179 device drivers,this
	                program demonstrates a simple video stream program.
					The video data is captured into SDRAM from the AD7183 decoder,
					then output through AD7179 encoder. 
					In this example,no processing is done on the video data.
*********************************************************************/

void main(void)
{
	unsigned int i,Result;									// index
	u32	ResponseCount;								// response count
	
	
	// initialize the system services
	InitSystemServices();
	
	ezInitButton(EZ_LAST_BUTTON); // enables the last button
	
	// turn off LED's	
	ezTurnOffAllLEDs();

                      
	StartADV7183();
	
	
	                      
	StartADV7179();


	
	// keep going until the last push button is pressed
    while (ezIsButtonPushed(EZ_LAST_BUTTON) == FALSE) ;

	// close the device
	ezErrorCheck(adi_dev_Close(AD7183DriverHandle));

	// close the device
	ezErrorCheck(adi_dev_Close(AD7179DriverHandle));
	
		// close the Device Manager
	ezErrorCheck(adi_dev_Terminate(adi_dev_ManagerHandle));
	
	// close down the DMA Manager
	ezErrorCheck(adi_dma_Terminate(adi_dma_ManagerHandle));
	
	
	
}

/*********************************************************************

	Function:		InitSystemServices

	Description:	Initializes the necessary system services.  

*********************************************************************/

void InitSystemServices(void) {
    
    // initialize the ezKit power,EBIU,DMA....
	adi_ssl_Init();	
	
	
	// enable and configure async memory
	//ezInit(1);

	// return
}



/*********************************************************************

	Function:	CallbackFunction

	Description:	Each type of callback event has it's own unique ID
					so we can use a single callback function for all
					callback events.  The switch statement tells us
					which event has occurred.

					Note that in the device driver model, in order to 
					generate a callback for buffer completion, the 
					CallbackParameter of the buffer must be set to a non-NULL 
					value.
*********************************************************************/

static void CallbackFunction(void *AppHandle,u32  Event,void *pArg)
{
	switch (Event)
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
		
	}

	// return
}


/*********************************************************************

	Function:	StartADV7183

	Description: Initialise Video Decoder ADV7183, set up buffers and
				 configure the decoder	
*********************************************************************/
static void StartADV7183(void){
   
		ezEnableVideoDecoder();// enable AD7183
		//ezDisableVideoDecoder();// enable AD7183
	
	// open the ad7183 driver
	ezErrorCheck(adi_dev_Open(adi_dev_ManagerHandle,		// DevMgr handle
				&ADIADV7183EntryPoint,		// pdd entry point
				0,				// device instance
	                      	(void *)0x7183,            // client handle (0x7183 will be given to the AD7183 decoder driver)
				&AD7183DriverHandle,			// DevMgr handle for this device
				ADI_DEV_DIRECTION_INBOUND,// data direction for this device
				adi_dma_ManagerHandle,			// handle to DmaMgr for this device
				NULL,				// handle to deferred callback service
				CallbackFunction));		// client's callback function
				
	/********* open AD7183-PPI ****************************************/		
	// open the AD7183-PPI device 0 (see Schematic)
	ezErrorCheck(adi_dev_Control(AD7183DriverHandle, ADI_AD7183_CMD_OPEN_PPI, (void *)0));
			
	// command PPI to work in NTSC or PAL mode
#if defined(PAL_FRAME)
		ezErrorCheck(adi_dev_Control(AD7183DriverHandle, ADI_AD7183_CMD_SET_VIDEO_FORMAT, (void *)PAL));
#else
		ezErrorCheck(adi_dev_Control(AD7183DriverHandle, ADI_AD7183_CMD_SET_VIDEO_FORMAT, (void *)NTSC));
#endif		
				
/******************* AD7183 Inbound Buffers  ***********************************************/	
	// populate the buffers that we'll use for the PPI input
	In1_Buffer2D.Data = (void*)sFrame0;
	In1_Buffer2D.ElementWidth =sizeof(u32); 
	In1_Buffer2D.XCount = (FRAME_DATA_LEN/2);
	In1_Buffer2D.XModify = 4;
	In1_Buffer2D.YCount = NUM_LINES;
	In1_Buffer2D.YModify = 4;
	In1_Buffer2D.CallbackParameter = NULL;
	In1_Buffer2D.pNext = &In2_Buffer2D;

	
	In2_Buffer2D.Data = (void*)sFrame1;
	In2_Buffer2D.ElementWidth = sizeof(u32);
	In2_Buffer2D.XCount = (FRAME_DATA_LEN/2);
	In2_Buffer2D.XModify = 4;
	In2_Buffer2D.YCount = NUM_LINES;
	In2_Buffer2D.YModify = 4;
#ifdef USE_LOOPBACK	
	In1_Buffer2D.CallbackParameter = NULL;
	In2_Buffer2D.pNext = &In3_Buffer2D;
#else	// chained buffer
	In2_Buffer2D.CallbackParameter = &In1_Buffer2D;
	In2_Buffer2D.pNext = NULL;
#endif	


	In3_Buffer2D.Data = (void*)sFrame2;
	In3_Buffer2D.ElementWidth = sizeof(u32);
	In3_Buffer2D.XCount = (FRAME_DATA_LEN/2);
	In3_Buffer2D.XModify = 4;
	In3_Buffer2D.YCount = NUM_LINES;
	In3_Buffer2D.YModify = 4;
	In3_Buffer2D.CallbackParameter = NULL;
	In3_Buffer2D.pNext = &In4_Buffer2D;


	In4_Buffer2D.Data = (void*)sFrame3;
	In4_Buffer2D.ElementWidth = sizeof(u32);
	In4_Buffer2D.XCount = (FRAME_DATA_LEN/2);
	In4_Buffer2D.XModify = 4;
	In4_Buffer2D.YCount = NUM_LINES;
	In4_Buffer2D.YModify = 4;
#ifdef USE_LOOPBACK	
	In4_Buffer2D.CallbackParameter = NULL;
#else	// chained buffer
	In4_Buffer2D.CallbackParameter = &In3_Buffer2D;
#endif	
	In4_Buffer2D.pNext = NULL;
	

	// configure the ad7183 dataflow method
#if defined(USE_LOOPBACK)
	ezErrorCheck(adi_dev_Control(AD7183DriverHandle, ADI_DEV_CMD_SET_DATAFLOW_METHOD, (void *)ADI_DEV_MODE_CHAINED_LOOPBACK));
	// give the PPI driver the buffer to process
	ezErrorCheck(adi_dev_Read(AD7183DriverHandle, ADI_DEV_2D, (ADI_DEV_BUFFER *)&In1_Buffer2D));
#else	
	ezErrorCheck(adi_dev_Control(AD7183DriverHandle, ADI_DEV_CMD_SET_DATAFLOW_METHOD, (void *)ADI_DEV_MODE_CHAINED));
	// give the PPI driver the buffer to process
	ezErrorCheck(adi_dev_Read(AD7183DriverHandle, ADI_DEV_2D, (ADI_DEV_BUFFER *)&In1_Buffer2D));
	ezErrorCheck(adi_dev_Read(AD7183DriverHandle, ADI_DEV_2D, (ADI_DEV_BUFFER *)&In3_Buffer2D));

#endif

	/********* AD7183 registers access ****************************************/		
	// Send Pseudo TWI Configuration table to AD7183 if register configuratian is needed
	ezErrorCheck(adi_dev_Control(AD7183DriverHandle,ADI_AD7183_CMD_SET_TWI_CONFIG_TABLE,(void*)PseudoTWIConfig)); 

	// do the register configuration here if needed.
#if defined(READ_REG_VALUE)
	// read AD7183 status register
	Read7183StatusReg();
#endif	
		// start capturing video data
	ezErrorCheck(adi_dev_Control(AD7183DriverHandle, ADI_DEV_CMD_SET_DATAFLOW, (void*)TRUE));
	

}


/*********************************************************************

	Function:	StartADV7170

	Description: Initialise Video Encoder ADV7179, set up buffers and
				 configure the encoder	
*********************************************************************/
static void StartADV7179(void){		
    
    u32 Result = 0;
    
	ezEnableVideoEncoder();// enable AD7179
	//ezDisableVideoEncoder();// enable AD7179

	// open the ad7179 driver
	ezErrorCheck(adi_dev_Open(  adi_dev_ManagerHandle,   // device manager handle
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
	Result = adi_dev_Control(AD7179DriverHandle,ADI_ADV717x_CMD_SET_TWI_CONFIG_TABLE,(void*)PseudoTWIConfig); 
	if ( Result != ADI_DEV_RESULT_SUCCESS) printf("Set TWI-config table failed\n");


#if defined(READ_REG_VALUE)
	// Read AD7179 registers before configuration
	Read7179Regs();
#endif								
    								
#ifdef PAL_FRAME
	Set7179ToPAL();// operate in PAL mode

#else
	Set7179ToNTSC();// operate in NTSC mode
#endif	
	
#if defined(READ_REG_VALUE)
	// Read AD7179 register value after configuration	
	Read7179Regs();
#endif	

	// populate AD7179 outbound buffers
	Out1_Buffer2D.Data = (void*)sFrame0;// address of the data storage
	Out1_Buffer2D.ElementWidth = sizeof(u32);
	Out1_Buffer2D.XCount = (FRAME_DATA_LEN/2);
	Out1_Buffer2D.XModify = 4;
	Out1_Buffer2D.YCount = NUM_LINES;
	Out1_Buffer2D.YModify = 4;
	Out1_Buffer2D.CallbackParameter = NULL;
	Out1_Buffer2D.pNext = &Out2_Buffer2D;// point to the next buffer in the chain

	
	
	
	Out2_Buffer2D.Data = (void*)sFrame1;// address of the data storage
	Out2_Buffer2D.ElementWidth = sizeof(u32);
	Out2_Buffer2D.XCount = (FRAME_DATA_LEN/2);
	Out2_Buffer2D.XModify = 4;
	Out2_Buffer2D.YCount = NUM_LINES;
	Out2_Buffer2D.YModify = 4;
#ifdef	USE_LOOPBACK	
	Out2_Buffer2D.CallbackParameter = NULL;
	Out2_Buffer2D.pNext = &Out3_Buffer2D;// point to the next buffer in the chain
#else	// chained buffer	
	Out2_Buffer2D.CallbackParameter = &Out1_Buffer2D;// generate callback, pArg = buffer address
	Out2_Buffer2D.pNext = NULL;// terminate the chain of buffers
#endif




	Out3_Buffer2D.Data = (void*)sFrame2;// address of the data storage
	Out3_Buffer2D.ElementWidth = sizeof(u32);
	Out3_Buffer2D.XCount = (FRAME_DATA_LEN/2);
	Out3_Buffer2D.XModify = 4;
	Out3_Buffer2D.YCount = NUM_LINES;
	Out3_Buffer2D.YModify = 4;
	Out4_Buffer2D.CallbackParameter = NULL;
	Out3_Buffer2D.pNext = &Out4_Buffer2D; // point to the next buffer in the chain

	
	Out4_Buffer2D.Data = (void*)sFrame3;// address of the data storage
	Out4_Buffer2D.ElementWidth = sizeof(u32);
	Out4_Buffer2D.XCount = (FRAME_DATA_LEN/2);
	Out4_Buffer2D.XModify = 4;
	Out4_Buffer2D.YCount = NUM_LINES;
	Out4_Buffer2D.YModify = 4;
#ifdef	USE_LOOPBACK	
	Out4_Buffer2D.CallbackParameter = NULL;
#else	// chained buffer
	Out4_Buffer2D.CallbackParameter = &Out3_Buffer2D;// generate callback, pArg = buffer address
#endif	
	Out4_Buffer2D.pNext = NULL; // terminate the chain of buffers
	
	// configure the ad7179 dataflow method
#if defined(USE_LOOPBACK)
	ezErrorCheck(adi_dev_Control(AD7179DriverHandle, ADI_DEV_CMD_SET_DATAFLOW_METHOD, (void *)ADI_DEV_MODE_CHAINED_LOOPBACK));
    	ezErrorCheck(adi_dev_Write(AD7179DriverHandle, ADI_DEV_2D, (ADI_DEV_BUFFER *)&Out1_Buffer2D));
#else	
	ezErrorCheck(adi_dev_Control(AD7179DriverHandle, ADI_DEV_CMD_SET_DATAFLOW_METHOD, (void *)ADI_DEV_MODE_CHAINED));
    	ezErrorCheck(adi_dev_Write(AD7179DriverHandle, ADI_DEV_2D, (ADI_DEV_BUFFER *)&Out1_Buffer2D));
    	ezErrorCheck(adi_dev_Write(AD7179DriverHandle, ADI_DEV_2D, (ADI_DEV_BUFFER *)&Out3_Buffer2D));

#endif
	// start outputting video data
	ezErrorCheck(adi_dev_Control(AD7179DriverHandle, ADI_DEV_CMD_SET_DATAFLOW, (void*)TRUE));

}
/**********************************************************************
* Configure AD7179 to NTSC mode 
**********************************************************************/
static void Set7179ToNTSC(void)	
{	
    // ADV7179 register configuration array for NTSC mode
    ADI_DEV_ACCESS_REGISTER ADV7179_Cfg[]={{ADV717x_MR0, 	0x00},		// register address, configuration data
    					   {ADV717x_MR1, 	0x58},
    					   {ADV717x_MR2, 	0x00},
    					   {ADV717x_MR3, 	0x00},
    					   {ADV717x_MR4, 	0x10},
    					   {ADV717x_TMR0, 	0x00}, 
    					   {ADV717x_TMR1, 	0x00},
    					   {ADI_DEV_REGEND,0	}};	// End of register access
  
	
	
							
    ezErrorCheck(adi_dev_Control(AD7179DriverHandle, ADI_ADV717x_CMD_SET_SCF_REG, (void *) ADV717x_SCF_VALUE_NTSC));
    
   // configure ADV7179 in selected mode
    ezErrorCheck(adi_dev_Control(AD7179DriverHandle, ADI_DEV_CMD_REGISTER_TABLE_WRITE, (void *)ADV7179_Cfg));


}

/**********************************************************************
* Configure AD7179 to PAL mode 
**********************************************************************/
static void Set7179ToPAL(void)	
{
	
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
    
    	// configure ADV7179 in selected mode
    ezErrorCheck(adi_dev_Control(AD7179DriverHandle, ADI_DEV_CMD_REGISTER_TABLE_WRITE, (void *)ADV7179_Cfg));
  

}

/**********************************************************************
* read ADV7183 status registers 
**********************************************************************/
static void Read7183StatusReg(void)	
{
    	u32 Result = 0, i;

    // array to hold the read AD7183 subaddress register value
    u16 Read_data[4] ={0};        

    
    ADI_DEV_ACCESS_REGISTER Regs[] = 
	{{ ADV7183_STATUS1_RO, 	0 },		// Register address to access, corresponding register data
         { ADV7183_IDENT_RO, 	0 },
         { ADV7183_STATUS2_RO, 	0 },
         { ADV7183_STATUS3_RO, 	0 },
         { ADI_DEV_REGEND,	0 }};	// Register access delimiter (indicates end of register access)

//To read list of registers in DevRegs
    Result =  adi_dev_Control(AD7183DriverHandle, ADI_DEV_CMD_REGISTER_TABLE_READ, (void *)&Regs);    
    if (Result != 0) printf("CMD_SELECTIVE_REGISTER_READ failed(error:%x)\n",Result);
    else {
    	// print the values	
		printf("AD7183: STATUS1 IDENT STATUS2 STATUS3\n ");
		for (i=0; i<4; i++){
		printf("0x%02X ",Regs[i].Data);
		}
		printf("\n");

    }

}


/**********************************************************************
* Read AD7179 mode(PAL or NTSC) 
**********************************************************************/
static void Read7179Regs(void)	
{							
	u32 i,AD7179SCFValue,Result;
	
   // ADV7179 register configuration array for NTSC mode
    ADI_DEV_ACCESS_REGISTER ADV7179_read[]={{ADV717x_MR0, 	0},		// register address, configuration data
    										{ADV717x_MR1, 	0},
    										{ADV717x_MR2, 	0},
    										{ADV717x_MR3, 	0},
    										{ADV717x_MR4, 	0},
    										{ADV717x_TMR0, 	0}, 
    										{ADV717x_TMR1, 	0},
    										{ADI_DEV_REGEND,0	}	};	// End of register access

 	Result = adi_dev_Control(AD7179DriverHandle, ADI_ADV717x_CMD_GET_SCF_REG, (void *)&AD7179SCFValue);
	if ( Result != ADI_DEV_RESULT_SUCCESS) printf("Get SCF-Reg failed\n");
	else 
	printf("\nAD7179 SCF=0x%x\n",AD7179SCFValue);
	
	
	    // Read ADV7179 in selected mode
    Result = adi_dev_Control(AD7179DriverHandle, ADI_DEV_CMD_REGISTER_TABLE_READ, (void *)ADV7179_read);       
	if ( Result != ADI_DEV_RESULT_SUCCESS) printf("Selective Read failed\n");
	else {
		printf("AD7179 Regs: ");
	
		for(i=0;i<7;i++){
		printf("0x%x ",ADV7179_read[i].Data);
		}
		printf("\n");
	}

}





