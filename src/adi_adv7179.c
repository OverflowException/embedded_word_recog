/*********************************************************************************

Copyright (c) 2005 Analog Devices.  All Rights Reserved.

This software is proprietary and confidential.  By using this software you agree
to the terms of the associated Analog Devices License Agreement.  
******************************************************************************

$File: adi_adv7179.h $
$Revision: 4 $
$Date: 2009-07-28 11:25:38 -0400 (Tue, 28 Jul 2009) $

Project:    ADV7179 video encoder device driver
Title:      ADV7179 driver source code
Author(s):  Bala
Revised by:

Description:
            This is the driver source code for the ADV7179 Video Encoder. It is layered
            on top of the PPI and TWI device drivers, which are configured for
            the specific use ADV7179 peripheral.

References:
          
**********************************************************************************

ADV7179 device macro define

*********************************************************************************/

#define ADI_ADV7179_DEVICE
#include "adi_adv717x.c"      	// driver register access includes

