/*********************************************************************************

Copyright(c) 2005 Analog Devices, Inc. All Rights Reserved.

This software is proprietary and confidential.  By using this software you agree
to the terms of the associated Analog Devices License Agreement.

$RCSfile: SDK-ezkitutilities.c,v $
$Revision: 1.8 $
$Date: 2010/11/02 08:44:44 $

Description:    EZ-Kit utility routines.  This file contains a collection
                of functions that automate typical EZ-Kit functionality,
                including control of LEDs and push buttons.

*********************************************************************************/


/*********************************************************************

Include files

*********************************************************************/

#include <services/services.h>      // system service includes
#include <sysreg.h>                 // system config definitions
#include "SDK-ezkitutilities.h"     // EZ-Kit utility definitions


/*********************************************************************

flash addresses for BF-533 EZ-Kit

*********************************************************************/

#if defined(__ADSP_EDINBURGH__)         // the Edinburgh EZ-Kit uses a flash for ports

#define pFlashA_PortA_In    ((volatile unsigned char *)0x20270000)  // address of flash A port A input data register
#define pFlashA_PortA_Out   ((volatile unsigned char *)0x20270004)  // address of flash A port A output data register
#define pFlashA_PortA_Dir   ((volatile unsigned char *)0x20270006)  // address of flash A port A direction register

#define pFlashA_PortB_In    ((volatile unsigned char *)0x20270001)  // address of flash A port B input data register
#define pFlashA_PortB_Out   ((volatile unsigned char *)0x20270005)  // address of flash A port B output data register
#define pFlashA_PortB_Dir   ((volatile unsigned char *)0x20270007)  // address of flash A port B direction register

#define pFlashB_PortA_In    ((volatile unsigned char *)0x202E0000)  // address of flash B port A input data register
#define pFlashB_PortA_Out   ((volatile unsigned char *)0x202E0004)  // address of flash B port A output data register
#define pFlashB_PortA_Dir   ((volatile unsigned char *)0x202E0006)  // address of flash B port A direction register

#define pFlashB_PortB_In    ((volatile unsigned char *)0x202E0001)  // address of flash B port B input data register
#define pFlashB_PortB_Out   ((volatile unsigned char *)0x202E0005)  // address of flash B port B output data register
#define pFlashB_PortB_Dir   ((volatile unsigned char *)0x202E0007)  // address of flash B port B direction register

#endif

//REMOVE WHEN DRIVERS ARE ADDED
/*********************************************************************

Miscellaneous defines

*********************************************************************/

#if defined(__ADSP_EDINBURGH__)         // ADSP-BF533 EZ-Kit specific info
#define RST_7183                (0x8)   // decoder reset bit #3 in flashA portA
#define PPICLK_ADV7183_SELECT   (0x10)  // decoder clock to PPI bit #4 in flashA portA
#define ADV7183_OE_MASK         (0x4)   // ADV7183 /OE = PF2
#define ADV7183_OE              (2)     //Index into Codec flag handler array
#define RST_7171                (0x4)   // encoder reset

#endif

#if defined(__ADSP_TETON__)
#define ADV7183_RESET       (13)        //decoder reset bit
#define ADV7183_OE           (2)        //ADV7183 /OE = PF2
#define ADV7183_Reset_Delay 0x04000000  // For delay after reset
#define ADV7183_Lock        0x0000ffff  // For startup locktime delay
#define ADV7183_RST         (3)
#define ADV7183_CLK_SELECT  (4)
#define ADV7183_RESET_DELAY (4200000)
#define ADV7183_LOCK        0x0001ffff  // For startup locktime delay

#define ADV7179_RESET_DELAY (4200000)
#define ADV7179_RESET_BIT   (14)        // encoder reset bit
#define ADV7179_RESET       (14)        // encoder reset bit

#endif



/*********************************************************************

LED/Button to pf mappings

*********************************************************************/
#if defined(__ADSP_EDINBURGH__) // ADSP-BF533 EZ-Kit mappings
ADI_FLAG_ID ezButtonToFlag[] = {
    ADI_FLAG_PF8,           // button 0
    ADI_FLAG_PF9,           // button 1
    ADI_FLAG_PF10,          // button 2
    ADI_FLAG_PF11           // button 3
};
ADI_FLAG_ID ezLEDToFlag[] = {  // (not used on Edinburgh but needed as placeholder)
    ADI_FLAG_PF0,           // led 0
    ADI_FLAG_PF0,           // led 1
    ADI_FLAG_PF0,           // led 2
    ADI_FLAG_PF0,           // led 3
    ADI_FLAG_PF0,           // led 4
    ADI_FLAG_PF0            // led 5
};
#endif

/*********************************************************************/

#if defined(__ADSP_STIRLING__) // ADSP-BF538 EZ-Kit mappings
ADI_FLAG_ID ezButtonToFlag[] = {
    ADI_FLAG_PF0,           // button 0
    ADI_FLAG_PF1,           // button 1
    ADI_FLAG_PF2,           // button 2
    ADI_FLAG_PF3            // button 3
};

ADI_FLAG_ID ezLEDToFlag[] = {  
    ADI_FLAG_PC4,           // led 0
    ADI_FLAG_PC5,           // led 1
    ADI_FLAG_PC6,           // led 2
    ADI_FLAG_PC7,           // led 3
    ADI_FLAG_PC8,           // led 4
    ADI_FLAG_PC9            // led 5
};
#endif



#if defined(__ADSP_BRAEMAR__)   // ADSP-BF537 EZ-Kit mappings
ADI_FLAG_ID ezButtonToFlag[] = {
    ADI_FLAG_PF2,           // button 0
    ADI_FLAG_PF3,           // button 1
    ADI_FLAG_PF4,           // button 2
    ADI_FLAG_PF5            // button 3
};
ADI_FLAG_ID ezLEDToFlag[] = {
    ADI_FLAG_PF6,           // led 0
    ADI_FLAG_PF7,           // led 1
    ADI_FLAG_PF8,           // led 2
    ADI_FLAG_PF9,           // led 3
    ADI_FLAG_PF10,          // led 4
    ADI_FLAG_PF11           // led 5
};
#endif

#if defined(__ADSP_TETON__)     // ADSP-BF561 EZ-Kit mappings
ADI_FLAG_ID ezButtonToFlag[] = {
    ADI_FLAG_PF5,           // button 0
    ADI_FLAG_PF6,           // button 1
    ADI_FLAG_PF7,           // button 2
    ADI_FLAG_PF8            // button 3
};
ADI_FLAG_ID ezLEDToFlag[] = {
    ADI_FLAG_PF40,          // led 0
    ADI_FLAG_PF41,          // led 1
    ADI_FLAG_PF42,          // led 2
    ADI_FLAG_PF43,          // led 3
    ADI_FLAG_PF44,          // led 4
    ADI_FLAG_PF45,          // led 5
    ADI_FLAG_PF46,          // led 6
    ADI_FLAG_PF47,          // led 7
    ADI_FLAG_PF32,          // led 8
    ADI_FLAG_PF33,          // led 9
    ADI_FLAG_PF34,          // led 10
    ADI_FLAG_PF35,          // led 11
    ADI_FLAG_PF36,          // led 12
    ADI_FLAG_PF37,          // led 13
    ADI_FLAG_PF38,          // led 14
    ADI_FLAG_PF39           // led 15
};

#endif

#if defined(__ADSP_MOAB__) // ADSP-BF54x EZ-Kit mappings
ADI_FLAG_ID ezButtonToFlag[] = {
    ADI_FLAG_PB8,           // button 1
    ADI_FLAG_PB9,           // button 2
    ADI_FLAG_PB10,          // button 3
    ADI_FLAG_PB11           // button 4
};

ADI_FLAG_ID ezLEDToFlag[] = {  
    ADI_FLAG_PG6,           // led 1
    ADI_FLAG_PG7,           // led 2
    ADI_FLAG_PG8,           // led 3
    ADI_FLAG_PG9,           // led 4
    ADI_FLAG_PG10,          // led 5
    ADI_FLAG_PG11           // led 6
};
#endif

#if defined(__ADSP_KOOKABURRA__) // ADSP-BF527 EZ-Kit mappings
ADI_FLAG_ID ezButtonToFlag[] = {
    ADI_FLAG_PG13,          // button 1
    ADI_FLAG_PG0,           // button 2
};

ADI_FLAG_ID ezLEDToFlag[] = {  
    ADI_FLAG_PF8,           // led 1
    ADI_FLAG_PG11,          // led 2
    ADI_FLAG_PG12,          // led 3
};
#endif

/*********************************************************************

LED control

*********************************************************************/
static u32 LEDDisplay;      // bit field representing the LED display
static u32 LEDEnables;      // bit field representing the enabled LEDs



/*********************************************************************

function prototypes

*********************************************************************/


/*********************************************************************

    Function:       ezInit

    Description:   Initializes the EZ-Kit - configures flash (where applicable)

*********************************************************************/
void ezInit(u32 NumCores)
{


    // configure flash
#if defined(__ADSP_EDINBURGH__)         // ADSP-BF533 EZ-Kit specific info
    *pFlashA_PortA_Out = 0;         // resets port A to initial value
    *pFlashA_PortA_Dir = 0xFF;      // configure everything on port A as outputs
    *pFlashA_PortB_Out = 0;         // resets port B to initial value
    *pFlashA_PortB_Dir = 0x3f;      // configure everything on port B as outputs
#endif


    // no longer initializing power or EBIU services or Asynch memory

}




/*********************************************************************

    Function:       ezInitLEDs

    Description:    Enables an LED for use

*********************************************************************/

void ezInitLED(u32 LED)    // enables an LED
{

    // make sure the LED is valid
    if (LED >= EZ_NUM_LEDS) return;

    // set the enable bit
    LEDEnables |= (1 << LED);

    // configure the flag for output
#if defined(__ADSP_BRAEMAR__) 	||\
	defined (__ADSP_TETON__) 	||\
	defined (__ADSP_STIRLING__) ||\
	defined (__ADSP_MOAB__)		||\
	defined(__ADSP_KOOKABURRA__)
    adi_flag_Open(ezLEDToFlag[LED]);
    adi_flag_SetDirection(ezLEDToFlag[LED], ADI_FLAG_DIRECTION_OUTPUT);
#endif

    // dim the LED
    ezTurnOffLED(LED);
}


/*********************************************************************

	Function:		ezInitLEDFlags

	Description:	Configures the flags to drive LEDs

*********************************************************************/

void ezInitLEDFlags(void)
{
#ifdef __ADSPBF561__
  *((short*)FIO2_DIR)    = 0xFFFF;       
  ssync();
  *((short*)FIO2_FLAG_C) = 0xFFFF;     
  ssync();
  *((short*)FIO2_FLAG_S) = 0x0;
  ssync();
#endif
}

/*********************************************************************

    Function:       ezTurnOnLED

    Description:    Lights an LED

*********************************************************************/

void ezTurnOnLED(u32 LED)   // lights an LED
{

    // update
    ezSetDisplay(LEDDisplay | (1 << LED));

}

/*********************************************************************

    Function:       ezTurnOffLED

    Description:    Dims an LED

*********************************************************************/

void ezTurnOffLED(u32 LED)  // dims an LED
{

    // update
    ezSetDisplay(LEDDisplay & ~(1 << LED));

}


/*********************************************************************

    Function:       ezToggleLED

    Description:    Toggles an LED

*********************************************************************/

void ezToggleLED(u32 LED)   // toggles an LED
{

    // update
    ezSetDisplay(LEDDisplay ^ (1 << LED));

}
/*********************************************************************

	Function:		blink

	Description:	Blinks a LED

*********************************************************************/
void delay(int i)
{
	while(i--)
		asm("nop;nop;nop;nop;nop;");
}

void blink(int n, int led)
{
	int i;
	for (i=0;i<n;i++)
	{	
	  ezToggleLED(led);
	  delay(100000);	
	}
}


/*********************************************************************

    Function:       ezTurnOnAllLEDs

    Description:    Lights all LEDs

*********************************************************************/

void ezTurnOnAllLEDs(void)  // lights all LEDs
{

    // update
    ezSetDisplay(0xffff);

}


/*********************************************************************

    Function:       ezTurnOffAllLEDs

    Description:    Dims all LEDs

*********************************************************************/

void ezTurnOffAllLEDs(void) // dims all LEDs
{

    // update
    ezSetDisplay(0);

}


/*********************************************************************

    Function:       ezToggleAllLEDs

    Description:    Toggles all LEDs

*********************************************************************/

void ezToggleAllLEDs(void)  // toggles all LEDs
{

    // update
    ezSetDisplay(~LEDDisplay);

}


/*********************************************************************

    Function:       ezIsLEDon

    Description:    Senses if an LED is lit

*********************************************************************/

u32 ezIsLEDOn(u32 LED)  // senses if an LED is lit
{

    // return
    return (LEDDisplay & (1 << LED)?TRUE:FALSE);

}


/*********************************************************************

    Function:       ezCycleLEDs

    Description:    Cycles LEDs

*********************************************************************/

void ezCycleLEDs(void)  // cycles all LEDs
{
    static u32 CycleDisplay;

    // insure at least 1 LED is enabled
    if (LEDEnables == 0) return;

    // calculate the pattern
    do {
        CycleDisplay <<= 1;
        if (CycleDisplay == 0) CycleDisplay = 1;
    } while ((CycleDisplay & LEDEnables) == 0);

    // update
    ezSetDisplay(CycleDisplay);

}


/*********************************************************************

    Function:       ezGetDisplay

    Description:    Gets the display pattern

*********************************************************************/

u32 ezGetDisplay(void)  // gets the pattern
{

    // get it
    return (LEDDisplay);

}


/*********************************************************************

    Function:       ezSetDisplay

    Description:    Sets the display pattern

*********************************************************************/

void ezSetDisplay(u32 Display)  // sets the display
{

    u32 i;
    u32 Mask;

    // update the display
    LEDDisplay = Display & LEDEnables;

    // FOR (each LED)
    for (i = 0, Mask = 1; i < EZ_NUM_LEDS; i++, Mask <<= 1) {

        // IF (the LED should be lit)
        if (LEDDisplay & Mask) {

            // light it
#if defined(__ADSP_EDINBURGH__)
            *pFlashA_PortB_Out |= Mask;
#endif
#if defined(__ADSP_BRAEMAR__) 	||\
	defined (__ADSP_TETON__) 	||\
	defined (__ADSP_STIRLING__) ||\
	defined (__ADSP_MOAB__)		||\
	defined(__ADSP_KOOKABURRA__)
            adi_flag_Set(ezLEDToFlag[i]);
#endif

        // ELSE
        } else if (LEDEnables & Mask) {

            // dim it
#if defined(__ADSP_EDINBURGH__)
            *pFlashA_PortB_Out &= ~Mask;
#endif
#if defined(__ADSP_BRAEMAR__) 	||\
	defined (__ADSP_TETON__) 	||\
	defined (__ADSP_STIRLING__) ||\
	defined (__ADSP_MOAB__)		||\
	defined(__ADSP_KOOKABURRA__)
            adi_flag_Clear(ezLEDToFlag[i]);
#endif

        // ENDIF
        }

    // ENDFOR
    }

}



/*********************************************************************

    Function:       ezInitButton

    Description:    Enables a button for use

*********************************************************************/

void ezInitButton(u32 Button) // enables a button
{

    // make sure the button is valid
    if (Button >= EZ_NUM_BUTTONS) return;

    // open the flag and make it an input
    adi_flag_Open(ezButtonToFlag[Button]);
    adi_flag_SetDirection(ezButtonToFlag[Button], ADI_FLAG_DIRECTION_INPUT);

}



/*********************************************************************

    Function:       ezIsButtonPushed

    Description:    Returns TRUE if a button has been pushed, FALSE otherwise.

*********************************************************************/

u32 ezIsButtonPushed(u32 Button)        // returns TRUE if button is pushed, FALSE otherwise
{
    u32 Value;

    // make sure the button is valid
    if (Button >= EZ_NUM_BUTTONS) return (FALSE);

    // sense it
    adi_flag_Sense(ezButtonToFlag[Button], &Value);

    // return
    return (Value);

}


/*********************************************************************

    Function:       ezClearButton

    Description:    Clears a push button latch.  This must be called to
                    reset the latch for the push button, if a button has
                    been pressed.

*********************************************************************/

void ezClearButton(u32 Button)  // clears a button latch
{
    volatile int i;

    // make sure the button is valid
    if (Button >= EZ_NUM_BUTTONS) return;

    // delay to allow for any debounce to clear
    ezDelay(10);

    // clear the flag corresponding to the button
    adi_flag_Clear(ezButtonToFlag[Button]);

}



/*********************************************************************

    Function:       ezDelay

    Description:    Delays for approximately 1 msec when running at 600 MHz

*********************************************************************/

void ezDelay(u32 msec) {

    volatile u32 i,j;

    // value of 0x3000000 is about 1 sec so 0xc49b is about 1msec
    for (j = 0; j < msec; j++) {
        for (i = 0; i < 0xc49b; i++) ;
    }

}



/*********************************************************************

    Function:       ezErrorCheck

    Description:    This function is intended to be used as a means to
                    quickly determine if a function has returned a non-zero
                    (hence an error) return code.  All driver and system
                    services functions return a value of zero for success and
                    a non-zero value when a failure occurs.  This function
                    makes all LEDs glow dimly when a non-zero value is
                    passed to it.

*********************************************************************/

void ezErrorCheck(u32 Result)               // lights all LEDS and spins on nonzero value
{
	if(Result != 0)
		while(1)
			ezTurnOnAllLEDs();
    /*while (Result != 0) {
        ezCycleLEDs();
    }*/
}


/*********************************************************************

    Function:       ezResetad1836

    Description:    This function is intended to be used to reset the AD1836 audio codec

*********************************************************************/
void ezReset1836(void)
{
  int             i;
  static unsigned char data;

#if defined (__ADSP_EDINBURGH__)
  *pFlashA_PortA_Dir = data = 0x01;

#if defined (__ADSP_EDINBURGH_AVEXTNDR__) || defined (__ADSPBF533_AVEXTNDR__)
  *pFlashA_PortA_Dir = data = 0x40;
#endif

  *pFlashA_PortA_Out = 0x00; //write to Port A to reset AD1836
  asm("nop; nop; nop;"); //wait at least 5 ns in reset
  *pFlashA_PortA_Out = data; //write to Port A to enable AD1836
#endif //__ADSP_EDINBURGH__

#if defined (__ADSP_BRAEMAR__)

#elif defined (__ADSP_BRAEMAR___AVEXTNDR__) || defined (__ADSPBF537_AVEXTNDR__)
  SlaveSelect = 6; //SPI Slave Select connected to 1836
  adi_flag_Open(ADI_FLAG_PF6);
  adi_flag_SetDirection(ADI_FLAG_PF6, ADI_FLAG_DIRECTION_OUTPUT);
  adi_flag_Clear(ADI_FLAG_PF6); //Clear PF6 to reset the AD1836
  asm("nop; nop; nop;"); //assert reset for at least 5 ns
  adi_flag_Set(ADI_FLAG_PF6); //Set PF6 to reset the AD1836
#endif //__ADSP_BRAEMAR__

#if defined (__ADSP_TETON__)
  adi_flag_Open(ADI_FLAG_PF15);
  adi_flag_SetDirection(ADI_FLAG_PF15, ADI_FLAG_DIRECTION_OUTPUT);
  adi_flag_Clear(ADI_FLAG_PF15); //Clear PF6 to reset the AD1836
  asm("nop; nop; nop;"); //Assert reset for at least 5 ns
  adi_flag_Set(ADI_FLAG_PF15); //Set PF6 to reset the AD1836
#endif //__ADSP_TETON__

  //Now wait 4500 MCLK periods (MCLK = 12.288 MHz) to recover from reset
  for (i=0; i<220000; i++) { asm("nop;"); } //Prevents compiler from optimizing
}


/*********************************************************************

    Function:       ezEnableVideoDecoder

    Description:    Connects up the bus, sets up the video clocking and
                    enables the AD7183.

*********************************************************************/

void ezEnableVideoDecoder(void)         // connects bus, sets up video clock and enables the 7183
{

#if defined(__ADSP_EDINBURGH__)         // ADSP-BF533 EZ-Kit specific info
#define ADV7183_OE_FLAG ADI_FLAG_PF2

    unsigned short tempReg;

    //The GPIOs of the flash are linked to the ADV7183 reset pin as well as to the
    //switch that drives the PPI clock. De-assert the ADV7183 reset and link the ADV7183
    //Clock "LLC1" to the Blackfin PPI clock input.  See EZ-KIT schematics for more details.
    tempReg = *pFlashA_PortA_Out;
	tempReg  &= ~RST_7171;	// disable 7171
    *pFlashA_PortA_Out = tempReg | RST_7183 | PPICLK_ADV7183_SELECT;

    //Setup the Blackfin PF registers
#if defined(__ADSP_EDINBURGH__) && (__SILICON_REVISION__==0xffff || __SILICON_REVISION__==0x1)
        tempReg = *pFIO_INEN;
        *pFIO_INEN = tempReg | ADV7183_OE_MASK;
#endif

    // open PF2 and PF13
    adi_flag_Open(ADI_FLAG_PF2);
    adi_flag_Open(ADI_FLAG_PF13);

    //Blackfin PF2 pin must be set as output
    adi_flag_SetDirection(ADI_FLAG_PF2, ADI_FLAG_DIRECTION_OUTPUT);

    //Set the Blackfin pin PF2 to output enable the ADV7183 data bus
    adi_flag_Clear(ADI_FLAG_PF2);

#endif // Edinburgh


#if defined(__ADSP_TETON__)

    volatile long delay_time;

    //Blackfin PF2 pin must be set as output
    adi_flag_SetDirection(ADI_FLAG_PF2, ADI_FLAG_DIRECTION_OUTPUT);

    //Blackfin PF13 pin must be set as output
    adi_flag_SetDirection(ADI_FLAG_PF13, ADI_FLAG_DIRECTION_OUTPUT);
    ssync();

    // set bit PF2 to disable ADV7183 outputs
    adi_flag_Set(ADI_FLAG_PF2);
    ssync();

    // clear bit PF13 to reset ADV7183
    adi_flag_Clear(ADI_FLAG_PF13);
    ssync();

    // set bit PF13 to re-enable ADV7183
    adi_flag_Set(ADI_FLAG_PF13);
    ssync();

    for (delay_time = 0; delay_time < ADV7183_Reset_Delay; delay_time++)
        asm("nop;");

    // clear bit to enable ADV7183 outputs
    adi_flag_Clear(ADI_FLAG_PF2);
    ssync();

    for (delay_time = 0; delay_time < ADV7183_Lock; delay_time++)
        asm("nop;");

#endif   // Teton

}

/*********************************************************************

    Function:       ezEnableVideoEncoder

    Description:    Enables the AD7171 or AD7179

*********************************************************************/

void ezEnableVideoEncoder(void)     // enables the 7171
{

#if defined(__ADSP_EDINBURGH__)         // ADSP-BF533 EZ-Kit specific info

	unsigned short tempReg;
	
	tempReg = *pFlashA_PortA_Out;
	tempReg  &= ~(RST_7183 | PPICLK_ADV7183_SELECT);	// disable 7183
	*pFlashA_PortA_Out = tempReg | RST_7171;

#endif // Edinburgh


#if defined(__ADSP_TETON__)

    // open PF14
    adi_flag_Open(ADI_FLAG_PF14);

    //Blackfin PF14 pin must be set as output
    adi_flag_SetDirection(ADI_FLAG_PF14, ADI_FLAG_DIRECTION_OUTPUT);
    ssync();

    // clear bit to reset ADV7179, Blackfin pin PF14
    adi_flag_Clear(ADI_FLAG_PF14);
    ssync();

    adi_flag_Set(ADI_FLAG_PF14);
    ssync();


#endif   // Teton

}


/*********************************************************************

    Function:   ezDisableVideoEncoder

    Description:    Disables the AD7179.

*********************************************************************/

void ezDisableVideoEncoder(void)
{

#if defined(__ADSP_EDINBURGH__)			// ADSP-BF533 EZ-Kit specific info

	unsigned short tempReg;
	
	tempReg = *pFlashA_PortA_Out;
	tempReg  &= ~(RST_7171);	// disable 7171
	*pFlashA_PortA_Out = tempReg;
	
#endif // Edinburgh
    
    
#if defined(__ADSP_TETON__)
    adi_flag_Clear(ADI_FLAG_PF14);

    // close PF14
    adi_flag_Close(ADI_FLAG_PF14);

    ssync();
#endif
}


/*********************************************************************

    Function:       ezDisableVideoDecoder

    Description:     Disables 7183
*********************************************************************/

void ezDisableVideoDecoder(void)
{

#if defined(__ADSP_EDINBURGH__)			// ADSP-BF533 EZ-Kit specific info

	// open PF2
	adi_flag_Open(ADI_FLAG_PF2);	
	//Blackfin PF2 pin must be set as output (PF2 is ADV7183 Output Enable)
	adi_flag_SetDirection(ADI_FLAG_PF2, ADI_FLAG_DIRECTION_OUTPUT);	
	//Set the Blackfin pin PF2 to disable ADV7183
	adi_flag_Set(ADI_FLAG_PF2);
	
#endif // Edinburgh

#if defined(__ADSP_TETON__)

    // set bit PF2 to disable ADV7183 outputs
    adi_flag_Set(ADI_FLAG_PF2);
    ssync();

    // clear bit PF13 to reset ADV7183
    adi_flag_Clear(ADI_FLAG_PF13);
    ssync();
#endif

    // close PF2 and PF13
    adi_flag_Close(ADI_FLAG_PF2);
    adi_flag_Close(ADI_FLAG_PF13);

}

/*********************************************************************

   Function:      ezEnableVideoDecoder

   Description:   Enables the AD7183A.  

*********************************************************************/

void enableVideoDecoder(void)      // enables the 7183A
{
    int reg_val;
    long delay_time;
    int i;

   
#ifdef __ADSPBF561__

   reg_val = ((1 << ADV7183_OE ) | (1 << ADV7183_RESET));
   *pFIO0_DIR |= reg_val;
   ssync();

   *pFIO0_FLAG_S = (1 << ADV7183_OE);        
   ssync();

   *pFIO0_FLAG_C = (1 << ADV7183_RESET);   
   ssync();
   *pFIO0_FLAG_S = (1 << ADV7183_RESET);   
   ssync();

   for(i=0; i < ADV7183_RESET_DELAY; i++)
        asm("nop;");

   *pFIO0_FLAG_C = (1 << ADV7183_OE);        
   ssync();

   for(i=0; i < ADV7183_LOCK; i++)
        asm("nop;");

#endif   
}

/*********************************************************************

   Function:      ezEnableVideoEncoder

   Description:   Enables the AD7179.  

*********************************************************************/

void enableVideoEncoder(void)
{
	int i;

#ifdef __ADSPBF561__
	// configure RESET flag as output
	*pFIO0_DIR |=   (1 << ADV7179_RESET_BIT);

	ssync();

	// generate reset pulse
	*pFIO0_FLAG_C = (1 << ADV7179_RESET_BIT);	// clear bit to reset ADV7179
	ssync();

	for(i=0; i< ADV7179_RESET_DELAY; i++)
		asm("nop;");

	*pFIO0_FLAG_S = (1 << ADV7179_RESET_BIT);	// set bit to re-enable ADV7179
	ssync();
    
	for(i=0; i< ADV7179_RESET_DELAY; i++)
		asm("nop;");
#endif
}




