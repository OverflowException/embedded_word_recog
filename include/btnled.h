#ifndef _BTNLED_H
#define _BTNLED_H

#include <sys\exception.h>
#include "deftypes.h"
 
/*   
PF5 button 0
PF6 button 1
PF7 button 2
PF8 button 3

PF32 led 0
PF33 led 1
PF34 led 2
PF35 led 3
PF36 led 4
PF37 led 5
PF38 led 6
PF39 led 7
PF40 led 8
PF41 led 9
PF42 led 10
PF43 led 11
PF44 led 12
PF45 led 13
PF46 led 14
PF47 led 15

FIO0 15-0
FIO1 31-16
FIO2 47-32
*/

#define BTN0_PF	5   
#define BTN1_PF	6
#define BTN2_PF	7
#define BTN3_PF	8

void initButtons(void);
void initButtonsInt(void);
void initLEDs(void);  
void turnOnLED(u32_t led);
void turnOffLED(u32_t led);
void toggleLED(u32_t led);
void setLEDDisplay(u16_t pattern);
u16_t getLEDDisplay(void);
bool getLEDStatus(u32_t led);

EX_INTERRUPT_HANDLER(ButtonsISR);


#endif
