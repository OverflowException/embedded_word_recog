/*********************************************************************************

Copyright(c) 2005 Analog Devices, Inc. All Rights Reserved. 

This software is proprietary and confidential.  By using this software you agree
to the terms of the associated Analog Devices License Agreement.  

$RCSfile: adi_device_access_twi.c,v $
$Revision: 4 $
$Date: 2009-07-28 11:25:38 -0400 (Tue, 28 Jul 2009) $

Project:    Device Access Service (TWI/SPI)
Title:      TWI driven device register access source file
Author(s):  Bala

Revised by:

Description:
            This is the source code for the device drivers using TWI to access
            its internal registers. This file creates a macro that states that 
            we are building the TWI driven version of the device driver.  
            It then includes the main source code file to access the device registers
            
Note:
    See also  adi_device_access_spi.c for the SPI driven version of the driver.  
            
**********************************************************************************

TWI driven device register access macro define

*********************************************************************************/

#define ADI_DEVICE_ACCESS_TWI
#include "adi_device_access.c"      	// driver register access includes

