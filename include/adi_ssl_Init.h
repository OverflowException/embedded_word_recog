/*********************************************************************************

Copyright(c) 2005 Analog Devices, Inc. All Rights Reserved. 

This software is proprietary and confidential.  By using this software you agree
to the terms of the associated Analog Devices License Agreement.  
			
*********************************************************************************/


/*********************************************************************

Include files

*********************************************************************/
#include <services/services.h>		// system service includes
#include <drivers/adi_dev.h>        // device driver includes


/*********************************************************************

Warn user against using this file from the common area

*********************************************************************/
/*#warning ** Do not use adi_ssl_Init.h and adi_ssl_Init.c from the common area. \
Copy adi_ssl_Init.h and adi_ssl_Init.c to your local directory, edit adi_ssl_Init.h \
to set the sizings appropriately, then comment out this line from adi_ssl_Init.h **/

/*********************************************************************

Definitions/Sizings

The user should modify the values in parenthesis as needed by their
application.

*********************************************************************/

#define ADI_SSL_INT_NUM_SECONDARY_HANDLERS  (10) // number of secondary interrupt handlers
#define ADI_SSL_DCB_NUM_SERVERS             (1) // number of DCB servers
#define ADI_SSL_DMA_NUM_CHANNELS            (8) // number of DMA channels
#define ADI_SSL_FLAG_NUM_CALLBACKS          (4) // number of flag callbacks
#define ADI_SSL_DEV_NUM_DEVICES             (6) // number of device drivers
#define ADI_SSL_SEM_NUM_SEMAPHORES          (0) // number of semaphores


/*********************************************************************

Handles to Services

Declare the handles to the various services and device manager.  These
handles can be used after the adi_ssl_Init() call.  

DO NOT MODIFY THIS SECTION

*********************************************************************/

extern ADI_DMA_MANAGER_HANDLE adi_dma_ManagerHandle;   // handle to the DMA manager
extern ADI_DEV_MANAGER_HANDLE adi_dev_ManagerHandle;   // handle to the device manager


/*********************************************************************

Critical Region Parameter

This is the value that is pased to the adi_int_EnterCriticalRegion()
function call.  This value is NULL for all current implementations.

DO NOT MODIFY THIS SECTION

*********************************************************************/

#define ADI_SSL_ENTER_CRITICAL (NULL)




/*********************************************************************

Function Declarations

Declare the intialization and termination functions.

DO NOT MODIFY THIS SECTION

*********************************************************************/

#if defined __cplusplus
    extern "C" {
#endif


u32 adi_ssl_Init(void);
u32 adi_ssl_Terminate(void);


#if defined __cplusplus
    }
#endif



