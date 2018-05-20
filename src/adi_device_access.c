/**********************************************************************

Copyright(c) Analog Devices, Inc. All Rights Reserved. 

This software is proprietary and confidential.  By using this software 
you agree to the terms of the associated Analog Devices License Agreement.  

$RCSfile: adi_device_access.c,v $
$Revision: 2095 $
$Date: 2010-02-18 17:08:05 -0500 (Thu, 18 Feb 2010) $

Description:
    This is source code for 'Devices' capable of using 
    TWI or SPI to access its internal registers

Note: 
    'Device' refers to the underlying device driver 
    presently using this service
                
***********************************************************************/
/***********************************************************************
Check to insure that TWI driven or SPI driven or TWI and SPI driven 
file is being built
***********************************************************************/

#ifndef ADI_DEVICE_ACCESS_TWI       /* If none of the modes were/are defined */
#ifndef ADI_DEVICE_ACCESS_SPI

#define ADI_DEVICE_ACCESS_TWI       /* Build a file that supports both TWI and SPI device access */
#define ADI_DEVICE_ACCESS_SPI

#endif
#endif

/*********************************************************************

Include files

*********************************************************************/

#include <services/services.h>                      /* system service includes          */
#include <drivers/adi_dev.h>                        /* device manager includes          */
#include <drivers/deviceaccess/adi_device_access.h> /* adi_device_access driver includes*/

#if defined(ADI_DEVICE_ACCESS_TWI)                  /* Build for TWI based device access*/
#include <drivers/twi/adi_twi.h>                    /* TWI driver includes              */
#endif

#if defined(ADI_DEVICE_ACCESS_SPI)                  /* Build for SPI based device access*/
#include <drivers/spi/adi_spi.h>                    /* SPI driver includes              */
#endif

/*********************************************************************

Globals

*********************************************************************/
/* Device Access Semaphores */
typedef struct ADI_DEVICE_ACCESS_SEMAPHORES 
{
    u8  rwFlag;         /* Flag to indicate Read(1)/Write(0) operation */
/* TWI related semaphores */
#if defined(ADI_DEVICE_ACCESS_TWI)  
    u8  TwiAddrLsb;     /* TwiDataOut array location holding LSB of the 'Device' register address (possible values:0,1) */
    u8  TwiAddrMsb;     /* TwiDataOut array location holding MSB of the 'Device' register address (possible values:0,1) */
    u8  TwiDoutLsb;     /* TwiDataOut array location holding LSB of the 'Device' register value (possible values:1,2,3) */
    u8  TwiDoutMsb;     /* TwiDataOut array location holding MSB of the 'Device' register value (possible values:1,2,3))*/
    u8  TwiDinLsb;      /* TwiDataIn array location holding LSB of the 'Device' register value (possible values:0,1)    */
    u8  TwiDinMsb;      /* TwiDataIn array location holding MSB of the 'Device' register value (possible values:0,1))   */  
#endif
#if defined(ADI_DEVICE_ACCESS_SPI)  
/* SPI related semaphores */
    u8  SpiGaddrLsb;    /* SpiPacket array location holding LSB of 'Device' global address (possible values:0,1)        */
    u8  SpiGaddrMsb;    /* SpiPacket array location holding MSB of 'Device' global address (possible values:0,1)        */
    u8  SpiAddrLsb;     /* SpiPacket array location holding LSB of 'Device' register address (possible values:0,1,2,3)  */
    u8  SpiAddrMsb;     /* SpiPacket array location holding MSB of 'Device' register address (possible values:0,1,2,3)  */
    u8  SpiDataLsb;     /* SpiPacket array location holding LSB of 'Device' register value (possible values:1,2,3,4,5)  */
    u8  SpiDataMsb;     /* SpiPacket array location holding MSB of 'Device' register value (possible values:1,2,3,4,5)  */
#endif
} ADI_DEVICE_ACCESS_SEMAPHORES;

/*********************************************************************

Static functions

*********************************************************************/

/* performs register mask operation for regiester field access */
static void MaskRegField (
    ADI_DEVICE_ACCESS_REGISTERS     *Device,            /* pointer to the 'Device' structure                    */
    ADI_DEVICE_ACCESS_SEMAPHORES    *Semaphores,        /* Flag to indicate Read / Write operation              */
    u16                             RegAddr,            /* Present 'Device' register address being accessed     */
    u16                             RegField,           /* Reg Field Location of the 'Device' to accessed       */
    u16                             *RegValue           /* Value of 'Device' register being accessed            */
);

/* Generates next address for block read / write operation */
static void GenerateAddress(
    ADI_DEVICE_ACCESS_REGISTERS     *Device,            /* pointer to the 'Device' structure                    */
    ADI_DEVICE_ACCESS_SEMAPHORES    *Semaphores,        /* Flag to indicate Read / Write operation              */
    u16                             *RegAddr            /* Present 'Device' register address being accessed     */
);

/* Updates reserved bit locations with the values recommended in the 'Device' manual */
static void UpdateReservedBits(
    ADI_DEVICE_ACCESS_REGISTERS     *Device,            /* pointer to the 'Device' structure                    */
    u16                             RegAddr,            /* Present 'Device' register address to be configured   */
    u16                             *RegValue           /* Value of 'Device' reg address to be configured       */
);

#if defined(ADI_DEVICE_ACCESS_TWI)

static u32 TwiOpen(                                     /* Open TWI device (driver)                             */
    ADI_DEVICE_ACCESS_REGISTERS     *Device             /* pointer to the 'Device' structure                    */
);

static u32  TwiAccess (                                 /* Access 'Device' registers via TWI                    */
    ADI_DEVICE_ACCESS_REGISTERS     *Device,            /* pointer to the 'Device' table                        */
    ADI_DEV_SEQ_1D_BUFFER           *TwiDataOutBuf,     /* Sequential buffer holding TWI data                   */
    ADI_DEV_SEQ_1D_BUFFER           *TwiDataInitBuf,    /* Sequential buffer holding TWI data                   */
    ADI_DEV_SEQ_1D_BUFFER           *TwiDataInBuf,      /* Sequential buffer holding TWI data                   */
    ADI_DEVICE_ACCESS_SEMAPHORES    *Semaphores,        /* Device access semaphores                             */
    u16                             RegAddr,            /* 'Device' register address to be accessed             */
    u16                             *RegData            /* 'Device' register data                               */
);

static void TwiCallbackFunction(                        /* TWI Callback function                                */
    void*   DeviceHandle, 
    u32     Event, 
    void*   pArg
);

static  ADI_DEV_PDD_HANDLE  TwiHandle = NULL;   /* Handle to TWI device (driver) */

/* This configuration is fixed for TWI Device access service */
static ADI_DEV_CMD_VALUE_PAIR DevAccessTWIConfig[] = {
    { ADI_DEV_CMD_SET_DATAFLOW_METHOD,  (void *)ADI_DEV_MODE_SEQ_CHAINED    },  /* Dataflow is always Seq IO    */
    { ADI_DEV_CMD_SET_DATAFLOW,         (void *)TRUE                        },  /* Enable TWI Dataflow          */
    { ADI_DEV_CMD_END,                  (void *)NULL                        }   /* End of command table         */
};
    
#endif

#if defined(ADI_DEVICE_ACCESS_SPI)


static u32 SpiOpen(                                     /* Open SPI device (driver)                             */
    ADI_DEVICE_ACCESS_REGISTERS     *Device             /* pointer to the 'Device' structure                    */
);

static u32 SpiAccess (                                  /* Access 'Device' registers via SPI                    */
    ADI_DEVICE_ACCESS_REGISTERS     *Device,            /* pointer to the 'Device' table                        */
    ADI_DEV_1D_BUFFER               *SpiWriteBuf,       /* 1D buffer for SPI write operation                    */
    ADI_DEV_1D_BUFFER               *SpiReadBuf,        /* 1D buffer for SPI read operation                     */
    ADI_DEVICE_ACCESS_SEMAPHORES    *Semaphores,        /* Device access semaphores                             */
    u16                             RegAddr,            /* 'Device' register address to be accessed             */
    u16                             *RegData            /* 'Device' register data                               */
);

static u32 SpiSlaveAccess ( 
    ADI_DEVICE_ACCESS_REGISTERS     *Device,            /* pointer to the 'Device' table                        */
    ADI_DEVICE_ACCESS_SEMAPHORES    *Semaphores         /* Device access Semaphores                             */
) ;

/* the callback function passed to the SPI driver */
static void SpiCallbackFunction(
    void*   DeviceHandle, 
    u32     Event, 
    void*   pArg
);

static volatile int SpiCompleteFlag = 0;                /* semaphore used to monitor SPI IO                     */
static  ADI_DEV_PDD_HANDLE  SpiHandle = NULL;           /* Handle to SPI device (driver)                        */

/* Default SPI configuration for Device Access. 
'Device' can change these settings by passing its own SPI configuration table                                   */
static ADI_DEV_CMD_VALUE_PAIR DefaultSPIConfig[] = {        
    { ADI_SPI_CMD_SET_BAUD_REG,         (void *)0x7FF                   },  /* baud rate                        */
    { ADI_SPI_CMD_SET_CLOCK_POLARITY,   (void *)1                       },  /* active low SCK                   */
    { ADI_DEV_CMD_END,                  (void *)NULL                    }
};

/* This SPI configuration is fixed for SPI Device access service */
static ADI_DEV_CMD_VALUE_PAIR DevAccessSPIConfig[] = {
    { ADI_DEV_CMD_SET_DATAFLOW_METHOD,  (void *)ADI_DEV_MODE_CHAINED    },
    { ADI_SPI_CMD_SET_MASTER,           (void *)TRUE                    },  /* Blackfin is always Master        */
    { ADI_SPI_CMD_SET_WORD_SIZE,        (void *)8                       },  /* SPI wordlength is fixed as 8-bit */
    { ADI_SPI_CMD_SET_CLOCK_PHASE,      (void *)1                       },  /* Software chip select             */
    { ADI_DEV_CMD_END,                  (void *)NULL                    }
};

#endif

/*********************************************************************

Debug Mode functions (debug build only)
 
*********************************************************************/
#if defined(ADI_DEV_DEBUG)

/* Validate Register address / register field / register field value */
static u32 ValidateDeviceAccess (
    ADI_DEVICE_ACCESS_REGISTERS *Device,    /* Pointer to the 'Device' table                                    */
    u16                         RegAddr,    /* Register address to be accessed                                  */
    u16                         RegValue,   /* Value of Reg address or Reg field location to be accessed        */
    u16                         RegField    /* Register field locati on to be accessed                          */
);
#endif

#if defined(ADI_DEVICE_ACCESS_TWI)
/*********************************************************************
*
* Function:  TwiOpen
*
* Description: Opens the TWI device to access 'Device' registers
*
*********************************************************************/
static u32 TwiOpen( 
    ADI_DEVICE_ACCESS_REGISTERS     *Device     /* pointer to the 'Device' structure    */
){    
    u32 Result = ADI_DEV_RESULT_SUCCESS;        /* default return code - assume success */
    ADI_DEV_CMD_VALUE_PAIR  *pPair;             /* pointer to command pair              */
        
    /* Open TWI device (driver) */
    if ((Result = adi_dev_Open( 
                        Device->ManagerHandle,          /* device manager handle    */
                        &ADITWIEntryPoint,              /* TWI Entry point          */
                        Device->DeviceNumber,           /* TWI device number        */
                        Device,                         /* client handle - passed to internal callback function */
                        &TwiHandle,                     /* pointer to DM handle (for TWI driver) location       */
                        ADI_DEV_DIRECTION_BIDIRECTIONAL,/* Bidirectional data transfer  */
                        NULL,                           /* No DMA                       */
                        Device->DCBHandle,              /* handle to the callback manager   */
                        TwiCallbackFunction             /* internal callback function       */
        )) == ADI_DEV_RESULT_SUCCESS)
    {
        /* Continue only if the TWI driver open is a success */
        /* 'Device' must pass a TWI Configuration table */
        /* Check if 'Device' has a Configuration table for TWI driver */
        if (Device->ConfigTable!=NULL) 
        {
            /* Pass the configuration table to TWI driver */
            if ((Result = adi_dev_Control(TwiHandle,ADI_DEV_CMD_TABLE,(void *)Device->ConfigTable)) == ADI_DEV_RESULT_SUCCESS)
            {
                /* 'Device' specific TWI configuration is a success */          
                /* Configure TWI driver with TWI Device access service configuration table  */
                Result = adi_dev_Control(TwiHandle, ADI_DEV_CMD_TABLE, (void*)DevAccessTWIConfig );
            }
        }
        else 
        {   
            /* pass error indicating application to pass a TWI configuration table */
            Result = ADI_DEV_RESULT_REQUIRES_TWI_CONFIG_TABLE;
        }
    }
    /* Return */
    return (Result);        
}

/*********************************************************************
*
* Function:  TwiAccess
*
* Description: Access 'Device' registers via TWI
*
*********************************************************************/
static u32  TwiAccess ( 
    ADI_DEVICE_ACCESS_REGISTERS     *Device,            /* pointer to the 'Device' table            */
    ADI_DEV_SEQ_1D_BUFFER           *TwiDataOutBuf,     /* Sequential buffer holding TWI data       */
    ADI_DEV_SEQ_1D_BUFFER           *TwiDataInitBuf,    /* Sequential buffer holding TWI data       */
    ADI_DEV_SEQ_1D_BUFFER           *TwiDataInBuf,      /* Sequential buffer holding TWI data       */
    ADI_DEVICE_ACCESS_SEMAPHORES    *Semaphores,        /* Device access semaphores                 */
    u16                             RegAddr,            /* 'Device' register address to be accessed */
    u16                             *RegData            /* 'Device' register data                   */
){
    u32 Result = ADI_DEV_RESULT_SUCCESS;/* default return code - assume success     */
    u8  TwiDataOut[4]   = {0};          /* Data to be sent to the Device using TWI  */
    u8  TwiDataIn[2]    = {0};          /* Data read from the Device using TWI      */
    u32 count = 0;                      /* count to limit TWI access time & save processor from deadlock */
 
    TwiDataOut[Semaphores->TwiAddrLsb] = (u8)RegAddr;       /* Load LSB of the 'Device' register address */
    TwiDataOut[Semaphores->TwiAddrMsb] = (u8)(RegAddr>>8);  /* Load MSB of the 'Device' register address */
    
    /* Open TWI to access the 'Device' */
    if ((Result = TwiOpen(Device)) == ADI_DEV_RESULT_SUCCESS)
    {       
        /* check the read/write flag */
        if (Semaphores->rwFlag)
        {
            /* Perform a TWI read */
            TwiDataInitBuf->Buffer.Data             = &TwiDataOut[0];
            TwiDataInBuf->Buffer.Data               = &TwiDataIn[0];
            TwiDataInitBuf->Buffer.pAdditionalInfo  = (void *)Device->DeviceAddress;    
            TwiDataInBuf->Buffer.ProcessedFlag      = 0; /* Clear Processed flag for next TWI operation */

            /* submit the read request */
            if ((Result = adi_dev_SequentialIO(TwiHandle,ADI_DEV_SEQ_1D,
                                              (ADI_DEV_BUFFER *) TwiDataInitBuf )) == ADI_DEV_RESULT_SUCCESS)
            {
                /* wait for the last in the chain buffer to be processed */
                while (!TwiDataInBuf->Buffer.ProcessedFlag)
                {
                    /* wait untill master finishes reading the data */
                    /* run a count to check if TWI is locked in any other process   */
                    count++;
                    if (count == 0xFFFFF) 
                    {
                        /* TWI device is locked in other operation. return failure */
                        Result = ADI_DEV_RESULT_TWI_LOCKED;
                        break;
                    }
                }
                
                if (Result == ADI_DEV_RESULT_SUCCESS)
                {
                    /* check if the TWI read buffer is processed */
                    if (TwiDataInBuf->Buffer.ProcessedFlag != ADI_DEV_EVENT_BUFFER_PROCESSED)
                    {
                        Result = ADI_DEV_RESULT_FAILED; /* return failure */
                    }
                    else
                    {
                        /* Load the data read from the 'Device' to RegData */
                        *RegData = ((((u16)TwiDataIn[Semaphores->TwiDinMsb])<<8) | TwiDataIn[Semaphores->TwiDinLsb]);
                    }
                }
            }
        }
        else    /* Perform a TWI write */
        {
            TwiDataOutBuf->Buffer.Data              = &TwiDataOut[0];
            /* Load the 'Device' TWI address in pAdditionalInfo of the Sequential Buffers */
            TwiDataOutBuf->Buffer.pAdditionalInfo   = (void *)Device->DeviceAddress;
            /* Clear process flag for next operation */
            TwiDataOutBuf->Buffer.ProcessedFlag     = 0; 

            /* set the reserved bits back to recommended values */
            UpdateReservedBits(Device,RegAddr,RegData);
            /* Load LSB of the 'Device' register data */
            TwiDataOut[Semaphores->TwiDoutLsb] = (u8)(*RegData);
            /* Load MSB of the 'Device' register data */
            TwiDataOut[Semaphores->TwiDoutMsb] = (u8)((*RegData)>>8);

            /* submit the write request */
            if ((Result = adi_dev_SequentialIO (TwiHandle,ADI_DEV_SEQ_1D,
                                                (ADI_DEV_BUFFER *) TwiDataOutBuf)) == ADI_DEV_RESULT_SUCCESS)
            {
                /* wait for the last in the chain buffer to be processed */
                while (!TwiDataOutBuf->Buffer.ProcessedFlag)
                {
                    /* wait till master finish sending data */
                    /* run a count to check if TWI is locked in any other process */
                    count++;
                    if (count == 0xFFFFF) 
                    {
                        /* TWI device is locked in other operation. return failure */
                        Result = ADI_DEV_RESULT_TWI_LOCKED;
                        break;
                    }
                }
                
                if (Result == ADI_DEV_RESULT_SUCCESS)
                {
                    /* check if the TWI write buffer is processed */
                    if (TwiDataOutBuf->Buffer.ProcessedFlag != ADI_DEV_EVENT_BUFFER_PROCESSED)
                    {
                        Result = ADI_DEV_RESULT_FAILED; /* return failure */
                    }
                }
            }
        }
        adi_dev_Close(TwiHandle);       /* Close the TWI device opened for use */
    }
    
    /* return */
    return (Result);
}

#endif

#if defined(ADI_DEVICE_ACCESS_SPI)
/*********************************************************************
*
*   Function:       SpiOpen
*
*   Description:    Opens the SPI device to access 'Device' registers
*
*********************************************************************/
static u32 SpiOpen( 
    ADI_DEVICE_ACCESS_REGISTERS     *Device /* pointer to the 'Device' structure */
) {
  
    u32 Result = ADI_DEV_RESULT_SUCCESS;    /* default return code - assume success     */
    
    /* Open SPI device (driver) */
    if ((Result = adi_dev_Open(
                        Device->ManagerHandle,          /* device manager handle                                */
                        &ADISPIIntEntryPoint,           /* spi Entry point                                      */
                        Device->DeviceNumber,           /* spi device number                                    */
                        Device,                         /* client handle -                                      */
                        &SpiHandle,                     /* pointer to DM handle (for SPI driver) location       */
                        ADI_DEV_DIRECTION_BIDIRECTIONAL,/* Bidirectional data transfer                          */
                        NULL,                           /* No DMA                                               */
                        Device->DCBHandle,              /* handle to the callback manager                       */
                        SpiCallbackFunction             /* internal callback function                           */
        )) == ADI_DEV_RESULT_SUCCESS)
     {
        /* Configure SPI driver with default configuration table */
        if ((Result = adi_dev_Control(SpiHandle,ADI_DEV_CMD_TABLE,
                                     (void*)DefaultSPIConfig )) == ADI_DEV_RESULT_SUCCESS)
        {
            /* Check if 'Device' has a Configuration table for SPI driver */
            if (Device->ConfigTable!=NULL) 
            {
                /* Pass the configuration table to SPI driver */
                Result = adi_dev_Control(SpiHandle,ADI_DEV_CMD_TABLE,(void *)Device->ConfigTable);
            }
        }
                
        if (Result == ADI_DEV_RESULT_SUCCESS)
        {
            /* Configure SPI driver with SPI Device access service configuration table */
            Result = adi_dev_Control(SpiHandle, ADI_DEV_CMD_TABLE, (void*)DevAccessSPIConfig );
        }
    }
    /* Return */
    return (Result);
}

/*********************************************************************
*
* Function:  SpiAccess
*
* Description: Access 'Device' registers via SPI
*
*********************************************************************/
static u32 SpiAccess ( 
    ADI_DEVICE_ACCESS_REGISTERS     *Device,        /* pointer to the 'Device' table            */
    ADI_DEV_1D_BUFFER               *SpiWriteBuf,   /* 1D buffer for SPI write operation        */
    ADI_DEV_1D_BUFFER               *SpiReadBuf,    /* 1D buffer for SPI read operation         */
    ADI_DEVICE_ACCESS_SEMAPHORES    *Semaphores,    /* Device access Semaphores                 */
    u16                             RegAddr,        /* 'Device' register address to be accessed */
    u16                             *RegData        /* 'Device' register data                   */
) {
    u32     Result = ADI_DEV_RESULT_SUCCESS;/* default return code - assume success     */
    u16     u16Temp;                        /* a Temp location */
    u8      SpiPacket[6] = { 0 };           /* SPI write data packet */

    SpiWriteBuf->Data   = &SpiPacket[0];    /* SpiPacket holds SPI read/write data */
    SpiReadBuf->Data    = &SpiPacket[0];
        
    /* Load 'Device' SPI Global address LSB */
    SpiPacket[Semaphores->SpiGaddrLsb]  = (u8) Device->DeviceAddress;
    /* Load 'Device' SPI Global address MSB */
    SpiPacket[Semaphores->SpiGaddrMsb]  = (u8) (Device->DeviceAddress>>8);   
    /* Load LSB of 'Device' register address */
    SpiPacket[Semaphores->SpiAddrLsb]   = (u8)RegAddr;
    /* Load MSB of 'Device' register address */
    SpiPacket[Semaphores->SpiAddrMsb]   = (u8)(RegAddr>>8);    

    SpiWriteBuf->ProcessedFlag          = FALSE;
    SpiWriteBuf->pNext                  = NULL;

    /* if the selected operation is SPI read */
    if (Semaphores->rwFlag) 
    {
        SpiReadBuf->ProcessedFlag = FALSE;
        SpiReadBuf->pNext = NULL;
        SpiReadBuf->CallbackParameter = &SpiReadBuf;
        SpiWriteBuf->CallbackParameter = NULL;    
    }
    else    /* SPI write */
    {
        /* set the reserved bits back to recommended values */
        UpdateReservedBits ( Device,RegAddr,RegData );
        SpiWriteBuf->CallbackParameter = &SpiWriteBuf;
        /* if this 'Device' is not of a special case */
        if (Device->SelectAccess->Rdata_len)
        {
            /* Load LSB of 'Device' register data */
            SpiPacket[Semaphores->SpiDataLsb] = (u8)*RegData;
            /* Load MSB of 'Device' register data */
            SpiPacket[Semaphores->SpiDataMsb] = (u8)(*RegData>>8);
        }
    }

    /* if the 'Device' is not of a special case */
    if (Device->SelectAccess->Rdata_len)
    {
        /* if 'Device' Global address is 2 bytes or 
           'Device has no Global address and its register address of 2bytes */
        if ((Device->SelectAccess->Gaddr_len == ADI_DEVICE_ACCESS_LENGTH2) ||
            ((Device->SelectAccess->Gaddr_len == ADI_DEVICE_ACCESS_LENGTH0) 
              && (Device->SelectAccess->Raddr_len == ADI_DEVICE_ACCESS_LENGTH2)))
        {
            /* get the complete register address */
            u16Temp = (SpiPacket[1] | ((u16)SpiPacket[0])<<8);
            /* IF (device R/W flag is at bit 0) */
            if (Device->SelectAccess->AccessType == ADI_DEVICE_ACCESS_TYPE_SPI)
            {
                u16Temp <<= 1;  /* shift it by 1 to append R/W bit */
            }
            /* IF(this is SPI a read) */
            if (Semaphores->rwFlag)
            {
                /* indicate 'Device' as register read operation */
                u16Temp |= ((u16)Device->SelectAccess->AccessType);
            }
            /* ELSE (SPI write) */
            else
            {
                /* clear R/W bit to indicate this as write access */
                u16Temp &= ((u16)(~Device->SelectAccess->AccessType));
            }    
            /* split the register address as two 8bit chunks */    
            SpiPacket[0] = (u8) (u16Temp>>8);
            SpiPacket[1] = (u8) u16Temp;
        }
        /* else if the 'Device' has 8 bit global/register address */
        else
        {
            /* Append Read/Write bit to the first SPI data packet */
            /* IF (device R/W flag is at bit 0) */
            if (Device->SelectAccess->AccessType == ADI_DEVICE_ACCESS_TYPE_SPI)
            {
                SpiPacket[0] <<= 1; /* shift it by 1 to append R/W bit */
            }
            if (Semaphores->rwFlag) /* SPI Read operation */
            {
                /* indicate 'Device' as register read operation */
                SpiPacket[0] |= ((u8)Device->SelectAccess->AccessType);
            }
            /* ELSE (SPI write) */
            else
            {
                /* clear R/W bit to indicate this as write access */
                SpiPacket[0] &= ((u8)(~Device->SelectAccess->AccessType));
            }  
        }
    }
    
    /* Open SPI to access the 'Device' */
    if ((Result = SpiOpen(Device)) == ADI_DEV_RESULT_SUCCESS)
    {
        /* Submit the output buffer to the SPI device driver */
        if ((Result = adi_dev_Write(SpiHandle, ADI_DEV_1D, 
                                   (ADI_DEV_BUFFER *)SpiWriteBuf)) == ADI_DEV_RESULT_SUCCESS)
        {            
            /* if the selected operation is SPI read */
            if (Semaphores->rwFlag)
            {
                /* Submit the input buffer to the SPI device driver */
                Result = adi_dev_Read(SpiHandle, ADI_DEV_1D, (ADI_DEV_BUFFER *)SpiReadBuf);
            }
        }
        
        /* Continue SPI access only if read/write buffer submission is success */
        if (Result == ADI_DEV_RESULT_SUCCESS)
        {
            /* Access the SPI 'Device' */
            if ((Result = SpiSlaveAccess (Device,Semaphores )) == ADI_DEV_RESULT_SUCCESS) 
            {
                /* if the selected operation is SPI read */
                if (Semaphores->rwFlag) 
                {
                    /* update the RegData */
                    *RegData = ((((u16)SpiPacket[Semaphores->SpiDataMsb])<<8) | SpiPacket[Semaphores->SpiDataLsb]);
                }
            }
        }
    } else {
        // open was unsuccessful... no need to close.
        // also, better to return the open failure than a close failure of an unopened device
        return Result;
    }
    
    /* close spi driver */
    Result = adi_dev_Close(SpiHandle); 
    
    /* return */
    return(Result);
}

/*********************************************************************
*
* Function:  SpiSlaveAccess
*
* Description: Access the SPI slave 'Device'
*
*********************************************************************/
static u32 SpiSlaveAccess ( 
    ADI_DEVICE_ACCESS_REGISTERS     *Device,            /* pointer to the 'Device' table    */
    ADI_DEVICE_ACCESS_SEMAPHORES    *Semaphores         /* Device access Semaphores         */
) {

    u32     Result = ADI_DEV_RESULT_SUCCESS;    /* default return code - assume success     */

    /* Activate 'Device' SPI chip select line */
    if ((Result = adi_dev_Control(SpiHandle, ADI_SPI_CMD_ENABLE_SLAVE_SELECT, 
                                  (void*)Device->SelectAccess->DeviceCS )) == ADI_DEV_RESULT_SUCCESS) 
    {
        /* drive 'Device' chipselect line to low (CLATCH) */
        if ((Result = adi_dev_Control(SpiHandle, ADI_SPI_CMD_SELECT_SLAVE, 
                                      (void*)Device->SelectAccess->DeviceCS )) == ADI_DEV_RESULT_SUCCESS)
        {
            /* Enable SPI data flow */
            if ((Result = adi_dev_Control(SpiHandle, ADI_DEV_CMD_SET_DATAFLOW, 
                                          (void *)TRUE)) == ADI_DEV_RESULT_SUCCESS)
            {
                /* wait untill SPI device is done with 'Device' read/write operation */
                /* SPI Callback function will set SpiCompleteFlag to one */
                while(!SpiCompleteFlag)
                {
                    asm("nop;");
                }

                SpiCompleteFlag = 0;    /* clear the semaphore for next SPI access */

                /* Disable SPI data flow */
                if ((Result = adi_dev_Control(SpiHandle, ADI_DEV_CMD_SET_DATAFLOW, 
                                              (void *)FALSE)) == ADI_DEV_RESULT_SUCCESS) 
                {                    
                    /* Deactivate 'Device' SPI chip select line */
                    Result = adi_dev_Control(SpiHandle, ADI_SPI_CMD_DISABLE_SLAVE_SELECT, 
                                            (void*)Device->SelectAccess->DeviceCS );
                }
            }
        }
    }
    /* return */
    return (Result);
}

#endif

/*********************************************************************
*
* Function:  MaskRegfield
*
* Description: for 'Device' register field access
* Write mode => Masks other register fields and updates 
*               the corresponding register field with new value
*
* Read mode =>  Masks corresponding register fields and clears 
*               other register field values
*
*********************************************************************/
static void MaskRegField ( 
    ADI_DEVICE_ACCESS_REGISTERS     *Device,        /* pointer to the 'Device' table                            */
    ADI_DEVICE_ACCESS_SEMAPHORES    *Semaphores,    /* Flag to indicate Read / Write operation                  */
    u16                             RegAddr,        /* Present 'Device' register address being accessed         */
    u16                             RegField,       /* Register Field Location of the 'Device' being accessed   */
    u16                             *RegValue       /* Value of 'Device' register being accessed                 */
) {     

    /* Temp locations */
    u16 ShiftCount,RegTable,i,FieldLen,MaxShiftCount,MaxMaskValue;

    /* This function call is valid only when the 'Device' has a RegisterField table */
    if (Device->RegisterField != NULL)
    {
        /* Generate Maximum Shift count value and Mask value */
        if (Device->SelectAccess->Rdata_len == ADI_DEVICE_ACCESS_LENGTH2)
        {
            /* Datalength is of 2 bytes (16 bits) */
            MaxShiftCount   = 15;
            MaxMaskValue    = 0xFFFF;
        }
        else
        {
            /* Data length must be 1 (8 bits) */
            MaxShiftCount   = 7;
            MaxMaskValue    = 0xFF;
        }
    
        i = 0;
        
        /* check all Register addresses containing individual fields */
        while (i<Device->RegisterField->Count) 
        {
            /* If the register entry is found in the table, check for a valid register field */
            if (RegAddr == *(Device->RegisterField->RegAddr+i)) /* get the register address */
            {
                /* Register address match found. Get the corresponding register field to access */
                ShiftCount = RegField;

                /* load the corresponding 'Device' register field locations from the table */
                RegTable = *(Device->RegisterField->RegField+i);
                /* shift the value to number of shift count+1 */
                RegTable >>= ShiftCount+1;
                FieldLen = 1; /* by default, register field value will be atleast 1 bit */

                /* get the register field length (shift untill bit 0 of RegField = 1) */
                while ((!(RegTable & 0x01)) && (ShiftCount<MaxShiftCount)) 
                {
                    FieldLen++;     /* increment register field length */
                    RegTable >>= 1; /* move to next bit */
                    ShiftCount++;   /* increment shift count */
                }
                break;  /* found a register match */            
            }
            else 
            {
                i++;   /* move to next reg address in the table */
            }
        }
        
        /* Do nothing if this register is not listed in the RegisterField table */
        if (i<Device->RegisterField->Count)
        {
            /* this register is listed in RegisterField table */            
            /* Generate Mask for other register fields */
            i = 0;   /* 'i' will hold the final mask value */
            /* check if any data shifting is required */
            /* (ie., register field location to uptade doesnot start at bit 0) */
            if (RegField)
            {
                /* shift register field times to mask bits after the register field to be accessed */
                i = MaxMaskValue >> ((MaxShiftCount+1) - RegField); 
            }
            /* Mask bits present before the register field to be accessed */
            i = i | (MaxMaskValue << (RegField + FieldLen));

            /* is the selected operation is read? */
            if (Semaphores->rwFlag)
            {
                /* Perform mask operation (clears other register field values) */
                *RegValue = *RegValue & ~i;
            }
            else
            {
                /* Perform mask operation (clears present values of register field to be accessed) */
                *RegValue = *RegValue & i;
            }
        }
    }
    /* return */
    return;
}

/*********************************************************************
*
* Function:  GenerateAddress
*
* Description: Generates next address for block read / write operation
*
*********************************************************************/
static void GenerateAddress(
    ADI_DEVICE_ACCESS_REGISTERS     *Device,        /* pointer to the 'Device' table */
    ADI_DEVICE_ACCESS_SEMAPHORES    *Semaphores,    /* Flag to indicate Read / Write operation */
    u16                             *RegAddr        /* Present 'Device' register address being accessed */
) {

    u16     i,j;
    u8      loop;
    
    ++(*RegAddr);  /* Next 'Device' address to access */

    /* Continue only if the 'Device' has a ValidateRegister table */
    if (Device->ValidateRegister != NULL)
    {    
        loop = 1;    /* Loop flag for register address validation */
        /* Validate the new address */
        while(loop)
        {
            /* reset counters */
            i = 0;
            j = 0;
            /* if ('Device' has a list of Invalid Registers) */
            if (Device->ValidateRegister->InvalidRegs != NULL)
            {
                /* Compare the generated address with the listed Invalid Register addresses */
                while (i<Device->ValidateRegister->Count1)
                {
                    /* if (this register address is listed as Invalid register) */
                    if (*RegAddr == *(Device->ValidateRegister->InvalidRegs+i)) 
                    {
                        ++(*RegAddr);  /* if its an invalid loaction, move to the next. */
                    }
                    /* move to next invalid address in the list */
                    i++;
                }
            }
            loop = 0;   /* assume register address validation is done */
            /* if (this is a block write operation & if the 'Device' has a list of Read only Registers) */
            if ((!Semaphores->rwFlag) && (Device->ValidateRegister->ReadOnlyRegs != NULL))
            {
                /* Check if the present address to be accessed is a read-only location */
                while (j<Device->ValidateRegister->Count2) 
                { 
                    /* if (this register address is listed as Read only register) */
                    if (*RegAddr == *(Device->ValidateRegister->ReadOnlyRegs+j)) 
                    {                   
                        ++(*RegAddr);   /* if its a Read-only loaction, move to the next */
                        loop = 1;       /* continue register validation */
                    }
                    /* move to next read-only address in the list */
                    j++;
                }
            }
        }
    }
}

/*********************************************************************
*
* Function:  UpdateReservedBits
*
* Description:  Updates reserved bit locations with the values 
*    recommended in the 'Device' manual
*
*********************************************************************/
static void UpdateReservedBits(
    ADI_DEVICE_ACCESS_REGISTERS *Device,    /* pointer to the 'Device' table */
    u16                         RegAddr,    /* Present 'Device' register address to be configured */
    u16                         *RegValue   /* Value of the corresponding 'Device' register address to be configured */
) {
    u32 i;
    
    /* This function call is valid only when the 'Device' has a valid 'ReservedValues' and 'ReservedBitValue' table */
    if ((Device->ReservedValues != NULL) && (Device->ReservedValues->ReservedBitValue !=NULL))
    {
        /* Look for the register address in the 'ReservedValues' table */
        for (i=0;i<Device->ReservedValues->Count;i++)
        {
            if (RegAddr == *(Device->ReservedValues->RegAddr+i))
            {
                /* Update the reserved bit with its recommended value
                   BJ - added & j so we only add reserved default values, this makes it 
                   easier to create the reserved defaults table because you can use
                   the default value for the register. */
                /* Masks & Clears reserved bit locations */
                *RegValue = ((*RegValue) & (~(*(Device->ReservedValues->ReservedBits+i))));
                /* Update RegValue with default reserved bit values */
                *RegValue = (*RegValue)| ((*(Device->ReservedValues->ReservedBitValue+i)) & (*(Device->ReservedValues->ReservedBits+i)));
            }
        }
    }
}

/*********************************************************************
*
* Function:  adi_device_access
*
* Description: Access Device registers via TWI or SPI
*
*********************************************************************/
    
u32 adi_device_access (
    ADI_DEVICE_ACCESS_REGISTERS     *Device  /* pointer to the 'Device' table */
) {

    u32     Result = ADI_DEV_RESULT_SUCCESS;/* default return code - assume success     */
    /* holds the value of remaining number of Device registers to be accessed */
    u16     regcount;
    /* locations for internal data processing */
    u16     RegAddr, RegData;
    
    /* Create a semphore structure */
    ADI_DEVICE_ACCESS_SEMAPHORES    Semaphores;
    /* Access mode structure */
    ADI_DEVICE_ACCESS_MODE          AccessMode;
 
    /* holds address of application program table location being accessed */
    ADI_DEV_ACCESS_REGISTER         *AccessSelective;
    ADI_DEV_ACCESS_REGISTER_BLOCK   *AccessBlock;
    ADI_DEV_ACCESS_REGISTER_FIELD   *AccessField;

/* TWI related registers */
#if defined(ADI_DEVICE_ACCESS_TWI)
    
/*************************************
Seq. buffer to send data to the Device
*************************************/

ADI_DEV_SEQ_1D_BUFFER  TwiDataOutBuf = {
    {
        { 0 },          /* reserved[] */
        NULL,           /* location holding data to be sent to the Device using TWI */
        2,              /* data count (default as 2 (8bit register address & 8bit register value)) */
        1,              /* data width */
        0,              /* callback argument */
        0,              /* processed flag */
        0,              /* processed element count */
        NULL,           /* next buffer address */
        (void *)NULL    /* TWI address of the Device to be accesed as additional info */
    },
    ADI_DEV_DIRECTION_OUTBOUND  /* TWI data direction */
};

/*****************************************
Seq. buffers to read data from the Device
*****************************************/

ADI_DEV_SEQ_1D_BUFFER  TwiDataInBuf = {
    {
        { 0 },              /* reserved[] */
        NULL,               /* location to hold data read from the Device using TWI */
        1,                  /* data count (default as 1 (read 8bit value of a register) */
        1,                  /* data width */
        0,                  /* callback argument */
        0,                  /* processed flag */
        0,                  /* processed element count */
        NULL,               /* next buffer address */
        (void *)ADI_TWI_RSTART   /* Restart to read the data */
    },
    ADI_DEV_DIRECTION_INBOUND
};

 ADI_DEV_SEQ_1D_BUFFER  TwiDataInitBuf = {    /* Buffer to initiate read from 'Device' using TWI */
    {
        { 0 },                  /* reserved[] */
        NULL,                   /* location holding data to be sent to the Device using TWI */
        1,                      /* data count (default as 1 (Subregister address of 8bit length) */
        1,                      /* data width */
        0,                      /* callback argument */
        0,                      /* processed flag */
        0,                      /* processed element count */
        &TwiDataInBuf.Buffer,   /* next buffer address */
        (void *)NULL            /* TWI address of the Device to be accesed as additional info */
    },
    ADI_DEV_DIRECTION_OUTBOUND
};

    /* TWI Device address LSB */
    Semaphores.TwiAddrLsb   = ((Device->SelectAccess->Raddr_len & 2) >>1); 
    /* TWI Device address MSB */
    Semaphores.TwiAddrMsb   = (Device->SelectAccess->Raddr_len & 1);
    /* TWI dataout LSB */
    Semaphores.TwiDoutLsb   = Device->SelectAccess->Rdata_len + Semaphores.TwiAddrLsb;          
    /* TWI dataout MSB */
    if (Device->SelectAccess->Rdata_len == ADI_DEVICE_ACCESS_LENGTH1)
    {
        Semaphores.TwiDoutMsb   = (Semaphores.TwiDoutLsb+1);
    }
    else
    {
        Semaphores.TwiDoutMsb   = (Semaphores.TwiDoutLsb-1);
    }
    /* TWI datain LSB */
    Semaphores.TwiDinLsb    = ((Device->SelectAccess->Rdata_len & 2) >>1); 
    /* TWI datain MSB */
    Semaphores.TwiDinMsb    = (Device->SelectAccess->Rdata_len & 1);
    
    /* Number of address bytes and data bytes */
    TwiDataOutBuf.Buffer.ElementCount   = Device->SelectAccess->Raddr_len + Device->SelectAccess->Rdata_len;
    /* Number of address bytes */
    TwiDataInitBuf.Buffer.ElementCount  = Device->SelectAccess->Raddr_len;
    /* Number of data bytes */
    TwiDataInBuf.Buffer.ElementCount    = Device->SelectAccess->Rdata_len;

#endif

#if defined(ADI_DEVICE_ACCESS_SPI)

    ADI_DEV_1D_BUFFER   SpiWriteBuf;        /* 1D buffer for SPI write operation    */
    ADI_DEV_1D_BUFFER   SpiReadBuf;         /* 1D buffer for SPI read operation     */
 
    SpiWriteBuf.ElementWidth    = 1;        /* By default, element width is 1 byte  */
    SpiReadBuf.ElementWidth     = 1; 

    /* SPI Global address LSB location for the 'Device' */
    Semaphores.SpiGaddrLsb  = ((Device->SelectAccess->Gaddr_len & 2) >>1); 
    /* SPI Global address MSB location for the 'Device' */
    Semaphores.SpiGaddrMsb  = (Device->SelectAccess->Gaddr_len & 1);
    
    /* SPI 'Device' register address LSB */
    if (Device->SelectAccess->Gaddr_len) 
    {
        /* 'Device' has a valid SPI Global address */
        Semaphores.SpiAddrLsb   = (Device->SelectAccess->Raddr_len + Semaphores.SpiGaddrLsb);
    } else 
    {
        Semaphores.SpiAddrLsb   = ((Device->SelectAccess->Raddr_len & 2) >>1);
    }
    
    /* SPI 'Device' register address MSB */
    if (Device->SelectAccess->Raddr_len == ADI_DEVICE_ACCESS_LENGTH1) 
    {
        Semaphores.SpiAddrMsb   = Semaphores.SpiAddrLsb+1;
    } 
    else 
    {
        Semaphores.SpiAddrMsb   = Semaphores.SpiAddrLsb-1;
    }
    
    /* Rdata_len == 0 means this device falls in to ADI_DEVICE_ACCESS_SPECIAL_SPI_CASE1 */
    /* also Rdata_len can hold other special case access information */ 
    if (Device->SelectAccess->Rdata_len)
    {
        /* this device requests normal SPI access (need to append R/W bits) */
        /* SPI 'Device' register value LSB */
        Semaphores.SpiDataLsb   = (Semaphores.SpiAddrLsb+Device->SelectAccess->Rdata_len);
        /* SPI 'Device' register value MSB */
        if (Device->SelectAccess->Rdata_len == ADI_DEVICE_ACCESS_LENGTH2)
        {
            Semaphores.SpiDataMsb   = (Semaphores.SpiDataLsb-1);
        }
        else
        {
            Semaphores.SpiDataMsb   = (Semaphores.SpiDataLsb+1);
        }
        /* Element Count = Length of SPI Global Address + Length of 'Device' Register Address + Length of 'Device' Register Data */
        SpiWriteBuf.ElementCount    = Device->SelectAccess->Gaddr_len + Device->SelectAccess->Raddr_len + Device->SelectAccess->Rdata_len;
        SpiReadBuf.ElementCount     = SpiWriteBuf.ElementCount;
    } 
    else    /* Special case (Rdata_len == 0) */
    {
        /* this device requests special SPI access, where Device access just need 
           to transmit data passed by the 'Device' and pass the received data to 
           the 'Device' with out any modifications */
        /* SPI 'Device' register value MSB */
        Semaphores.SpiDataMsb       = 0;
        /* SPI 'Device' register value LSB */   
        Semaphores.SpiDataLsb       = 1;
        /* Element Count */
        SpiWriteBuf.ElementCount    = Device->SelectAccess->Gaddr_len + Device->SelectAccess->Raddr_len;
        SpiReadBuf.ElementCount     = SpiWriteBuf.ElementCount;       
    }
    
#endif

    regcount = 0; /* Reset the register count */

    /* Read/Write to Device? */
    if ((Device->Command == ADI_DEV_CMD_REGISTER_WRITE)             ||
        (Device->Command == ADI_DEV_CMD_REGISTER_BLOCK_WRITE)       ||      
        (Device->Command == ADI_DEV_CMD_REGISTER_TABLE_WRITE))
    {
        /* Set read/write flag to write mode */
        Semaphores.rwFlag = 0;
        AccessMode = ADI_DEVICE_ACCESS_PRE_WRITE;       
    }
    else
    {
        /* Set read/write flag to read mode */
        Semaphores.rwFlag = 1;
        AccessMode = ADI_DEVICE_ACCESS_PRE_READ;        
    }
    
    switch (Device->Command)
    {
        /* CASE (Read block of Device registers starting from first given address) */
        case(ADI_DEV_CMD_REGISTER_BLOCK_READ):
        /* CASE (Write to a block of 'Device' registers starting from first given address) */
        case(ADI_DEV_CMD_REGISTER_BLOCK_WRITE):    
    
            /* pointer to block access table */
            AccessBlock = (ADI_DEV_ACCESS_REGISTER_BLOCK *) Device->Value;      
            /* Load the register block start address */
            RegAddr = AccessBlock->Address;
        
            /* access all registers in the given array */
            while (regcount < AccessBlock->Count)
            {
                /* Load the 'Device' Register data in write mode */
                if (!Semaphores.rwFlag)
                {
                    RegData = *(AccessBlock->pData+regcount);
                }
                
                /* Call 'Device' specific function */
                if (Device->DeviceFunction)
                {
                    if ((Result = (Device->DeviceFunction)(Device->ClientHandle,&RegAddr,RegData,AccessMode)) != ADI_DEV_RESULT_SUCCESS)
                    {
                        break;  /* exit on error */
                    }
                }

/* Validate 'Device' register address / data */
#ifdef ADI_DEV_DEBUG    /* Debug build only */
                if ((Result = ValidateDeviceAccess (Device,RegAddr,0,0)) != ADI_DEV_RESULT_SUCCESS)
                {
                    break;  /* exit on error */
                }
                        
#endif
                 /* Check if the access type is TWI */
                 if (Device->SelectAccess->AccessType == ADI_DEVICE_ACCESS_TYPE_TWI) 
                 {
#if defined(ADI_DEVICE_ACCESS_TWI)              
                    /* Access the 'Device' register */
                    Result = TwiAccess (    Device,             /* pointer to the 'Device' table */ 
                                            &TwiDataOutBuf,     /* Seq. Buffer for TWI dataout */
                                            &TwiDataInitBuf,    /* Seq. Buffer to initialise TWI data in */
                                            &TwiDataInBuf,      /* Seq. Buffer for TWI data in */
                                            &Semaphores,        /* Device access Semaphores */
                                            RegAddr,            /* 'Device' register address to access */
                                            &RegData );         /* 'Device' register data */
#endif
                }
                else    /* its access type is SPI */
                {
#if defined(ADI_DEVICE_ACCESS_SPI)
                    /* Access the 'Device' register */
                    Result = SpiAccess (    Device,             /* pointer to the 'Device' table */
                                            &SpiWriteBuf,       /* 1D Buffer for SPI data out */
                                            &SpiReadBuf,        /* 1D Buffer for SPI data in */
                                            &Semaphores,        /* Device access Semaphores */
                                            RegAddr,            /* 'Device' register address to access */
                                            &RegData );         /* 'Device' register data */
#endif
                }       
                if (Result != ADI_DEV_RESULT_SUCCESS)
                {
                    break;  /* exit on error */
                }       
                /* update the read data to application buffer in read mode */
                if (Semaphores.rwFlag)  
                {
                    *(AccessBlock->pData+regcount)= RegData;
                }
                ++regcount;
                /* Code specific for devices with multiple Page/Bank registers */
                if (regcount < AccessBlock->Count)
                {
                    /* Call 'Device' specific function */
                    if (Device->DeviceFunction)
                    {
                        /* Devices with multiple Page/Bank registers */
                        /* 'Device' driver should check the present Register address being passed
                           and if there is a page/bank switch, the 'Device' driver should update 'RegAddr' 
                           with (starting address - 1) of present page/bank */
                        if ((Result = (Device->DeviceFunction)(Device->ClientHandle,&RegAddr,RegData,ADI_DEVICE_ACCESS_PRE_ADDR_GENERATE)) 
                                      != ADI_DEV_RESULT_SUCCESS)
                        {
                            break;  /* exit on error */
                        }
                                            
                    }
                }
                /* generates next address to access */
                GenerateAddress(Device,&Semaphores,&RegAddr);
            }
            break; 

        /* CASE (Read a specific register from the 'Device')    */
        case (ADI_DEV_CMD_REGISTER_READ):
        /* CASE (Configure a specific register in the 'Device') */
        case(ADI_DEV_CMD_REGISTER_WRITE):
        /* CASE (Read selected registers of the 'Device')       */
        case(ADI_DEV_CMD_REGISTER_TABLE_READ):
        /* CASE (Configure selected registers of the 'Device')  */
        case(ADI_DEV_CMD_REGISTER_TABLE_WRITE):        

            /* pointer to the table of selective registers to access */
            AccessSelective = (ADI_DEV_ACCESS_REGISTER *) Device->Value;        
            RegAddr = AccessSelective->Address; /* get the first register address to be accessed */

            /* access all registers in the array until we reach the register access delimiter */
            while (RegAddr != ADI_DEV_REGEND)
            {
                /* Load the 'Device' Register data in write mode */
                if (!Semaphores.rwFlag)
                {
                    RegData = AccessSelective->Data;
                }
                /* Call 'Device' specific function */
                if (Device->DeviceFunction)
                {
                    if ((Result = (Device->DeviceFunction)(Device->ClientHandle,&RegAddr,RegData,AccessMode)) 
                                  != ADI_DEV_RESULT_SUCCESS)
                    {
                        break;  /* exit on error */
                    }
                }
                
/* Validate 'Device' register address / data */
#ifdef ADI_DEV_DEBUG
                if ((Result = ValidateDeviceAccess (Device,RegAddr,0,0)) != ADI_DEV_RESULT_SUCCESS)
                {
                    break;  /* exit on error */
                }
#endif
            
                /* Check if the access type is TWI */
                if (Device->SelectAccess->AccessType == ADI_DEVICE_ACCESS_TYPE_TWI) 
                {
#if defined(ADI_DEVICE_ACCESS_TWI)              
                    /* Access the 'Device' register */
                    Result = TwiAccess (    Device,             /* pointer to the 'Device' table */
                                            &TwiDataOutBuf,     /* Seq. Buffer for TWI dataout */
                                            &TwiDataInitBuf,    /* Seq. Buffer to initialise TWI data in */
                                            &TwiDataInBuf,      /* Seq. Buffer for TWI data in */
                                            &Semaphores,        /* Device access Semaphores */
                                            RegAddr,            /* 'Device' register address to access */
                                            &RegData );         /* 'Device' register data */
#endif
                }
                else
                {
#if defined(ADI_DEVICE_ACCESS_SPI)
                    /* Access the 'Device' register */
                    Result = SpiAccess (    Device,             /* pointer to the 'Device' table */
                                            &SpiWriteBuf,       /* 1D Buffer for SPI data out */
                                            &SpiReadBuf,        /* 1D Buffer for SPI data in */
                                            &Semaphores,        /* Device access Semaphores */
                                            RegAddr,            /* 'Device' register address to access */
                                            &RegData );         /* 'Device' register data */
#endif
                }
                if (Result != ADI_DEV_RESULT_SUCCESS)
                {
                    break;  /* exit on error */
                }
            
                /* update the read data to application buffer in read mode */
                if (Semaphores.rwFlag)
                {
                    AccessSelective->Data = RegData; 
                }
                ++AccessSelective;      /* go to next array element */
            
                /* check if the access command is for single register or selective table of registers */
                if ((Device->Command == ADI_DEV_CMD_REGISTER_WRITE) ||
                    (Device->Command == ADI_DEV_CMD_REGISTER_READ))
                {
                    /* for single register access */
                    RegAddr = ADI_DEV_REGEND;   /* exit the while loop after accessing one register */
                }
                else
                {
                    /* for register table access */
                    RegAddr = AccessSelective->Address; /* get the next register address to be accessed */
                }
            }
            break;

        /* CASE (Read a specific field from a single device register) */
        case(ADI_DEV_CMD_REGISTER_FIELD_READ):
        /* CASE (Write to a specific field in a single device register) */
        case(ADI_DEV_CMD_REGISTER_FIELD_WRITE):
        /* CASE (Read selected field(s) of the 'Device' Register(s)) */
        case(ADI_DEV_CMD_REGISTER_FIELD_TABLE_READ):    
        /* CASE (Write to selected field(s) of the 'Device' Register(s)) */
        case(ADI_DEV_CMD_REGISTER_FIELD_TABLE_WRITE):
            
            /* pointer to the table of selective register fields to access */
            AccessField = (ADI_DEV_ACCESS_REGISTER_FIELD *) Device->Value;
            RegAddr     = AccessField->Address; /* get the first register address to be accessed */

            /* access all registers in the array until we reach the register access delimiter */
            while (RegAddr != ADI_DEV_REGEND)  
            { 
                /* Call 'Device' specific function */
                if (Device->DeviceFunction)
                {
                    if ((Result = (Device->DeviceFunction)(Device->ClientHandle,&RegAddr,RegData,AccessMode))
                                  != ADI_DEV_RESULT_SUCCESS)
                    {
                        break;  /* exit on error */

                    }
                }

/* Validate 'Device' register address / data */
#ifdef ADI_DEV_DEBUG
                if ((Result = ValidateDeviceAccess (Device,RegAddr,AccessField->Data,AccessField->Field)) 
                              != ADI_DEV_RESULT_SUCCESS)
                {
                    break;  /* exit on error */
                }
#endif

                /* Check if the access type is TWI */
                if (Device->SelectAccess->AccessType == ADI_DEVICE_ACCESS_TYPE_TWI)
                {
#if defined(ADI_DEVICE_ACCESS_TWI)              
                    /* Access the 'Device' register */
                    Result = TwiAccess (    Device,             /* pointer to the 'Device' table */
                                            &TwiDataOutBuf,     /* Seq. Buffer for TWI dataout */
                                            &TwiDataInitBuf,    /* Seq. Buffer to initialise TWI data in */
                                            &TwiDataInBuf,      /* Seq. Buffer for TWI data in */
                                            &Semaphores,        /* Device access Semaphores */
                                            RegAddr,            /* 'Device' register address to access */
                                            &RegData );         /* 'Device' register data */
#endif
                }
                else
                {
#if defined(ADI_DEVICE_ACCESS_SPI)
                    /* Access the 'Device' register */
                    Result = SpiAccess (    Device,             /* pointer to the 'Device' table */
                                            &SpiWriteBuf,       /* 1D Buffer for SPI data out */
                                            &SpiReadBuf,        /* 1D Buffer for SPI data in */
                                            &Semaphores,        /* Device access Semaphores */
                                            RegAddr,            /* 'Device' register address to access */
                                            &RegData );         /* 'Device' register data */
#endif  
                }

                if (Result != ADI_DEV_RESULT_SUCCESS)
                {
                    break;  /* exit on error */
                }

                /* if (this is register field write) */
                if ((Device->Command == ADI_DEV_CMD_REGISTER_FIELD_WRITE ) ||
                    (Device->Command == ADI_DEV_CMD_REGISTER_FIELD_TABLE_WRITE))
                {
                    /* Toggle the rwFlag as each field write should be preceded with a field read */
                    Semaphores.rwFlag = !Semaphores.rwFlag;
                }
            
                /* in read mode, update register field value location with the read field value */
                if (Semaphores.rwFlag)
                {
                    if ((Device->Command == ADI_DEV_CMD_REGISTER_FIELD_READ)    || 
                        (Device->Command == ADI_DEV_CMD_REGISTER_FIELD_TABLE_READ))
                    {
                        /* Mask other register fields and clear the corresponding register field value */
                        MaskRegField(Device,&Semaphores,RegAddr,AccessField->Field,&RegData);
                        /* update register field value location with the read field value */
                        AccessField->Data = (RegData >> AccessField->Field);
                    }
                    AccessMode = ADI_DEVICE_ACCESS_PRE_READ;
                    ++AccessField;  /* go to next array element */
                
                    /* check if the access command is for single register field or table of register fields */
                    if ((Device->Command == ADI_DEV_CMD_REGISTER_FIELD_READ)    ||
                        (Device->Command == ADI_DEV_CMD_REGISTER_FIELD_WRITE))
                    {
                        /* for single register field access */
                        /* exit the while loop after accessing one register */
                        RegAddr = ADI_DEV_REGEND;
                    }
                    else
                    {
                        /* To access table of register fields */
                        /* get the next register address to be accessed */
                        RegAddr = AccessField->Address;
                    }
                }
                /* in write mode, update the register with new register field value */
                else
                {
                    /* Mask other register fields and clear the corresponding register field value */
                    MaskRegField(Device,&Semaphores,RegAddr,AccessField->Field,&RegData);
                    RegData |= (AccessField->Data << AccessField->Field);
                    AccessMode = ADI_DEVICE_ACCESS_PRE_WRITE;
                }
            }
            break;

        default: /* command not supported or recogonised */

            Result = ADI_DEV_RESULT_CMD_NOT_SUPPORTED; /*  return error */
 
            break;
    }  
      
return(Result);
}

#if defined(ADI_DEVICE_ACCESS_TWI)
/*********************************************************************

 Function:  TwiCallbackFunction

 Description: Fields the callback from the TWI Driver

*********************************************************************/

static void TwiCallbackFunction(
 void*  DeviceHandle, 
 u32    Event, 
 void*  pArg
){

/* no action for TWI callback */

}

#endif

#if defined(ADI_DEVICE_ACCESS_SPI)
/*********************************************************************

 Function:  SpiCallbackFunction

 Description: Fields the callback from the SPI Driver

*********************************************************************/

static void SpiCallbackFunction(
 void*  DeviceHandle, 
 u32    Event, 
 void*  pArg
){
    ADI_DEVICE_ACCESS_REGISTERS     *Device;            /* pointer to the 'Device' table */
    /* retrive the device table we're working on */
    Device = (ADI_DEVICE_ACCESS_REGISTERS *) DeviceHandle;
    
    switch (Event) 
    {
  
        /* identify which buffer is generating the callback */
        case ADI_SPI_EVENT_WRITE_BUFFER_PROCESSED:
        case ADI_SPI_EVENT_READ_BUFFER_PROCESSED:
        
            /* SPI has finished procesing 'Device' IO buffers. De-select the slave device */
            /* 'Device' SPI CS is tied high to Deselect it */
            adi_dev_Control( SpiHandle, ADI_SPI_CMD_DESELECT_SLAVE, (void*)Device->SelectAccess->DeviceCS );            
            /* update SPI semaphore to terminate SPI access */
            SpiCompleteFlag = 1;
        
            break;    
        case ADI_SPI_EVENT_TRANSMISSION_ERROR:
        case ADI_SPI_EVENT_RECEIVE_ERROR:       
            break; 
   
        /* ENDCASE */
    }
}

#endif

/* Debug build only */
#ifdef ADI_DEV_DEBUG
/*********************************************************************
*
* Function:  ValidateDeviceAccess
*
* Description: Validates Device register address/register field
*              to be accessed and its values
*
*********************************************************************/
static u32 ValidateDeviceAccess (
    ADI_DEVICE_ACCESS_REGISTERS     *Device,    /* Pointer to the 'Device' table */
    u16                             RegAddr,    /* Register address to be accessed */
    u16                             RegValue,   /* Value of Register address or Register field location to be accessed */
    u16                             RegField    /* Register field location to be accessed */
)
{
    /* default return code - assume success */
    u32 Result = ADI_DEV_RESULT_SUCCESS;
    /* Temp locations */
    u16 i,j,field,ShiftCount,MaxShiftCount,Reserved;
    u8  ValidationDone = FALSE; /* Flag to indicate Validation status */
    
    /* Continue until validation is done */
    while(!ValidationDone)
    {
        /* Step 1 - Check if the address is beyond the limits */
        if (RegAddr>Device->FinalRegAddr) 
        {
            /* Given address is beyond the limits */
            Result = ADI_DEV_RESULT_INVALID_REG_ADDRESS;
            break;  /* exit on error */
        }
    
        /* Step 2 - Check for Valid address */
        if ((Device->ValidateRegister != NULL) && (Device->ValidateRegister->InvalidRegs != NULL))
        {
            for (i=0;i<Device->ValidateRegister->Count1;i++)
            {
                /* if this register address is listed as invalid */
                if (RegAddr==*(Device->ValidateRegister->InvalidRegs+i))
                {
                    /* return error */
                    Result = ADI_DEV_RESULT_INVALID_REG_ADDRESS;
                    break;
                }
            }
            if (Result != ADI_DEV_RESULT_SUCCESS)
            {
                break;  /* exit on error */
            }
        }
        
        /* Step 3 - Check for write operation to Read only registers */
        if (((Device->Command==ADI_DEV_CMD_REGISTER_WRITE)              ||
            (Device->Command==ADI_DEV_CMD_REGISTER_FIELD_WRITE)         ||
            (Device->Command==ADI_DEV_CMD_REGISTER_TABLE_WRITE)         ||
            (Device->Command==ADI_DEV_CMD_REGISTER_FIELD_TABLE_WRITE)   ||
            (Device->Command==ADI_DEV_CMD_REGISTER_BLOCK_WRITE)))
        {
            if ((Device->ValidateRegister != NULL) && (Device->ValidateRegister->ReadOnlyRegs != NULL))
            {
                for (i=0;i<Device->ValidateRegister->Count2;i++)
                {
                    /* if this is write operation and given register address is listed as read-only */
                    if (RegAddr == Device->ValidateRegister->ReadOnlyRegs[i])
                    {
                        /* return error */
                        Result = ADI_DEV_RESULT_ATTEMPT_TO_WRITE_READONLY_REG;
                        break;
                    }
                }
            }
            if (Result != ADI_DEV_RESULT_SUCCESS)
            {
                break;  /* exit on error */
            }
        }

    
        /* Step 4 - Check for Valid Register field location & register field value */
        /* Only for register field read / write operations */
        Reserved=0;
        if ((Device->Command == ADI_DEV_CMD_REGISTER_FIELD_READ)        ||
            (Device->Command == ADI_DEV_CMD_REGISTER_FIELD_TABLE_READ)  ||
            (Device->Command == ADI_DEV_CMD_REGISTER_FIELD_WRITE)       ||
            (Device->Command == ADI_DEV_CMD_REGISTER_FIELD_TABLE_WRITE))
        {
            if (Device->RegisterField != NULL)
            {
                /* Check whether user is trying to access invalid register field location */
                for (i=0;i<Device->RegisterField->Count;i++)
                {
                    /* If the register entry is found in the table, check for a valid register field */
                    if (RegAddr==Device->RegisterField->RegAddr[i]) /* get the register address */
                    {
                        /* if a register address match is found, get the corresponding register field to access */
                        ShiftCount = RegField;
                        j = 0x01 << ShiftCount; /* shift to compare with corresponding reg field entry in the table */
                
                        /* Check if the register field entry is valid */
                        if (!(j&Device->RegisterField->RegField[i]))
                        {
                            Result = ADI_DEV_RESULT_INVALID_REG_FIELD;
                        }
                        break;  /* exit loop */
                    }
                }
            }
            if (Result != ADI_DEV_RESULT_SUCCESS)
            {
                break;  /* exit on error */
            }
        
            /* Given Register address is not listed in Register field table. */
            /* Application might be trying to access a register with no individual fields. return error */
            if (i==Device->RegisterField->Count) 
            {
                Result = ADI_DEV_RESULT_INVALID_REG_FIELD;
                break;  /* exit on error */
            }
        
            /* Check whether user trying to access Reserved location */
            if (Device->ReservedValues != NULL)
            {
                for (i=0;i<Device->ReservedValues->Count;i++)
                {
                    /* Look for registers having Reserved locations */
                    if (RegAddr==Device->ReservedValues->RegAddr[i])
                    {
                        Reserved=Device->ReservedValues->ReservedBits[i];
                
                        /* if a register address match is found, get the corresponding register field to access */
                        ShiftCount = RegField;
                        j = 0x01 << ShiftCount; /* shift to compare with corresponding reg field entry in the table */
                    
                        /* client tries to access a reserved area? */
                        if (j&Device->ReservedValues->ReservedBits[i])
                        {
                            Result = ADI_DEV_RESULT_ATTEMPT_TO_ACCESS_RESERVE_AREA;
                        }
                        break;  /* exit loop */
                    }
                }
                if (Result != ADI_DEV_RESULT_SUCCESS)
                {
                    break;  /* exit on error */
                }
            }
        }
    
        /* Step 5 - Validate the register field value */
        /* Only for Register field write operation */
        if ((Device->Command == ADI_DEV_CMD_REGISTER_FIELD_WRITE) ||
            (Device->Command == ADI_DEV_CMD_REGISTER_FIELD_TABLE_WRITE))
        {
            /* Check whether user trying to configure register field with invalid value */
            if (Device->RegisterField != NULL)
            {
                for (i=0;i<Device->RegisterField->Count;i++)
                {
                    /* Look for register entry in the table */
                    if (RegAddr==Device->RegisterField->RegAddr[i]) /* get the register address */
                    {
                        ShiftCount = RegField;                          /* Load the present register field to be accessed */
                        field = Device->RegisterField->RegField[i]; /* load the corresponding register field location */
                        field|=Reserved;                                /* add reserved mask to the bit field mask - BJ   */
                        field>>=ShiftCount+1;                       /* shift the value to number of shift count+1   */
                
                        j=1;                                            /* by default, register field value will be atleast 1 bit */
                
                        /* Calculate Maximum Shift count value */
                        if (Device->SelectAccess->Rdata_len == ADI_DEVICE_ACCESS_LENGTH2)
                        {
                            MaxShiftCount = 15;
                        }
                        else
                        {
                            MaxShiftCount = 7;
                        }
                        /* get the register field length (shift untill bit 0 of field = 1) */
                        while ((!(field & 0x01)) && (ShiftCount<MaxShiftCount))
                        {
                            j++;        /* increment register field length */
                            field>>=1;
                            ShiftCount++;
                        }

                        /* if the length of register field value > actual register field length, return error */
                        if (RegValue >= (0x0001 <<j))
                        {
                            Result = ADI_DEV_RESULT_INVALID_REG_FIELD_DATA;
                        }
                        break;
                    }
                }
            }
        }
        ValidationDone = TRUE; /* Register address/data validation complete */
    }
    /* return result */
    return (Result);
}

#endif /* Debug build only */

/*****/
