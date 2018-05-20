/******************************************************************************

Copyright (c) 2005 Analog Devices.  All Rights Reserved.

This software is proprietary and confidential.  By using this software you agree
to the terms of the associated Analog Devices License Agreement.  
*******************************************************************************

$File: adi_twi_pseudo.c $
$Revision: 4 $
$Date: 2009-07-28 11:25:38 -0400 (Tue, 28 Jul 2009) $

Project:    TWI device driver
Title:      Pseudo TWI source
Author(s):  BJ

Description:
   This is the pseudo TWI source file for the TWI driver

Note:
	includes adi_twi.c file and define ADI_TWI_PSEUDO so that adi_twi.c 
	is complied with pseudo TWI support. ADI_TWI_PSEUDO_ARBITRATION is
	used if support for multi-master is needed.
	
*******************************************************************************

Modification History:
====================
Revision 1.00 - 18/7/2005 - BJ
	- Created driver
Revision 1.04 - 16/1/2006 - BJ
	- Updated source

******************************************************************************/


#define ADI_TWI_PSEUDO_ARBITRATION

#include "adi_twi.c"


/********************************************************************************/

