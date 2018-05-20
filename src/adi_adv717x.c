/**********************************************************************

Copyright(c) Analog Devices, Inc. All Rights Reserved. 

This software is proprietary and confidential.  By using this software 
you agree to the terms of the associated Analog Devices License Agreement.  

$File: adi_adv717x.c $
$Revision: 4 $
$Date: 2009-07-28 11:25:38 -0400 (Tue, 28 Jul 2009) $

Description:
            This is the driver source code for the ADV717x Video Encoder. 
            The driver is layered on top of the PPI/EPPI and TWI drivers, 
            which are configured for the specific use of selected 
            ADV717x peripheral.
            
Note:       This driver supports BF533,BF537,BF561,BF54x and 
            can be used to control ADV7170,ADV7171,ADV7174,ADV7179
   
***********************************************************************/

/*********************************************************************

Include files

*********************************************************************/

#include <drivers/adi_dev.h>                        /* device manager includes  */
#include <services/services.h>                      /* system services          */
#include <drivers/deviceaccess/adi_device_access.h> /* Device Access includes   */
#include <drivers/encoder/adi_adv717x.h>            /* ADV717x driver includes  */
#if defined (__ADSP_EDINBURGH__)    ||\
    defined (__ADSP_BRAEMAR__)      ||\
    defined (__ADSP_STIRLING__)     ||\
    defined (__ADSP_TETON__)        ||\
    defined (__ADSP_KOOKABURRA__)   ||\
    defined (__ADSP_MOCKINGBIRD__)
#include <drivers/ppi/adi_ppi.h>                    /* PPI driver includes      */
#elif defined (__ADSP_MOAB__)
#include <drivers/eppi/adi_eppi.h>                  /* EPPI driver includes     */
#else
#error "*** ADV717x Driver do not yet support this processor ***"
#endif

/*********************************************************************

Enumerations and defines

*********************************************************************/

/*********************************************************************

Check to insure that one of the ADV7170/ADV7171/ADV7174/ADV7179 
video encoders are built but not the ADV717x driver by itself.

*********************************************************************/

#ifndef ADI_ADV7170_DEVICE
#ifndef ADI_ADV7171_DEVICE
#ifndef ADI_ADV7174_DEVICE
#ifndef ADI_ADV7179_DEVICE
#error "*** ERROR: Must build either adi_adv7170.c or adi_adv7171.c or adi_adv7174.c or adi_adv7179.c, not adi_adv717x.c ***"
#endif
#endif
#endif
#endif

/* number of ADV717x devices instances */
#define ADI_ADV717x_NUM_DEVICES (sizeof(Device)/sizeof(ADI_ADV717x)) 

#if defined (ADI_ADV7171_DEVICE)    || defined (ADI_ADV7179_DEVICE)
#define ADV717x_FINAL_REG_ADDRESS     0x19    /* Address of the last register in ADV7171/ADV7179    */
#elif defined(ADI_ADV7170_DEVICE)   || defined (ADI_ADV7174_DEVICE)
#define ADV717x_FINAL_REG_ADDRESS     0x2F    /* Address of the last register in ADV7170/ADV7174    */
#endif

typedef struct ADV717x_SEMAPHORES {
#if defined (__ADSP_EDINBURGH__)    ||\
    defined (__ADSP_BRAEMAR__)      ||\
    defined (__ADSP_STIRLING__)     ||\
    defined (__ADSP_TETON__)        ||\
    defined (__ADSP_KOOKABURRA__)   ||\
    defined (__ADSP_MOCKINGBIRD__)
    u8  TripleFrameSync;    /* Flag to indicate whether Triple Frame Sync for PPI is set or not */
    u8  FrameSyncCount;     /* Frame Sync Count */
#endif
    u8  Adv717xMode;        /* indicates ADV717x in non-standard (1) or standard (0) mode */
    u8  NtscPalFlag;        /* Flag to indicate ADV717x in NTSC(0) or PAL(1) mode (only in standard video mode) */
    u8  DataflowFlag;       /* Video dataflow on(1) or off(0) */
    u8  InUseFlag;          /* Flag to indicate whether an instance is in use or not */
} ADV717x_SEMAPHORES;

// Structure to contain data for an instance of the ADV717x device driver
typedef struct {
    ADI_DEV_MANAGER_HANDLE      ManagerHandle;      /* Manager Handle */  
    ADI_DEV_DEVICE_HANDLE       DMHandle;           /* Handle of Device Manager instance */
    ADI_DMA_MANAGER_HANDLE      DMAHandle;          /* handle to the DMA manager */
    ADI_DCB_HANDLE              DCBHandle;          /* callback handle */
    ADI_DCB_CALLBACK_FN         DMCallback;         /* the callback function supplied by the Device Manager */
    ADI_DEV_PDD_HANDLE          PpiHandle;          /* Handle to the underlying PPI/EPPI device driver */
#if defined (__ADSP_EDINBURGH__)    ||\
    defined (__ADSP_BRAEMAR__)      ||\
    defined (__ADSP_STIRLING__)     ||\
    defined (__ADSP_TETON__)        ||\
    defined (__ADSP_KOOKABURRA__)   ||\
    defined (__ADSP_MOCKINGBIRD__)
    ADI_PPI_FS_TMR              *PPI_FS1_TMR;       /* PPI Frame Sync 1 timer configuration (only for non-standard video mode) */
    ADI_PPI_FS_TMR              *PPI_FS2_TMR;       /* PPI Frame Sync 2 timer configuration (only for non-standard video mode) */
#endif
    ADI_DEV_CMD_VALUE_PAIR      *TwiConfigTable;    /* TWI Configuration Table specific to the application */
    u32                         PpiDeviceNumber;    /* PPI/EPPI Device number used for ADV717x video dataflow */
#if defined (__ADSP_EDINBURGH__)    ||\
    defined (__ADSP_BRAEMAR__)      ||\
    defined (__ADSP_STIRLING__)     ||\
    defined (__ADSP_TETON__)        ||\
    defined (__ADSP_KOOKABURRA__)   ||\
    defined (__ADSP_MOCKINGBIRD__)
    u16                         PpiControl;         /* PPI Control register value */
#elif defined (__ADSP_MOAB__)
    u32                         PpiControl;         /* EPPI Control register value */
    u32                         PpiSamplesPerLine;  /* EPPI - Samples per line */
#endif
    u32                         PpiLinesPerFrame;   /* PPI/EPPI - Number of lines per frame */
    u32                         TwiDeviceNumber;    /* TWI Device number used for ADV717x video dataflow */
    u32                         TwiDeviceAddress;   /* TWI address of the ADV717x device */
    ADV717x_SEMAPHORES          Semaphores;         /* ADV717x locally used semaphores */
} ADI_ADV717x;

// The initial values for the device instance
static ADI_ADV717x Device[] = {
    {
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
#if defined (__ADSP_EDINBURGH__)    ||\
    defined (__ADSP_BRAEMAR__)      ||\
    defined (__ADSP_STIRLING__)     ||\
    defined (__ADSP_TETON__)        ||\
    defined (__ADSP_KOOKABURRA__)   ||\
    defined (__ADSP_MOCKINGBIRD__)
        NULL,
        NULL,
#endif
        NULL,
        0,      /* PPI 0 / EPPI 0 used by default */
#if defined (__ADSP_TETON__)    /* BF561 */
        0x0186, /* by default, PPI is configured as output with ITU 656 standard data & DMA32 enabled */
#elif defined (__ADSP_EDINBURGH__)    ||\
      defined (__ADSP_BRAEMAR__)      ||\
      defined (__ADSP_STIRLING__)     ||\
      defined (__ADSP_KOOKABURRA__)   ||\
      defined (__ADSP_MOCKINGBIRD__)  
        0x0086, /* by default, PPI is configured as output with ITU 656 standard data */
#elif defined (__ADSP_MOAB__)   /* BF54x */
        0x6810100E, /* EPPI DMA unpacking enabled, IFSGEN, ICLKGEN, BLANKGEN disabled, ITU Interlaced out */
        ADI_ADV71x_SAMPLES_PER_LINE_NTSC,
#endif
        ADI_ADV71x_LINES_PER_FRAME_NTSC,    /* NTSC as default  */
        0,      /* TWI 0 as default */
#if defined     (ADI_ADV7170_DEVICE) 
        0x6A,   /* AD7170 device twi address */
#elif defined   (ADI_ADV7171_DEVICE)
        0x2A,   /* AD7171 device twi address */
#elif defined   (ADI_ADV7174_DEVICE)
        0x6A,   /* AD7174 device twi address */
#elif defined   (ADI_ADV7179_DEVICE)
        0x2A,   /* AD7179 device twi address */
#endif       
        {       /* Semaphores */
#if defined (__ADSP_EDINBURGH__)    ||\
    defined (__ADSP_BRAEMAR__)      ||\
    defined (__ADSP_STIRLING__)     ||\
    defined (__ADSP_TETON__)        ||\
    defined (__ADSP_KOOKABURRA__)   ||\
    defined (__ADSP_MOCKINGBIRD__)
            0,      /* No Triple FS     */
            0,      /* No Frame Sync    */
#endif
            0,      /* NTSC by default  */
            FALSE,  /* Dataflow disabled*/
            FALSE,  /* Device not in use*/
        },
    },
};
           
/*********************************************************************

Static functions

*********************************************************************/
static u32 adi_pdd_Open(                            /* Open a device */
    ADI_DEV_MANAGER_HANDLE  ManagerHandle,          /* device manager handle */
    u32                     DeviceNumber,           /* device number */
    ADI_DEV_DEVICE_HANDLE   DMHandle,               /* device handle */
    ADI_DEV_PDD_HANDLE      *pPDDHandle,            /* pointer to PDD handle location */
    ADI_DEV_DIRECTION       Direction,              /* data direction */
    void                    *pEnterCriticalArg,     /* enter critical region parameter */
    ADI_DMA_MANAGER_HANDLE  DMAHandle,              /* handle to the DMA manager */
    ADI_DCB_HANDLE          DCBHandle,              /* callback handle */
    ADI_DCB_CALLBACK_FN     DMCallback              /* device manager callback function */
);

static u32 adi_pdd_Close(                           /* Closes a device */
    ADI_DEV_PDD_HANDLE      PDDHandle               /* PDD handle */
);

static u32 adi_pdd_Read(                            /* Reads data or queues an inbound buffer to a device */
    ADI_DEV_PDD_HANDLE      PDDHandle,              /* PDD handle */
    ADI_DEV_BUFFER_TYPE     BufferType,             /* buffer type */
    ADI_DEV_BUFFER          *pBuffer                /* pointer to buffer */
);

static u32 adi_pdd_SequentialIO(                    /* Reads data or queues an inbound buffer to a device */
    ADI_DEV_PDD_HANDLE      PDDHandle,              /* PDD handle */
    ADI_DEV_BUFFER_TYPE     BufferType,             /* buffer type */
    ADI_DEV_BUFFER          *pBuffer                /* pointer to buffer */
);
    
static u32 adi_pdd_Write(                           /* Writes data or queues an outbound buffer to a device */
    ADI_DEV_PDD_HANDLE      PDDHandle,              /* PDD handle */
    ADI_DEV_BUFFER_TYPE     BufferType,             /* buffer type */
    ADI_DEV_BUFFER          *pBuffer                /* pointer to buffer */
);
    
static u32 adi_pdd_Control(                         /* Sets or senses a device specific parameter */
    ADI_DEV_PDD_HANDLE      PDDHandle,              /* PDD handle */
    u32                     Command,                /* command ID */
    void                    *Value                  /* command specific value */
);

/* the callback function passed to the PPI driver */
static void PpiCallbackFunction(
    void*                   DeviceHandle,           /* Device Handle */ 
    u32                     Event,                  /* Callback event */
    void*                   pArg                    /* Callback Argument */
);

/* Device access function specific to ADV717x device */
static u32 DeviceAccessFunction(
    void                    *Handle,                /* AD717x driver Argument passed back by the device access */
    u16                     *AD717xRegAddr,         /* AD717x Register address being configured */
    u16                     AD717xRegValue,         /* Value of AD717x Register address being configured */
    ADI_DEVICE_ACCESS_MODE  AccessMode              /* present Device access mode of ADV717x */
);

/* Function to open a PPI/EPPI device */
static u32 PpiOpen( 
    ADI_ADV717x             *pADV717x               /* Pointer to the device we're working on */
);

/* Function to close a PPI/EPPI device */
static u32 PpiClose( 
    ADI_ADV717x             *pADV717x               /* Pointer to the device we're working on */
);

#if defined(ADI_DEV_DEBUG)
/*********************************************************************

Debug Mode functions (debug build only)

*********************************************************************/
/* Validates Physical Device Driver Handle */
static int ValidatePDDHandle(
    ADI_DEV_PDD_HANDLE      PDDHandle               /* Pointer to Physical Device Driver Handle */
);

#endif

/*********************************************************************

Global Registers

*********************************************************************/

/*********************************************************************
Table for Register Field Error check and Register field access
Table structure -  'Count' of Register addresses containing individual fields
     Register address containing individual fields, 
     Register field locations in the corresponding register, 
     Reserved bit locations in the corresponding register
Register field location - Bit indicating start of new field in a register will be 1
Reserved bit locations will be 1
*********************************************************************/

/* ADV717x register address to perform Register Field Error check and Register field access */
static u16 Adv717xRegAddr1[]    = { ADV717x_MR0,  ADV717x_MR1,  ADV717x_MR2,  
                                    ADV717x_MR3,  ADV717x_MR4,  ADV717x_TMR0, 
                                    ADV717x_TMR1, ADV717x_CGMS_WSS0, ADV717x_CGMS_WSS1, 
                                    ADV717x_TTX_REQ };
/* Register Field start locations corresponding to the entries in Adv717xRegAddr1 
(Reserved Bit locations marked as 1) */
static u16 Adv717xRegField[]    = { 0x25, 0xFB, 0xFB, 
                                    0xFF, 0xFF, 0xDB, 
                                    0x55, 0xF1, 0x41, 
                                    0x11    };

static ADI_DEVICE_ACCESS_REGISTER_FIELD RegisterField[] = {
    sizeof(Adv717xRegAddr1)/2,  /* 'Count' of Register addresses containing individual fields */
    Adv717xRegAddr1,            /* array of ADV717x register addresses containing individual fields */
    Adv717xRegField,            /* array of register field locations in the corresponding registers */
};

/*********************************************************************
Table to configure the reserved bits in the device to its recommended values
Table structure -  'Count' of Register addresses containing Reserved Locations
     Register address containing Reserved Locations
     Recommended value for the Reserved Locations
*********************************************************************/

#if defined(ADI_ADV7170_DEVICE) || defined (ADI_ADV7171_DEVICE)

/* ADV7170/ADV7171 Register address containing Reserved Locations */
static u16 Adv717xRegAddr2[]        = { ADV717x_MR2,  ADV717x_MR3,  ADV717x_MR4  };
/* ADV7170/ADV7171 register Reserved Bit locations corresponding to the entries in Adv717xRegAddr1 */
static u16 Adv717xReservedBits[]    = { 0x80, 0x03, 0x80 };
/* Recommended values for the Reserved Bit locations corresponding to the entries in Adv717xRegAddr2 */
static u16 Adv717xReservedValues[]  = { 0x00, 0x00, 0x00 };

#elif defined(ADI_ADV7174_DEVICE) || defined (ADI_ADV7179_DEVICE)

/* ADV7174/ADV7179 Register address containing Reserved Locations */
static u16 Adv717xRegAddr2[]        = { ADV717x_MR1,  ADV717x_MR2,  ADV717x_MR3,  
                                        ADV717x_MR4,  ADV717x_TMR0  };
/* ADV7174/ADV7179 register Reserved Bit locations corresponding to the entries in Adv717xRegAddr1 */
static u16 Adv717xReservedBits[]    = { 0x10, 0x80, 0x03, 
                                        0x80, 0x40  };
/* Recommended values for the Reserved Bit locations corresponding to the entries in Adv717xRegAddr2 */
static u16 Adv717xReservedValues[]  = { 0x10, 0x00, 0x00, 
                                        0x00, 0x00  };

#endif

static ADI_DEVICE_ACCESS_RESERVED_VALUES ReservedValues[] = {
    sizeof(Adv717xRegAddr2)/2,  // 'Count' of Register addresses containing Reserved Locations */
    Adv717xRegAddr2,            // array of ADV717x Register address containing Reserved Locations */
    Adv717xReservedBits,        // array of reserved bit locations in the corresponding register */
    Adv717xReservedValues,      // array of Recommended values for the Reserved Bit locations */
};

/*********************************************************************
Table for ADV717x Register Error check for invalid & read-only register(s) access
Structure - 'Count' of invalid Register addresses in ADV717x
    Invalid Register addresses in ADV717x
    'Count' of Read-only Register addresses in ADV717x
    Read-only Register addresses in ADV717x
*********************************************************************/

/* array of read-only registers addresses in ADV717x. */
/* No Read-only registers available in ADV717x */

/* array of invalid registers addresses in ADV717x */
#if defined(ADI_ADV7170_DEVICE) || defined (ADI_ADV7174_DEVICE)

/* Invalid address locations in ADV7170/ADV7174 */
/* Subcarrier register address is also listed here as they should not be accessed seperately */
static u16 Adv717xInvalidRegs[]={ 0x05, 0x06, 0x09, 0x0A, 0x0B, 0x0C, 0x1A, 0x1B, 0x1C, 0x1D };

#elif defined(ADI_ADV7171_DEVICE) || defined (ADI_ADV7179_DEVICE)

/* Invalid address locations in ADV7171/ADV7179 */
/* Subcarrier register address is also listed here as they should not be accessed seperately */
static u16 Adv717xInvalidRegs[]={ 0x05, 0x06, 0x09, 0x0A, 0x0B, 0x0C };

#endif

static ADI_DEVICE_ACCESS_VALIDATE_REGISTER ValidateRegister[] = {
    sizeof(Adv717xInvalidRegs)/2,   /* 'Count' of Invalid Register addresses in ADV717x */
    Adv717xInvalidRegs,             /* array of Invalid Register addresses in ADV717x */
    0,                              /* 'Count' of Read-only Register addresses in ADV717x */
    NULL,                           /* pointer to array of Read-only Register addresses in ADV717x */
};

// Table to select Device access type
static ADI_DEVICE_ACCESS_SELECT SelectTWIAccess[] = {
    0,                              /* Don't care in case of TWI access */
    ADI_DEVICE_ACCESS_LENGTH0,      /* 'Device' Global address (Don't care for TWI Access) */
    ADI_DEVICE_ACCESS_LENGTH1,      /* 'Device' register address length (1 byte) */
    ADI_DEVICE_ACCESS_LENGTH1,      /* 'Device' register data length (1 byte) */
    ADI_DEVICE_ACCESS_TYPE_TWI,     /* Select TWI access */
};

/*********************************************************************
*
*   Function:       PpiOpen
*
*   Description:    Opens PPI / EPPI device for ADV717x video dataflow
*
*********************************************************************/
static u32 PpiOpen( 
    ADI_ADV717x         *pADV717x               /* Pointer to the device we're working on */
){
    
    /* assume we're going to be success */
    u32 Result = ADI_DEV_RESULT_SUCCESS; 

    /* Open the PPI driver */
    Result = adi_dev_Open( 
        pADV717x->ManagerHandle,    /* device manager handle */
#if defined (__ADSP_EDINBURGH__)    ||\
    defined (__ADSP_BRAEMAR__)      ||\
    defined (__ADSP_STIRLING__)     ||\
    defined (__ADSP_TETON__)        ||\
    defined (__ADSP_KOOKABURRA__)   ||\
    defined (__ADSP_MOCKINGBIRD__)
        &ADIPPIEntryPoint,          /* PPI Entry point  */
#elif defined (__ADSP_MOAB__)
        &ADIEPPIEntryPoint,         /* EPPI Entry point */
#endif
        pADV717x->PpiDeviceNumber,  /* PPI/EPPI device number */
        pADV717x,                   /* client handle - passed to internal callback function */
        &pADV717x->PpiHandle,       /* pointer to DM handle (for PPI/EPPI driver) location */
        ADI_DEV_DIRECTION_OUTBOUND, /* PPI/EPPI used only to output video data */
        pADV717x->DMAHandle,        /* handle to the DMA manager */
        pADV717x->DCBHandle,        /* handle to the callback manager */
        PpiCallbackFunction         /* internal callback function */
    );

    /* Continue only when the PPI driver open is a success */
    if (Result == ADI_DEV_RESULT_SUCCESS)
    {

#if defined (__ADSP_EDINBURGH__)    ||\
    defined (__ADSP_BRAEMAR__)      ||\
    defined (__ADSP_STIRLING__)     ||\
    defined (__ADSP_TETON__)        ||\
    defined (__ADSP_KOOKABURRA__)   ||\
    defined (__ADSP_MOCKINGBIRD__)
        /* table of PPI driver configuration values */
        ADI_DEV_CMD_VALUE_PAIR PpiConfig [] = {              
            { ADI_PPI_CMD_SET_LINES_PER_FRAME_REG,  (void *)pADV717x->PpiLinesPerFrame  },/* Lines Per Frame count  */
            { ADI_PPI_CMD_SET_CONTROL_REG,          (void *)pADV717x->PpiControl        },/* PPI Control Register   */
            { ADI_DEV_CMD_END,                      NULL                                },
        };
#elif defined (__ADSP_MOAB__)
        /* table of PPI driver configuration values */
        ADI_DEV_CMD_VALUE_PAIR PpiConfig [] = {  
            { ADI_EPPI_CMD_SET_SAMPLES_PER_LINE,    (void *)pADV717x->PpiSamplesPerLine },/* Samples Per Line count */
            { ADI_EPPI_CMD_SET_LINES_PER_FRAME,     (void *)pADV717x->PpiLinesPerFrame  },/* Lines Per Frame count  */
            { ADI_EPPI_CMD_SET_CONTROL_REG,         (void *)pADV717x->PpiControl        },/* EPPI Control Register  */
            { ADI_DEV_CMD_END,                      NULL                                },
        };
#endif
        /* Configure PPI/EPPI device for ADV717x */    
        Result = adi_dev_Control( pADV717x->PpiHandle, ADI_DEV_CMD_TABLE, (void*)PpiConfig );
    }
    
    return (Result);
}

/*********************************************************************
*
*   Function:       PpiClose
*
*   Description:    Closes PPI / EPPI device used for 
*                   ADV717x video dataflow
*
*********************************************************************/
static u32 PpiClose( 
    ADI_ADV717x     *pADV717x               /* Pointer to the device we're working on */
){
    
    /* assume we're going to be success */
    u32 Result = ADI_DEV_RESULT_SUCCESS; 
        
    /* Check if any PPI/EPPI device is open */
    /* if so, close the present PPI/EPPI device in use */
    if (pADV717x->PpiHandle != NULL)
    {
        /* close PPI/EPPI device */
        if ((Result = adi_dev_Close(pADV717x->PpiHandle))== ADI_DEV_RESULT_SUCCESS)
        {
            /* Mark PPI/EPPI Handle as NULL indicating that the device is closed */
            pADV717x->PpiHandle = NULL;
        }
    }

    return (Result);
}

/*********************************************************************
*
* Function:  adi_pdd_Open
*
* Description: Opens the ADV717x device for use
*
*********************************************************************/
static u32 adi_pdd_Open(                            /* Open a device */
    ADI_DEV_MANAGER_HANDLE  ManagerHandle,          /* device manager handle */
    u32                     DeviceNumber,           /* device number */
    ADI_DEV_DEVICE_HANDLE   DMHandle,               /* device handle */
    ADI_DEV_PDD_HANDLE      *pPDDHandle,            /* pointer to PDD handle location */
    ADI_DEV_DIRECTION       Direction,              /* data direction */
    void                    *pEnterCriticalArg,     /* enter critical region parameter */
    ADI_DMA_MANAGER_HANDLE  DMAHandle,              /* handle to the DMA manager */
    ADI_DCB_HANDLE          DCBHandle,              /* callback handle */
    ADI_DCB_CALLBACK_FN     DMCallback              /* device manager callback function */
){
 
    u32             Result;             /* return code */
    ADI_ADV717x     *pADV717x;          /* pointer to the ADV717x device we're working on */
    void            *pExitCriticalArg;  /* exit critical region parameter */

/* Debug Build Only */
#ifdef ADI_DEV_DEBUG
    /* Check for a valid device number */
    if (DeviceNumber >= ADI_ADV717x_NUM_DEVICES)
    {
        /* return error */
        Result = ADI_DEV_RESULT_BAD_DEVICE_NUMBER;
    }
    else
    {        
#endif

    /* Configure ADV717x device instance*/
    /* assign the pointer to the device instance */
    pADV717x = &Device[DeviceNumber];
    /* and store the Manager handle */
    pADV717x->ManagerHandle = ManagerHandle;    
    /* and store the Device Manager handle */
    pADV717x->DMHandle = DMHandle;
    /* and store the DMA Manager handle */
    pADV717x->DMAHandle = DMAHandle;
    /* and store the DCallback Manager handle */
    pADV717x->DCBHandle = DCBHandle;
    /* and callback function */
    pADV717x->DMCallback = DMCallback;
    
    /* insure the device the client wants is available */
    Result = ADI_DEV_RESULT_DEVICE_IN_USE;
    /* Check if this device instance is not already in use
       If not, assign flag to indicate that it is now */
    /* Protect this section of code from interrupts */
    pExitCriticalArg = adi_int_EnterCriticalRegion(pEnterCriticalArg);
    if (pADV717x->Semaphores.InUseFlag == FALSE) 
    {
        /* Reserve this device for the client */
        pADV717x->Semaphores.InUseFlag = TRUE;
        Result = ADI_DEV_RESULT_SUCCESS;
    }
    /* unprotect */
    adi_int_ExitCriticalRegion(pExitCriticalArg);

    if (Result == ADI_DEV_RESULT_SUCCESS)
    {
        /* save the physical device handle in the client supplied location */
        *pPDDHandle = (ADI_DEV_PDD_HANDLE *)pADV717x;
    }
    
/* Debug Build Only */
#ifdef ADI_DEV_DEBUG
    }
#endif

    /* return */
    return(Result);
}

/*********************************************************************
*
* Function:  adi_pdd_Close
*
* Description: Closes down PPI/EPPI & ADV717x device 
*
*********************************************************************/
static u32 adi_pdd_Close(                           /* Closes a device */
    ADI_DEV_PDD_HANDLE      PDDHandle               /* PDD handle */
){

    /* assume we're going to be success */
    u32 Result = ADI_DEV_RESULT_SUCCESS; 
    /* Pointer to the device driver instance */
    ADI_ADV717x  *pADV717x = (ADI_ADV717x *)PDDHandle;

/* Debug build only */
#if defined(ADI_DEV_DEBUG)
    /* Validate the PDD handle */
    if ((Result = ValidatePDDHandle(PDDHandle)) == ADI_DEV_RESULT_SUCCESS)
    {
#endif

    /* Close the present PPI/EPPI device being used */
    Result = PpiClose(pADV717x);

    /* mark this ADV717x device as closed */
    pADV717x->Semaphores.InUseFlag = FALSE;

/* Debug Build Only */
#ifdef ADI_DEV_DEBUG
    }
#endif

    /* return */
    return(Result);
}

/*********************************************************************
*
* Function:  adi_pdd_Read
*
* Description: Never called as ADV717x is an outbound device
*
*********************************************************************/
static u32 adi_pdd_Read(                            /* Reads data or queues an inbound buffer to a device */
    ADI_DEV_PDD_HANDLE      PDDHandle,              /* PDD handle */
    ADI_DEV_BUFFER_TYPE     BufferType,             /* buffer type */
    ADI_DEV_BUFFER          *pBuffer                /* pointer to buffer */
){
    /* No read operation for ADV717x */
    return(ADI_ADV717x_RESULT_CMD_NOT_SUPPORTED);
}

/*********************************************************************
*
* Function:  adi_pdd_SequentialIO
*
* Description: Function not supported by this driver
*
*********************************************************************/
static u32 adi_pdd_SequentialIO(                    /* Reads data or queues an inbound buffer to a device */
    ADI_DEV_PDD_HANDLE      PDDHandle,              /* PDD handle */
    ADI_DEV_BUFFER_TYPE     BufferType,             /* buffer type */
    ADI_DEV_BUFFER          *pBuffer                /* pointer to buffer */
){
    /* Function not supported */
    return(ADI_ADV717x_RESULT_CMD_NOT_SUPPORTED);
}
/*********************************************************************
*
* Function:  adi_pdd_Write
*
* Description: Pass this to PPI/EPPI device (Video data)
*
*********************************************************************/
static u32 adi_pdd_Write(                           /* Writes data or queues an outbound buffer to a device */
    ADI_DEV_PDD_HANDLE      PDDHandle,              /* PDD handle */
    ADI_DEV_BUFFER_TYPE     BufferType,             /* buffer type */
    ADI_DEV_BUFFER          *pBuffer                /* pointer to buffer */
){
    /*  Pointer to the device instance we're working on */
    ADI_ADV717x  *pADV717x = (ADI_ADV717x *)PDDHandle;
    /* assume we're going to be success */
    u32 Result = ADI_DEV_RESULT_SUCCESS; 
    
/* Debug build only */
#if defined(ADI_DEV_DEBUG)
    /* Validate the PDD handle */
    if ((Result = ValidatePDDHandle(PDDHandle)) == ADI_DEV_RESULT_SUCCESS)
    {
#endif

    /* pass the request on to PPI/EPPI */
    Result = adi_dev_Write (pADV717x->PpiHandle, BufferType, pBuffer);

/* Debug Build Only */
#ifdef ADI_DEV_DEBUG
    }
#endif

    /* return */
    return(Result);
 }

/*********************************************************************
*
* Function:  adi_pdd_Control
*
* Description: Sets or senses ADV717x device specific parameters
*
*********************************************************************/
static u32 adi_pdd_Control(                         /* Sets or senses a device specific parameter */
    ADI_DEV_PDD_HANDLE      PDDHandle,              /* PDD handle */
    u32                     Command,                /* command ID */
    void                    *Value                  /* command specific value */
){

    ADI_ADV717x *pADV717x;  /* pointer to the device we're working on */
    u32         Result;     /* return value */
    u32         u32Value;   /* u32 type to avoid casts/warnings etc. */
    u8          u8Value;    /* u8 type to avoid casts/warnings etc. */

    u16         SCFRvalue[4]; /* array to hold subcarrier frequency value */
 
    /* passed to device access to read/write AD717x SCFR */
    ADI_DEV_ACCESS_REGISTER_BLOCK   AccessSCFR;
    /* passed to device access to reset ADV717x timing registers */
    ADI_DEV_ACCESS_REGISTER_FIELD   ResetTimingReg;
    /* Device Access API structure */
    ADI_DEVICE_ACCESS_REGISTERS     AccessAdv717x;

    /*  Pointer to the device instance we're working on */
    pADV717x = (ADI_ADV717x *)PDDHandle;
    /* assign 8, 16 and 32 bit values for the Value argument */
    u32Value = (u32) Value;
    u8Value  = (u8) u32Value;
    
/* Debug build only */
#if defined(ADI_DEV_DEBUG)
    /* Validate the PDD handle */
    if ((Result = ValidatePDDHandle(PDDHandle)) == ADI_DEV_RESULT_SUCCESS)
    {
#endif
 
    /* assume we're going to be successful */
    Result = ADI_DEV_RESULT_SUCCESS;
    
    /* CASEOF (Command) */
    switch (Command)    
    {
        /* CASE (control dataflow) */
        case (ADI_DEV_CMD_SET_DATAFLOW):

            /* IF (client switches dataflow on/off with PPI/EPPI open) */
            if (pADV717x->PpiHandle != NULL)
            {
                /* Enable dataflow with ADV717x in non-standard video mode */
                if ((u8Value) && (pADV717x->Semaphores.Adv717xMode))
                {
#if defined (__ADSP_EDINBURGH__)    ||\
    defined (__ADSP_BRAEMAR__)      ||\
    defined (__ADSP_STIRLING__)     ||\
    defined (__ADSP_TETON__)        ||\
    defined (__ADSP_KOOKABURRA__)   ||\
    defined (__ADSP_MOCKINGBIRD__)
                    /* IF (Frame Sync Count or Lines per frame Count is not set) */
                    if ((pADV717x->Semaphores.FrameSyncCount == 0) || (pADV717x->PpiLinesPerFrame == 0))
                    {
                        Result = ADI_ADV717x_RESULT_FRAME_ERROR;
                        break;  /* exit on error */
                    }
                    /* IF (Frame Sync 1 enabled?) */
                    if (pADV717x->Semaphores.FrameSyncCount)
                    {
                        /* IF (Frame Sync 1 Timer configuration info available?) */
                        if (pADV717x->PPI_FS1_TMR != NULL)
                        {
                            /* Pass FS1 Timer configuration info to PPI */
                            Result = adi_dev_Control( pADV717x->PpiHandle, ADI_PPI_CMD_SET_TIMER_FRAME_SYNC_1, (void*)pADV717x->PPI_FS1_TMR );
/* Debug build only */
#if defined(ADI_DEV_DEBUG)
                            if (Result != ADI_DEV_RESULT_SUCCESS)
                            {
                                break; /* exit on error */
                            }
#endif
                        }
                        /* ELSE (FS1 Timer configuration info is missing) */
                        else
                        {
                            /* return result indicating FS timer registers are not configured */
                            Result = ADI_ADV717x_RESULT_TIMING_NOT_CONFIGURED;
                            break;  /* Exit on error */
                        }
                    }

                    /* IF (Frame Sync 2 enabled?) */
                    if (pADV717x->Semaphores.FrameSyncCount > 1)
                    {
                        /* IF (Frame Sync 2 Timer configuration info available?) */
                        if (pADV717x->PPI_FS2_TMR != NULL)
                        {
                            /* Pass FS1 Timer configuration info to PPI */
                            Result = adi_dev_Control( pADV717x->PpiHandle, ADI_PPI_CMD_SET_TIMER_FRAME_SYNC_2, (void*)pADV717x->PPI_FS2_TMR );
/* Debug build only */
#if defined(ADI_DEV_DEBUG)
                            if (Result != ADI_DEV_RESULT_SUCCESS)
                            {
                                break; /* exit on error */
                            }
#endif
                        }
                        /* ELSE (FS2 Timer configuration info is missing) */
                        else
                        {
                            /* return result indicating FS timer registers are not configured */
                            Result = ADI_ADV717x_RESULT_TIMING_NOT_CONFIGURED;
                            break;  /* Exit on error */
                        }
                    }
                                                    
                    /* if (Triple frame sync flag is set (ADV717x operates in Mode 1/2/3)) */
                    if(pADV717x->Semaphores.TripleFrameSync)
                    {
                        /* Force 3 frame syncs for PPI*/
                        Result = adi_dev_Control( pADV717x->PpiHandle, ADI_PPI_CMD_SET_TRIPLE_FRAME_SYNC, (void*) TRUE );
/* Debug build only */
#if defined(ADI_DEV_DEBUG)
                        if (Result != ADI_DEV_RESULT_SUCCESS)
                        {
                            break; /* exit on error */
                        }
#endif
                    }
#elif defined (__ADSP_MOAB__)
                    /* IF (Samples per Lines Count is not set) */
                    if ((pADV717x->PpiSamplesPerLine == 0) || (pADV717x->PpiLinesPerFrame == 0))
                    {
                        Result = ADI_ADV717x_RESULT_FRAME_ERROR;
                        break;  /* exit on error */
                    }
#endif
                }
                /* Pass Dataflow value to PPI/EPPI device */
                Result = adi_dev_Control( pADV717x->PpiHandle, ADI_DEV_CMD_SET_DATAFLOW, (void*)u8Value );
            }
            /* ELSE (check if the client is trying to enable dataflow without opening the PPI/EPPI) */
            else if (u8Value == TRUE)
            {
                /* can not pass this value to PPI/EPPI as no valid PPI/EPPI device handle is available
                   also means that the client hasn't defined the dataflow method yet, return error. */
                Result = ADI_DEV_RESULT_DATAFLOW_UNDEFINED;
                break;
            }
/* Debug build only */
#if defined(ADI_DEV_DEBUG)
            if (Result != ADI_DEV_RESULT_SUCCESS)
            {
                break; /* exit on error */
            }
#endif
            /* save the dataflow status */
            pADV717x->Semaphores.DataflowFlag = u8Value;
            break;

        /* CASE (query for processor DMA support) */
        case (ADI_DEV_CMD_GET_PERIPHERAL_DMA_SUPPORT):
  
            /* PPI/EPPI needs DMA but ADV717x doesn't */
            *((u32 *)Value) = FALSE;
            break;

        /* CASE (Set Dataflow method - applies only for PPI/EPPI) */
        case (ADI_DEV_CMD_SET_DATAFLOW_METHOD):

            /* Check if PPI/EPPI device to used by ADV717x is already open */
            if (pADV717x->PpiHandle == NULL)
            {
                /* if not, try to open the PPI device corresponding to PpiDeviceNumber for ADV717x dataflow */
                Result = PpiOpen(pADV717x);

/* Debug build only */
#if defined(ADI_DEV_DEBUG)
                if (Result != ADI_DEV_RESULT_SUCCESS)
                {
                    break; /* exit on error */
                }
#endif
            }
            
            /* Pass the dataflow method to the PPI/EPPI device allocated to ADV717x */
            Result = adi_dev_Control( pADV717x->PpiHandle, ADI_DEV_CMD_SET_DATAFLOW_METHOD, (void*)Value );
            
            break;
            
        /* CASE (Read a specific register from the ADV717x) */
        case(ADI_DEV_CMD_REGISTER_READ):
        /* CASE (Configure a specific register in the ADV717x) */
        case(ADI_DEV_CMD_REGISTER_WRITE):
        /* CASE (Read a specific field from a single device register) */
        case(ADI_DEV_CMD_REGISTER_FIELD_READ):
        /* CASE (Write to a specific field in a single device register) */
        case(ADI_DEV_CMD_REGISTER_FIELD_WRITE):         
        /* CASE (Read block of ADV717x registers starting from first given address) */
        case(ADI_DEV_CMD_REGISTER_BLOCK_READ):  
        /* CASE (Write to a block of ADV717x registers starting from first given address) */
        case(ADI_DEV_CMD_REGISTER_BLOCK_WRITE):     
        /* CASE (Read a table of selective registers in ADV717x) */
        case(ADI_DEV_CMD_REGISTER_TABLE_READ):
        /* CASE (Write to a table of selective registers in ADV717x) */
        case(ADI_DEV_CMD_REGISTER_TABLE_WRITE):     
        /* CASE (Read a table of selective register(s) field(s) in ADV717x) */
        case(ADI_DEV_CMD_REGISTER_FIELD_TABLE_READ):
        /* CASE (Write to a table of selective register(s) field(s) in ADV717x) */
        case(ADI_DEV_CMD_REGISTER_FIELD_TABLE_WRITE):
    
            AccessAdv717x.ManagerHandle     = pADV717x->ManagerHandle;      /* device manager handle */
            AccessAdv717x.ClientHandle      = pADV717x;                     /* client handle - passed to the internal 'Device' specific function */
            AccessAdv717x.DeviceNumber      = pADV717x->TwiDeviceNumber;    /* TWI device number */
            AccessAdv717x.DeviceAddress     = pADV717x->TwiDeviceAddress;   /* TWI address of ADV717x Device */
            AccessAdv717x.DCBHandle         = pADV717x->DCBHandle;          /* handle to the callback manager */
            AccessAdv717x.DeviceFunction    = DeviceAccessFunction;         /* Function specific to ADV717x driver passed to the 'Device' access service */
            AccessAdv717x.Command           = Command;                      /* command ID */
            AccessAdv717x.Value             = Value;                        /* command specific value */
            AccessAdv717x.FinalRegAddr      = ADV717x_FINAL_REG_ADDRESS;    /* Address of the last register in ADV717x */
            AccessAdv717x.RegisterField     = RegisterField;                /* table for ADV717x Register Field Error check and Register field access */
            AccessAdv717x.ReservedValues    = ReservedValues;               /* table to configure reserved bits in ADV717x to recommended values */
            AccessAdv717x.ValidateRegister  = ValidateRegister;             /* table containing reserved and read-only registers in ADV717x */
            AccessAdv717x.ConfigTable       = pADV717x->TwiConfigTable;     /* TWI configuration table */
            AccessAdv717x.SelectAccess      = SelectTWIAccess;              /* Device Access type */       
            AccessAdv717x.pAdditionalinfo   = (void *)NULL;                 /* No Additional info */

            /* call device access */
            Result = adi_device_access (&AccessAdv717x);
            break;

        /* CASE (Configure Subcarrier frequency registers) */
        case (ADI_ADV717x_CMD_SET_SCF_REG):
        /* CASE (Read Subcarrier frequency registers) */
        case (ADI_ADV717x_CMD_GET_SCF_REG):
     
            /* IF (set SCFR) */
            if (Command == ADI_ADV717x_CMD_SET_SCF_REG)
            {
                SCFRvalue[0]  = u8Value;                /* SCFR0 value */
                SCFRvalue[1]  = (u8) (u32Value >> 8);   /* SCFR1 value */
                SCFRvalue[2]  = (u8) (u32Value >> 16);  /* SCFR2 value */
                SCFRvalue[3]  = (u8) (u32Value >> 24);  /* SCFR3 value */
                /* perform AD717x register write operation */
                AccessAdv717x.Command = ADI_DEV_CMD_REGISTER_BLOCK_WRITE; 
            }
            else
            {
                /* perform AD717x register read operation */
                AccessAdv717x.Command = ADI_DEV_CMD_REGISTER_BLOCK_READ; 
            }
            AccessSCFR.Count    = 4;                /* SCFR length (4 bytes) */
            AccessSCFR.Address  = ADV717x_SCFR0;    /* SCFR0 address */
            AccessSCFR.pData    = SCFRvalue;        /* array holding SCFR value */

            AccessAdv717x.ManagerHandle     = pADV717x->ManagerHandle;      /* device manager handle */
            AccessAdv717x.ClientHandle      = pADV717x;                     /* client handle - passed to the internal 'Device' specific function */
            AccessAdv717x.DeviceNumber      = pADV717x->TwiDeviceNumber;    /* TWI device number */
            AccessAdv717x.DeviceAddress     = pADV717x->TwiDeviceAddress;   /* TWI address of ADV717x Device */
            AccessAdv717x.DCBHandle         = pADV717x->DCBHandle;          /* handle to the callback manager */
            AccessAdv717x.DeviceFunction    = DeviceAccessFunction;         /* Function specific to ADV717x driver passed to the 'Device' access service */
            AccessAdv717x.Value             = (void *)&AccessSCFR;          /* command specific value */
            AccessAdv717x.FinalRegAddr      = ADV717x_FINAL_REG_ADDRESS;    /* Address of the last register in ADV717x */
            AccessAdv717x.RegisterField     = NULL;                         /* table for ADV717x Register Field Error check and Register field access */
            AccessAdv717x.ReservedValues    = NULL;                         /* table to configure reserved bits in ADV717x to recommended values */
            AccessAdv717x.ValidateRegister  = NULL;                         /* table containing reserved and read-only registers in ADV717x */
            AccessAdv717x.ConfigTable       = pADV717x->TwiConfigTable;     /* TWI configuration table */
            AccessAdv717x.SelectAccess      = SelectTWIAccess;              /* Device Access type */
            AccessAdv717x.pAdditionalinfo   = (void *)NULL;                 /* No Additional info */

            /* call device access */
            Result = adi_device_access (&AccessAdv717x);
/* Debug build only */
#if defined(ADI_DEV_DEBUG)
            if (Result != ADI_DEV_RESULT_SUCCESS)
            {
                break; /* exit on error */
            }
#endif
            /* IF (Set SCFR) */
            if (Command == ADI_ADV717x_CMD_SET_SCF_REG)
            {   
                /* Toggle TMR0 bit 7 to reset the timing registers */
                
                ResetTimingReg.Address  = ADV717x_TMR0;             /* ADV717x_TMR0 address to access */
                ResetTimingReg.Data     = 0;                        /* Clear ADV717x_TMR0 bit 7 (Timing register reset bit) */
                ResetTimingReg.Field    = ADV717x_TIMING_REG_RST;   /* ADV717x_TMR0 Field to configure (Timing register reset bit) */

                AccessAdv717x.Command           = ADI_DEV_CMD_REGISTER_FIELD_WRITE; /* command ID */
                AccessAdv717x.Value             = (void *)&ResetTimingReg;          /* command specific value */
                AccessAdv717x.RegisterField     = RegisterField;                    /* table for ADV717x Register Field Error check and Register field access */
                AccessAdv717x.ReservedValues    = ReservedValues;                   /* table to configure reserved bits in ADV717x to recommended values */
                AccessAdv717x.ValidateRegister  = ValidateRegister;                 /* table containing reserved and read-only registers in ADV717x */
                AccessAdv717x.ConfigTable       = pADV717x->TwiConfigTable;         /* TWI configuration table */
                AccessAdv717x.SelectAccess      = SelectTWIAccess;                  /* Device Access type */
                AccessAdv717x.pAdditionalinfo   = (void *)NULL;                     /* No Additional info */

                /* call device access */
                Result = adi_device_access (&AccessAdv717x);
/* Debug build only */
#if defined(ADI_DEV_DEBUG)
                if (Result != ADI_DEV_RESULT_SUCCESS)
                {
                    break; /* exit on error */
                }
#endif
                /* Set the TMR0 bit 7 (Timing register reset bit) */
                ResetTimingReg.Data = 1;
                /* call device access */
                Result = adi_device_access (&AccessAdv717x);
                
/* Debug build only */
#if defined(ADI_DEV_DEBUG)
                if (Result != ADI_DEV_RESULT_SUCCESS)
                {
                    break; /* exit on error */
                }
#endif
                /* Clear the TMR0 bit 7 (Timing register reset bit) */
                ResetTimingReg.Data = 0;
                /* call device access */
                Result = adi_device_access (&AccessAdv717x);
                /* Give some time for encoder to synchronize */
                for (u32Value=0;u32Value<0xFFFF;u32Value++);
            }
            /* ELSE (Read SCFR) */
            else
            {
                /* update client location with SCF value */
                *((u32 *)Value) = ((u32)SCFRvalue[3] << 24) | ((u32)SCFRvalue[2] << 16) | ((u32)SCFRvalue[1] << 8) | ((u32)SCFRvalue[0]);
            }
            break;  

        /* CASE (set Timer values for PPI FS1) */
        case (ADI_ADV717x_CMD_SET_TIMER_FRAME_SYNC_1):

#if defined (__ADSP_EDINBURGH__)    ||\
    defined (__ADSP_BRAEMAR__)      ||\
    defined (__ADSP_STIRLING__)     ||\
    defined (__ADSP_TETON__)        ||\
    defined (__ADSP_KOOKABURRA__)   ||\
    defined (__ADSP_MOCKINGBIRD__)
            /* Update the PPI FS1 Timer Configuration value */
            pADV717x->PPI_FS1_TMR = (ADI_PPI_FS_TMR*) Value;            
            
            /* IF (PPI/EPPI device is open & ADV717x in non-standard video mode) */
            if ((pADV717x->PpiHandle) && (pADV717x->Semaphores.Adv717xMode))
            {
                /* Update PPI FS1 Timer registers */
                Result = adi_dev_Control( pADV717x->PpiHandle, ADI_PPI_CMD_SET_TIMER_FRAME_SYNC_1, (void*)pADV717x->PPI_FS1_TMR );
            }
#endif
            break;

        /* CASE (set Timer values for PPI FS2) */
        case (ADI_ADV717x_CMD_SET_TIMER_FRAME_SYNC_2):

#if defined (__ADSP_EDINBURGH__)    ||\
    defined (__ADSP_BRAEMAR__)      ||\
    defined (__ADSP_STIRLING__)     ||\
    defined (__ADSP_TETON__)        ||\
    defined (__ADSP_KOOKABURRA__)   ||\
    defined (__ADSP_MOCKINGBIRD__)
            /* Update the PPI FS2 Timer Configuration value */
            pADV717x->PPI_FS2_TMR = (ADI_PPI_FS_TMR*) Value;            
            
            /* IF (PPI/EPPI device is open & ADV717x in non-standard video mode) */
            if ((pADV717x->PpiHandle) && (pADV717x->Semaphores.Adv717xMode))
            {
                /* Update PPI FS2 Timer registers */
                Result = adi_dev_Control( pADV717x->PpiHandle, ADI_PPI_CMD_SET_TIMER_FRAME_SYNC_2, (void*)pADV717x->PPI_FS2_TMR );
            }
#endif
            break;

        /* CASE (Set the number of frame syncs to be generated) */
        case (ADI_ADV717x_CMD_SET_FRAME_SYNC_COUNT):

#if defined (__ADSP_EDINBURGH__)    ||\
    defined (__ADSP_BRAEMAR__)      ||\
    defined (__ADSP_STIRLING__)     ||\
    defined (__ADSP_TETON__)        ||\
    defined (__ADSP_KOOKABURRA__)   ||\
    defined (__ADSP_MOCKINGBIRD__)
            /* IF (ADV717x device is in non-standard video mode) */
            if (pADV717x->Semaphores.Adv717xMode)
            {
                /* CASEOF (Frame Sync Count) */
                switch ((ADI_ADV717x_FS_COUNT)Value)
                {
                    /* CASE: (1 Frame sync) */
                    case ADI_ADV717x_FRAME_COUNT_1:
                        pADV717x->PpiControl &= ~0x0030;            /* Clear Port CFG */
                        pADV717x->Semaphores.FrameSyncCount = 1;    /* 1 Frame Sync */
                        break;

                    /* CASE: (2 Frame syncs) */
                    case ADI_ADV717x_FRAME_COUNT_2:
                        pADV717x->PpiControl &= ~0x0030;            /* Clear Port CFG */
                        pADV717x->PpiControl |= 0x0010;             /* Set Port CFG as 1 (2 or 3 frame syncs) */
                        pADV717x->Semaphores.FrameSyncCount = 2;    /* 2 Frame Syncs */
                        break;

                    /* CASE: (3 Frame Syncs with PPI_FS3 to assertion of PPI_FS1) */
                    case ADI_ADV717x_FRAME_COUNT_3_FS1:
                        pADV717x->PpiControl &= ~0x0030;            /* Clear Port CFG */
                        pADV717x->PpiControl |= 0x0010;             /* Set Port CFG as 1 (2 or 3 frame syncs) */
                        pADV717x->Semaphores.TripleFrameSync = 1;   /* update triple frame sync flag */
                        pADV717x->Semaphores.FrameSyncCount = 3;    /* 3 Frame Syncs */
                        break;

                    /* CASE: (3 Frame Syncs with PPI_FS3 to assertion of PPI_FS2) */
                    case ADI_ADV717x_FRAME_COUNT_3_FS2:
                        pADV717x->PpiControl &= ~0x0030;            /* Clear Port CFG */
                        pADV717x->PpiControl |= 0x0030;             /* Set Port CFG as 3 (3 Frame Syncs with PPI_FS3 to assertion of PPI_FS2) */
                        pADV717x->Semaphores.TripleFrameSync = 1;   /* update triple frame sync flag */
                        pADV717x->Semaphores.FrameSyncCount = 3;    /* 3 Frame Syncs */
                        break;
                    
                    /* CASE: (can't recogonise ADI_ADV717x_FS_COUNT) */
                    default:
                        Result = ADI_ADV717x_RESULT_FRAME_ERROR;    /*  return error */
                }
/* Debug build only */
#if defined(ADI_DEV_DEBUG)
                if (Result != ADI_DEV_RESULT_SUCCESS)
                {
                    break; /* exit on error */
                }
#endif
                /* if PPI device is already opened */
                if (pADV717x->PpiHandle)
                {
                    /* update the PPI control register */
                    Result = adi_dev_Control( pADV717x->PpiHandle, ADI_PPI_CMD_SET_CONTROL_REG, (void*)pADV717x->PpiControl );
/* Debug build only */
#if defined(ADI_DEV_DEBUG)
                    if (Result != ADI_DEV_RESULT_SUCCESS)
                    {
                        break; /* exit on error */
                    }
#endif
                    /* IF TripleFrameSync enabled */
                    if (pADV717x->Semaphores.TripleFrameSync)
                    {
                        /* Force triple frame sync */
                        Result = adi_dev_Control( pADV717x->PpiHandle, ADI_PPI_CMD_SET_TRIPLE_FRAME_SYNC, (void*) TRUE );
                    }
                }
            }
            /* ELSE (Command not valid for this ADV717x mode) */
            else
            {
                Result = ADI_ADV717x_RESULT_OPERATING_MODE_MISMATCH;
            }
#endif
            break;

/*************************************
PPI/EPPI related commands
*************************************/

        /* CASE (Set PPI/EPPI Frame Lines count) */
        case (ADI_ADV717x_CMD_SET_FRAME_LINES_COUNT):

            /* Update the PPI Lines per frame Count */
            pADV717x->PpiLinesPerFrame = u32Value;

            /* if PPI/EPPI device is already opened */
            if (pADV717x->PpiHandle)
            {
#if defined (__ADSP_EDINBURGH__)    ||\
    defined (__ADSP_BRAEMAR__)      ||\
    defined (__ADSP_STIRLING__)     ||\
    defined (__ADSP_TETON__)        ||\
    defined (__ADSP_KOOKABURRA__)   ||\
    defined (__ADSP_MOCKINGBIRD__)
                /* update PPI Lines per Frame register */
                Result = adi_dev_Control( pADV717x->PpiHandle, ADI_PPI_CMD_SET_LINES_PER_FRAME_REG, (void*)pADV717x->PpiLinesPerFrame );
#elif defined (__ADSP_MOAB__)
                /* update EPPI Lines per Frame register */
                Result = adi_dev_Control( pADV717x->PpiHandle, ADI_EPPI_CMD_SET_LINES_PER_FRAME, (void*)pADV717x->PpiLinesPerFrame );
#endif
            }                
            break;

        /* CASE (Set EPPI Samples per Line count) Command applicable only for EPPI based devices */
        case (ADI_ADV717x_CMD_SET_SAMPLES_PER_LINE):
#if defined (__ADSP_MOAB__)
            /* Update the EPPI Samples Per line Count */
            pADV717x->PpiSamplesPerLine = u32Value;
            /* if EPPI device is already opened */
            if (pADV717x->PpiHandle)
            {
                /* update EPPI Samples Per line register */
                Result = adi_dev_Control( pADV717x->PpiHandle, ADI_EPPI_CMD_SET_SAMPLES_PER_LINE, (void*)pADV717x->PpiSamplesPerLine );
            }
#endif
            break;

                
        /* CASE (Set PPI/EPPI Device Number that will be used to send video data to ADV717x) */
        case (ADI_ADV717x_CMD_SET_PPI_DEVICE_NUMBER):
        
            /* Close the present PPI device being used */
            Result = PpiClose(pADV717x);

/* Debug build only */
#if defined(ADI_DEV_DEBUG)
                if (Result != ADI_DEV_RESULT_SUCCESS)
                {
                    break; /* exit on error */
                }
#endif

            /* Update the PPI device number */
            pADV717x->PpiDeviceNumber = u8Value;

            /* The client (application) must open the new PPI/EPPI device, set its data flow method,
               load the buffer(s) for the new PPI/EPPI device & enable ADV717x dataflow */

            break;

        /* CASE (Set PPI Device status (Open/Close PPI/EPPI)) */
        case (ADI_ADV717x_CMD_SET_PPI_STATUS):

            /*IF (Open PPI/EPPI device) */            
            if ((ADI_ADV717x_SET_PPI_STATUS) Value == ADI_ADV717x_PPI_OPEN)
            {
                /* IF (a PPI/EPPI device is not opened for ADV717x) */
                if (pADV717x->PpiHandle== NULL)
                {
                    /* open the PPI/EPPI Device */
                    Result = PpiOpen(pADV717x);
                }
            }
            /* ELSE (Close the PPI/EPPI device used by ADV717x) */
            else
            {
                /* Close any PPI/EPPI device presently used by ADV717x */
                Result = PpiClose(pADV717x);
            }
            break;
            
/*************************************
TWI related commands
*************************************/

        /* CASE (Set TWI Device Number to be used access ADV717x device registers) */
        case (ADI_ADV717x_CMD_SET_TWI_DEVICE_NUMBER):

            /* Update the TWI device number */
            pADV717x->TwiDeviceNumber = u8Value;

            break;

        /* CASE (Set pointer to TWI Configuration table specific to the application) */
        case (ADI_ADV717x_CMD_SET_TWI_CONFIG_TABLE):

            /* Update the TWI Configuration table */
            pADV717x->TwiConfigTable = (ADI_DEV_CMD_VALUE_PAIR*) Value;

            break;
    
        /* CASE (Set TWI address for the ADV717x device) */
        case (ADI_ADV717x_CMD_SET_TWI_DEVICE_ADDRESS):

            /* Update the TWI device number */
            pADV717x->TwiDeviceAddress = u32Value;

            break;
                
        /* CASE (Not a ADV717x driver specific command. Pass it to PPI/EPPI driver */    
        default:

            /* IF (PPI/EPPI device is already open) */
            if (pADV717x->PpiHandle)
            {
                /* pass the unknown command to PPI/EPPI */
                Result = adi_dev_Control( pADV717x->PpiHandle, Command, Value );
            }
            /* ELSE (Command not supported, return error) */
            else
            {
                Result = ADI_ADV717x_RESULT_CMD_NOT_SUPPORTED;
            }
            break;
    }
/* Debug Build Only */
#ifdef ADI_DEV_DEBUG
    }
#endif
    return(Result);
}

/*********************************************************************

 Function:  PpiCallbackFunction

 Description: Fields the callback from the PPI Driver

*********************************************************************/
static void PpiCallbackFunction(
    void                    *DeviceHandle,  /* Device Handle */ 
    u32                     Event,          /* Callback event */
    void                    *pArg         	/* Callback Argument */
){
    /* Pointer to the ADV717x device we're working on */
    ADI_ADV717x *pADV717x = (ADI_ADV717x *)DeviceHandle;
    
/* Debug build only */
#if defined(ADI_DEV_DEBUG)
    /* Validate the PDD handle */
    if (ValidatePDDHandle(DeviceHandle) == ADI_DEV_RESULT_SUCCESS)
    {
#endif

    /* simply pass the callback along to the Device Manager Callback */
    (pADV717x->DMCallback)(pADV717x->DMHandle,Event,pArg);

/* Debug Build Only */
#ifdef ADI_DEV_DEBUG
    }
#endif
}

/*********************************************************************

Function:  DeviceAccessFunction

Description: ADV717x driver specific Device access function


*********************************************************************/
static u32 DeviceAccessFunction(
    void                    *Handle,                /* AD717x driver Argument passed back by the device access */
    u16                     *AD717xRegAddr,         /* AD717x Register address being configured */
    u16                     AD717xRegValue,         /* Value of AD717x Register address being configured */
    ADI_DEVICE_ACCESS_MODE  AccessMode              /* present Device access mode of ADV717x */
){
    /* Pointer to ADV717x device we're working on */
    ADI_ADV717x *pADV717x = (ADI_ADV717x *)Handle;
    /* assume we're going to be success */
    u32     Result = ADI_DEV_RESULT_SUCCESS;
    u8      UpdatePpiLines = 0;  /* Indicates if PPI/EPPI Lines per frame register need to be updated or not */
    u8      UpdatePpiControl = 0;  /* Indicates if PPI/EPPI control register need to be updated or not */
    
  /* Debug build only */
#if defined(ADI_DEV_DEBUG)
    /* Validate the given handle */
    if ((Result = ValidatePDDHandle(Handle)) == ADI_DEV_RESULT_SUCCESS)
    {
#endif



/* Debug Build Only */
#ifdef ADI_DEV_DEBUG
    }
#endif
    /* IF (Device Access Mode is Pre Write to ADV717x registers) */
    if (AccessMode == ADI_DEVICE_ACCESS_PRE_WRITE)
    {
        /* CASEOF (ADV717x register address) */
        switch(*AD717xRegAddr)
        {
            /* CASE: (Device access configuring ADV717x_MR0 Register) */
            case ADV717x_MR0:
                /* IF (Bit 0 of MR0 == 1 and ADV717x output mode changed) */
                if (AD717xRegValue & 0x01)
                {
                    /* PAL video output */
                    pADV717x->Semaphores.NtscPalFlag = 1;
                }
                /* ELSE Bit 0 of MR0 == 0) */
                else
                {
                    /* NTSC video output */
                    pADV717x->Semaphores.NtscPalFlag = 0;
                }
                UpdatePpiLines = 1;    /* Update PPI/EPPI Lines per frame */
                
                break;
                
            /* CASE: (Device access configuring ADV717x_TMR0 Register) */
            case ADV717x_TMR0:
       
                /* Check ADV717x video mode */
                /* IF (ADV717x configured in a non-standard video out mode) */
                if((AD717xRegValue & 0x06) && (pADV717x->Semaphores.Adv717xMode))
                {
#if defined (__ADSP_EDINBURGH__)    ||\
    defined (__ADSP_BRAEMAR__)      ||\
    defined (__ADSP_STIRLING__)     ||\
    defined (__ADSP_TETON__)        ||\
    defined (__ADSP_KOOKABURRA__)   ||\
    defined (__ADSP_MOCKINGBIRD__)
                    /* update ADV717x device mode indication flag to non-standard video mode */
                    pADV717x->Semaphores.Adv717xMode = 1;                     
                    /* PPI in output mode with 1,2 or 3 frame syncs */
                    pADV717x->PpiControl |= 0x000C;     /* transfer type set to 3 */
                    UpdatePpiControl = 1;   /* update PPI/EPPI Control register */
#endif
                }
                /* ELSE (ADV717x operates in standard video mode) */
                else
                {
                    /* IF (Previous ADV717x was non-standard video mode) */
                    if (pADV717x->Semaphores.Adv717xMode)
                    {
                        /* mark flag as Adv717xMode flag as in standard mode */
                        pADV717x->Semaphores.Adv717xMode    = 0;
#if defined (__ADSP_EDINBURGH__)    ||\
    defined (__ADSP_BRAEMAR__)      ||\
    defined (__ADSP_STIRLING__)     ||\
    defined (__ADSP_TETON__)        ||\
    defined (__ADSP_KOOKABURRA__)   ||\
    defined (__ADSP_MOCKINGBIRD__)
                        /* clear triple frame sync flag as ADV717x operates in standard mode */
                        pADV717x->Semaphores.TripleFrameSync= 0;
                        /* No Frame syncs in standard mode. 
                           Client must set frame sync count when ADV717x is operated in non-standard video mode */
                        pADV717x->Semaphores.FrameSyncCount = 0;
#endif
                        /* Clear Lines per Frame as it will be updated later */
                        pADV717x->PpiLinesPerFrame          = 0;
                        
                        /* Configure PPI output with ITU 656 standard data
                          (Unpack enabled,Transfer Entire field,PPI in output mode) */
#if defined (__ADSP_EDINBURGH__)    ||\
    defined (__ADSP_BRAEMAR__)      ||\
    defined (__ADSP_STIRLING__)     ||\
    defined (__ADSP_KOOKABURRA__)   ||\
    defined (__ADSP_MOCKINGBIRD__)
                        /* PPI output with ITU 656 standard data */
                        pADV717x->PpiControl = 0x0086;
#elif defined (__ADSP_TETON__)
                        /* PPI output with ITU 656 standard data & DMA32 enabled */
                        pADV717x->PpiControl = 0x0186;
#elif defined (__ADSP_MOAB__)
                        /* EPPI DMA unpacking enabled, IFSGEN, ICLKGEN, BLANKGEN disabled, ITU Interlaced out */
                        pADV717x->PpiControl = 0x6810100E; 
#endif
                        UpdatePpiControl = 1;   /* update PPI/EPPI Control register */
                    }
                }

                break;

            /* CASE: (Configuring other ADV717x registers) */
            default:
                break;
        }
        /* IF (Update PPI/EPPI registers?) */
        if (((UpdatePpiLines) && (!pADV717x->Semaphores.Adv717xMode)) || (UpdatePpiControl))
        {
            /* IF (PPI device is already opened) */
            if (pADV717x->PpiHandle)
            {
#if defined (__ADSP_EDINBURGH__)    ||\
    defined (__ADSP_BRAEMAR__)      ||\
    defined (__ADSP_STIRLING__)     ||\
    defined (__ADSP_TETON__)        ||\
    defined (__ADSP_KOOKABURRA__)   ||\
    defined (__ADSP_MOCKINGBIRD__)
                /* IF (ADV717x Dataflow is enabled) */
                if(pADV717x->Semaphores.DataflowFlag)
                {
                    /* if dataflow is on, disable PPI dataflow before changing the video mode */
                    Result  = adi_dev_Control( pADV717x->PpiHandle, ADI_DEV_CMD_SET_DATAFLOW, (void*)FALSE );
                }
/* Debug build only */
#if defined(ADI_DEV_DEBUG)
                if (Result == ADI_DEV_RESULT_SUCCESS)
                {
#endif
                /* IF (Update PPI Control register?) */
                if (UpdatePpiControl)
                {
                    /* update the PPI control register */
                    Result = adi_dev_Control( pADV717x->PpiHandle, ADI_PPI_CMD_SET_CONTROL_REG, (void*)pADV717x->PpiControl );
                }
/* Debug build only */
#if defined(ADI_DEV_DEBUG)
                }
                if (Result == ADI_DEV_RESULT_SUCCESS)
                {
#endif
                /* IF (Update PPI Lines per Frame register?) */
                if ((UpdatePpiLines) && (!pADV717x->Semaphores.Adv717xMode))
                {
                    /* IF (PAL Video out) */
                    if (pADV717x->Semaphores.NtscPalFlag)
                    {
                        /* Update Lines per frame value for PAL */
                        pADV717x->PpiLinesPerFrame = ADI_ADV71x_LINES_PER_FRAME_PAL;
                    }
                    /* ELSE (NTSC Video out) */
                    else
                    {
                        /* Update Lines per frame value for NTSC */
                        pADV717x->PpiLinesPerFrame = ADI_ADV71x_LINES_PER_FRAME_NTSC;
                    }
                    /* update the PPI Frame lines count */
                    Result = adi_dev_Control( pADV717x->PpiHandle, ADI_PPI_CMD_SET_LINES_PER_FRAME_REG, (void*)pADV717x->PpiLinesPerFrame );
                }
/* Debug build only */
#if defined(ADI_DEV_DEBUG)
                }
                if (Result == ADI_DEV_RESULT_SUCCESS)
                {
#endif
                /* IF (PPI Dataflow was enabled) */
                if(pADV717x->Semaphores.DataflowFlag)
                {
                    /* if dataflow is previously on, re-enable the dataflow */
                    Result  = adi_dev_Control( pADV717x->PpiHandle, ADI_DEV_CMD_SET_DATAFLOW, (void*)TRUE );
                }
/* Debug build only */
#if defined(ADI_DEV_DEBUG)
                }
#endif

#elif defined (__ADSP_MOAB__)

/* Debug build only */
#if defined(ADI_DEV_DEBUG)
                if (Result == ADI_DEV_RESULT_SUCCESS)
                {
#endif
                /* IF (Update PPI Control register?) */
                if (UpdatePpiControl)
                {
                    /* update the PPI control register */
                    Result = adi_dev_Control( pADV717x->PpiHandle, ADI_EPPI_CMD_SET_CONTROL_REG, (void*)pADV717x->PpiControl );
                }
/* Debug build only */
#if defined(ADI_DEV_DEBUG)
                }
                if (Result == ADI_DEV_RESULT_SUCCESS)
                {
#endif
                /* IF (Update PPI Lines per Frame register?) */
                if ((UpdatePpiLines) && (!pADV717x->Semaphores.Adv717xMode))
                {
                    /* IF (PAL Video out) */
                    if (pADV717x->Semaphores.NtscPalFlag)
                    {
                        /* Update the EPPI Samples Per line Count */
                        pADV717x->PpiSamplesPerLine = ADI_ADV71x_SAMPLES_PER_LINE_PAL;
                        /* Update Lines per frame value for PAL */
                        pADV717x->PpiLinesPerFrame = ADI_ADV71x_LINES_PER_FRAME_PAL;
                    }
                    /* ELSE (NTSC Video out) */
                    else
                    {
                        /* Update the EPPI Samples Per line Count */
                        pADV717x->PpiSamplesPerLine = ADI_ADV71x_SAMPLES_PER_LINE_NTSC;                        
                        /* Update Lines per frame value for NTSC */
                        pADV717x->PpiLinesPerFrame = ADI_ADV71x_LINES_PER_FRAME_NTSC;
                    }
                    /* update the EPPI samples per line count */
                    Result = adi_dev_Control( pADV717x->PpiHandle, ADI_EPPI_CMD_SET_SAMPLES_PER_LINE, (void*)pADV717x->PpiSamplesPerLine );
/* Debug build only */
#if defined(ADI_DEV_DEBUG)
                if (Result == ADI_DEV_RESULT_SUCCESS)
                {
#endif
                    /* update the EPPI lines per frame count */
                    Result = adi_dev_Control( pADV717x->PpiHandle, ADI_EPPI_CMD_SET_LINES_PER_FRAME, (void*)pADV717x->PpiLinesPerFrame );
/* Debug build only */
#if defined(ADI_DEV_DEBUG)
                }
#endif
                }
/* Debug build only */
#if defined(ADI_DEV_DEBUG)
                }
#endif

#endif
            }
        }
    }
    
    return (Result); 
}

/* Debug build only */
#ifdef ADI_DEV_DEBUG

/*********************************************************************

 Function:  ValidatePDDHandle

 Description: Validates Physical Device Driver Handle

*********************************************************************/
static int ValidatePDDHandle(
    ADI_DEV_PDD_HANDLE      PDDHandle               /* Pointer to Physical Device Driver Handle */
){
    /* Return value - insure the client has provided a valid PDD Handle */
    u32     Result = ADI_DEV_RESULT_BAD_PDD_HANDLE;
    /* index */
    u32     i;
    
    /* compare the given PDD handle with PDDHandle allocated to all ADV717x devices in the list    */
    for (i = 0; i < ADI_ADV717x_NUM_DEVICES; i++) 
    {
        if (PDDHandle == (ADI_DEV_PDD_HANDLE)&Device[i])
        {
            /* Given PDDHandle is valid. quit this loop */
            Result = ADI_DEV_RESULT_SUCCESS;
            break;
        }
    }
    /* return */
    return (Result);
}

#endif /* Debug build only */

/*********************************************************************

Entry point for device manager

*********************************************************************/
#ifdef ADI_ADV7170_DEVICE
ADI_DEV_PDD_ENTRY_POINT ADIADV7170EntryPoint = {
#endif
#ifdef ADI_ADV7171_DEVICE
ADI_DEV_PDD_ENTRY_POINT ADIADV7171EntryPoint = {
#endif
#ifdef ADI_ADV7174_DEVICE
ADI_DEV_PDD_ENTRY_POINT ADIADV7174EntryPoint = {
#endif
#ifdef ADI_ADV7179_DEVICE
ADI_DEV_PDD_ENTRY_POINT ADIADV7179EntryPoint = {
#endif
    adi_pdd_Open,
    adi_pdd_Close,
    adi_pdd_Read,
    adi_pdd_Write,
    adi_pdd_Control,
    adi_pdd_SequentialIO
};


