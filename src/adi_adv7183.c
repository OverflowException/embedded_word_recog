
/*****************************************************************************

Copyright (c) 2005 Analog Devices.  All Rights Reserved.

This software is proprietary and confidential.  By using this software you agree
to the terms of the associated Analog Devices License Agreement.  
******************************************************************************

$File: adi_ADV7183.c $
$Revision: 4 $
$Date: 2009-07-28 11:25:38 -0400 (Tue, 28 Jul 2009) $

Project:    ADV7183 video decoder device driver
Title:      ADV7183 driver source code
Author(s):  SS
Revised by:

Description:
            This is the driver source code for the ADV7183 Video Decoder. It is layered
            on top of the PPI and TWI device drivers, which are configured for
            the specific use of ADV7183 Video Decoder.

References:


Note: 
    This driver supports BF533,BF537 and BF561 EZ-Kit

******************************************************************************

Modification History:
====================
Revision 1.0
Revision 1.1 (20/04/2006) SS
 	- changed default PPI control register value for BF561 from 0x084 to 0x184(DMA32 enabled)
Revision 1.2 (30/04/2006) SS
 	- changed code for new device access service interface (register access array structure)
 	- added command ADI_AD7183_CMD_SET_ACTIVE_VIDEO
  	- added command ADI_AD7183_CMD_SET_VERTICAL_BLANKING
	
 	
******************************************************************************

Include files

*****************************************************************************/

#include <services/services.h>							// system services
#include <drivers/adi_dev.h>							// Device manager includes
#include <drivers/deviceaccess/adi_device_access.h>  	// adi_device_access driver includes
#include <drivers/decoder/adi_adv7183.h>				// ADV7183 includes
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
#error "*** ADV7183 Driver do not yet support this processor ***"
#endif

/*********************************************************************

Enumerations and defines

*********************************************************************/
#define	ADV7183_COMMON_PAGE_END_ADDRESS	0x3D	// Address of the last register in ADV7183 Common Register Area
#define ADV7183_PAGE1_END_ADDRESS     0xF9    	// Address of the Page1 last register in ADV7183
#define ADV7183_PAGE2_END_ADDRESS     0x14C    	// Address of the Page2 last register in ADV7183


#define ADI_ADV7183_NUM_DEVICES (sizeof(Device)/sizeof(ADI_ADV7183)) // number of ADV7183 devices in the system

typedef struct {
    ADI_DEV_PDD_HANDLE          twiHandle;          	// Handle to the underlying twi device driver
    u32                         twiDeviceNumber;    	// TWI Device number used for ADV7183 video dataflow
    u32                         twiDeviceAddress;   	// TWI address of the ADV7183 device
    ADI_DEV_CMD_VALUE_PAIR	*twiConfigTable;	// TWI Configuration Table specific to the application
} ADI_ADV7183_TWI;

typedef struct {
    ADI_DEV_PDD_HANDLE          ppiHandle;          	// Handle to the underlying PPI device driver    
    u32                         ppiDeviceNumber;    	// PPI Device number used for ADV7183 video dataflow    
    u32                     	DataflowFlag;      	// flag to indicate whether PPI dataflow is ON or OFF
#if defined (__ADSP_EDINBURGH__)    ||\
    defined (__ADSP_BRAEMAR__)      ||\
    defined (__ADSP_STIRLING__)     ||\
    defined (__ADSP_TETON__)        ||\
    defined (__ADSP_KOOKABURRA__)   ||\
    defined (__ADSP_MOCKINGBIRD__)
    u16     					PPIControlReg;     	// PPI Control register value
#elif defined (__ADSP_MOAB__)
    u32     					PPIControlReg;     	/* PPI Control register value   */
    u32                         PpiSamplesPerLine;  /* EPPI - Samples per line      */
#endif
    u32                        	ppiF_Lines;    		// Number of lines per frame
} ADI_ADV7183_PPI;

// Structure to contain data for an instance of the ADV7183 device driver
typedef struct {
    ADI_DEV_MANAGER_HANDLE      ManagerHandle;   	// Manager Handle  
    ADI_DEV_DEVICE_HANDLE       DMHandle;           	// Handle of Device Manager instance
    u32                     	InUseFlag;      	// flag to indicate whether an instance is in use or not
    ADI_DMA_MANAGER_HANDLE      DMAHandle;          	// handle to the DMA manager
    ADI_DCB_HANDLE              DCBHandle;          	// callback handle    
    ADI_DCB_CALLBACK_FN         DMCallback;         	// the callback function supplied by the Device Manager
    ADI_ADV7183_TWI				ADV7183Twi;
    ADI_ADV7183_PPI				ADV7183Ppi;
    u32                     	RegisterAccessFlag;     // flag to indicate whether page 1 or 2 of registers are available for 

} ADI_ADV7183;

// The initial values for the device instance
static ADI_ADV7183 Device[] = {
    {
        NULL,
        NULL,
        false,
        NULL,
        NULL,
        NULL,
        {// ADV7183Twi
        	NULL,	// TWI handle
        	0,      // TWI 0 as default
        	ADV7183_TWI_ADDR0,	// ADV7183 device twi address
        	NULL,	// use Default configuration table available in device access service
        },
        {
        	NULL,	// PPI/EPPI handle
		    0,      // PPI/EPPI device number 0 by default        
        	false,  // PPI/EPPI dataflow flag
#if defined (__ADSP_TETON__)    /* BF561 */
			0x0184,	// PPI is configured as input with ITU 656 standard data & DMA32 enabled
#elif defined (__ADSP_EDINBURGH__)    ||\
      defined (__ADSP_BRAEMAR__)      ||\
      defined (__ADSP_STIRLING__)     ||\
      defined (__ADSP_KOOKABURRA__)   ||\
      defined (__ADSP_MOCKINGBIRD__)    
        	0x0084,	// PPI is configured as output with ITU 656 standard data
#elif defined (__ADSP_MOAB__)   /* BF54x */
            0x38101004, /* EPPI DMA packing enabled, ITU Interlaced Entire Video input */
            1716,       /* EPPI Samples per line - For NTSC input */
#endif
        	525,	// PPI/EPPI lines per frame, default NTSC

        },
        0,	// access user register map
    },
};

typedef struct {
	ADI_ADV7183    					*pADV7183;     	// Pointer to the device we are working on
	ADI_DEVICE_ACCESS_REGISTERS     *Device;		// Pointer to Device Access Service table
}	ADV7183_HANDLE;
           
/*********************************************************************

Static functions

*********************************************************************/


static u32 adi_pdd_Open(                            // Open a device
    ADI_DEV_MANAGER_HANDLE  ManagerHandle,          // device manager handle
    u32                     DeviceNumber,           // device number
    ADI_DEV_DEVICE_HANDLE   DMHandle,               // device handle
    ADI_DEV_PDD_HANDLE      *pPDDHandle,            // pointer to PDD handle location 
    ADI_DEV_DIRECTION       Direction,              // data direction
    void                    *pEnterCriticalArg,     // enter critical region parameter
    ADI_DMA_MANAGER_HANDLE  DMAHandle,              // handle to the DMA manager
    ADI_DCB_HANDLE          DCBHandle,              // callback handle
    ADI_DCB_CALLBACK_FN     DMCallback              // device manager callback function
);

static u32 adi_pdd_Close(                           // Closes a device
    ADI_DEV_PDD_HANDLE      PDDHandle               // PDD handle
);

static u32 adi_pdd_Read(                            // Reads data or queues an inbound buffer to a device
    ADI_DEV_PDD_HANDLE      PDDHandle,              // PDD handle
    ADI_DEV_BUFFER_TYPE     BufferType,             // buffer type
    ADI_DEV_BUFFER          *pBuffer                // pointer to buffer
);
    
static u32 adi_pdd_Write(                           // Writes data or queues an outbound buffer to a device
    ADI_DEV_PDD_HANDLE      PDDHandle,              // PDD handle
    ADI_DEV_BUFFER_TYPE     BufferType,             // buffer type
    ADI_DEV_BUFFER          *pBuffer                // pointer to buffer
);
  
static u32 adi_pdd_SeqIO(                           // Queues an sequential buffer to a device
    ADI_DEV_PDD_HANDLE      PDDHandle,              // PDD handle
    ADI_DEV_BUFFER_TYPE     BufferType,             // buffer type
    ADI_DEV_BUFFER          *pBuffer                // pointer to buffer
);
  
static u32 adi_pdd_Control(                         // Sets or senses a device specific parameter
    ADI_DEV_PDD_HANDLE      PDDHandle,              // PDD handle
    u32                     Command,                // command ID
    void                    *Value                  // command specific value
);

// the callback function passed to the PPI driver
static void ppiCallbackFunction(
    void*   DeviceHandle, 
    u32   	Event, 
    void*   pArg
);

// the callback function passed to the twi driver
static void twiCallbackFunction(
    void*   DeviceHandle, 
    u32   	Event, 
    void*   pArg
);

// Function specific to ADV7183 driver passed to 'Device' access service

static u32 DeviceAccessFunction (
	//ADI_DEVICE_ACCESS_REGISTERS     *Device,// pointer to the 'Device' table passed back by the device access service   
	void	*Handle,
    u16		*ADV7183RegAddr,	// ADV7183 Register address being configured
    u16		ADV7183RegValue,	// Value of ADV7183 Register address being configured 
    ADI_DEVICE_ACCESS_MODE		access_mode		// present access mode of ADV7183
       
);
// PPI open function
static u32 PPI_Open( 
 	ADI_DEV_PDD_HANDLE	PDDHandle   // pointer to PDD handle location 
);

/*********************************************************************

Debug Mode functions (debug build only)

*********************************************************************/
#if defined(ADI_DEV_DEBUG)

// Validate PDD handle
static int ValidatePDDHandle(
    ADI_DEV_PDD_HANDLE PDDHandle
);

#endif


/*********************************************************************
Table for Common Register Field Error check and Register field access
Table structure -  'Count' of Register addresses containing individual fields
     Register address containing individual fields, 
     Register field locations in the corresponding register, 
     Reserved bit locations in the corresponding register
Register field location - Bit indicating start of new field in a register will be 1
Reserved bit locations will be 1
*********************************************************************/

// ADV7183 Common register address to perform Register Field Error check and Register field access
static u16 ADV7183RegAddr[] = {  
	ADV7183REGADDR00_3F		// array of ADV7183 register addresses 0x00 to 0x3F
};

// Register Field start locations corresponding to the entries in ADV7183RegAddr (Reserved Bit locations marked as 1)
static u16 ADV7183RegField[]= { 
	ADV7183REGFIELD00_3F
};
static ADI_DEVICE_ACCESS_REGISTER_FIELD RegisterField[] = {
	sizeof(ADV7183RegAddr)/2, 		// 'Count' of Register addresses containing individual fields
 	ADV7183RegAddr,   				// array of ADV7183 register addresses containing individual fields
 	ADV7183RegField,   				// array of register field locations in the corresponding registers
};

/*********************************************************************
Table to configure the reserved bits in the device to its recommended values
Table structure -  'Count' of Register addresses containing Reserved Locations
     Register address containing Reserved Locations
     Recommended value for the Reserved Locations
*********************************************************************/

// ADV7183 Common Register address containing Reserved Locations
static u16 ADV7183RegResvAddr[] = { 
	// ADV7183 Register address 0x00 to 0x3F containing Reserved Locations
	ADV7183REGRESVADDR00_3F
};

// ADV71831 register Reserved Bit locations corresponding to the entries in ADV7183RegResvAddr
static u16 ADV7183ReservedBits[] = { 
	// ADV71831 register Reserved Bit locations address 0x00 to 0x3F
	ADV7183RESERVEDBITS00_3F
};

// Recommended values for the Reserved Bit locations corresponding to the entries in ADV7183RegResvAddr
static u16 ADV7183ReservedValues[] = { 
	// Recommended values for the Reserved Bit locations address 0x00 to 0x3F	
	ADV7183RESERVEDVALUES00_3F
};

static ADI_DEVICE_ACCESS_RESERVED_VALUES ReservedValues[] = {
	sizeof(ADV7183RegResvAddr)/2, 	// 'Count' of Register addresses containing Reserved Locations
	ADV7183RegResvAddr,   			// array of ADV7183 Register address containing Reserved Locations
 	ADV7183ReservedBits,  			// array of reserved bit locations in the corresponding register	
	ADV7183ReservedValues, 			// array of Recommended values for the Reserved Bit locations
};

/*********************************************************************
Table for ADV7183 Register Error check for invalid & read-only register(s) access
Structure - 'Count' of invalid Register addresses in ADV7183
   Invalid Register addresses in ADV7183
   'Count' of Read-only Register addresses in ADV7183
   Read-only Register addresses in ADV7183
*********************************************************************/

// array of read-only registers addresses in ADV7183.
static u16 ADV7183ReadOnlyRegs[]={ 
	// array of read-only registers address 0x00 to 0x3F.
	ADV7183READONLYREGS00_3F
};

// array of invalid registers addresses in ADV7183.
static u16 ADV7183InvalidRegs[]={ 
	// array of invalid registers address 0x00 to 0x3F.
	ADV7183INVALIDREGS00_3F
};

static ADI_DEVICE_ACCESS_VALIDATE_REGISTER ValidateRegister[] = {
	sizeof(ADV7183InvalidRegs)/2, 		// 'Count' of Invalid Register addresses in ADV7183 
 	ADV7183InvalidRegs,		   			// array of Invalid Register addresses in ADV7183
 	sizeof(ADV7183ReadOnlyRegs)/2, 		// 'Count' of Read-only Register addresses in ADV7183
 	ADV7183ReadOnlyRegs,      			// pointer to array of Read-only Register addresses in ADV7183
};

/*********************************************************************
Table for Page 1 Register Field Error check and Register field access
Table structure -  'Count' of Register addresses containing individual fields
     Register address containing individual fields, 
     Register field locations in the corresponding register, 
     Reserved bit locations in the corresponding register
Register field location - Bit indicating start of new field in a register will be 1
Reserved bit locations will be 1
*********************************************************************/

// ADV7183 page 1 register address to perform Register Field Error check and Register field access
static u16 ADV7183RegAddrP1[] = {  
	// following register addresses reside in page 1( address 0x0E bit6 = 0 )
	ADV7183_RESAMPLE_CTR, ADV7183_GEMSTAR_CTR1, ADV7183_GEMSTAR_CTR2, ADV7183_GEMSTAR_CTR3, ADV7183_GEMSTAR_CTR4, 
	ADV7183_GEMSTAR_CTR5, 
	ADV7183REGADDR4D_F9 // array of ADV7183 register addresses 0x4D to 0xF9
};

// Register Field start locations corresponding to the entries in ADV7183RegAddrP1 (Reserved Bit locations marked as 1)
static u16 ADV7183RegFieldP1[]= { 
	0xFF, 0x01, 0x01, 0x01, 0x01, 0xFF, 
	ADV7183REGFIELD4D_F9

};
static ADI_DEVICE_ACCESS_REGISTER_FIELD RegisterFieldP1[] = {
	sizeof(ADV7183RegAddrP1)/2, 	// 'Count' of Register addresses containing individual fields
 	ADV7183RegAddrP1,   			// array of ADV7183 register addresses containing individual fields
 	ADV7183RegFieldP1,   			// array of register field locations in the corresponding registers
};

/*********************************************************************
Table to configure the reserved bits in the device to its recommended values
Table structure -  'Count' of Register addresses containing Reserved Locations
     Register address containing Reserved Locations
     Recommended value for the Reserved Locations
*********************************************************************/

// ADV7183 page 1 Register address containing Reserved Locations
static u16 ADV7183RegResvAddrP1[] = { 
	ADV7183_RESAMPLE_CTR, ADV7183_GEMSTAR_CTR5, 
	// ADV7183 Register address 0x4D to 0xF9 containing Reserved Locations
	ADV7183REGRESVADDR4D_F9
};


// ADV71831 register Reserved Bit locations corresponding to the entries in ADV7183RegAddr1
static u16 ADV7183ReservedBitsP1[] = { 
	 0xBF,0xFE,
	// ADV71831 register Reserved Bit locations address 0x4D to 0xF9
	ADV7183RESERVEDBITS4D_F9	
};

// Recommended values for the Reserved Bit locations corresponding to the entries in ADV7183RegAddr2
static u16 ADV7183ReservedValuesP1[] = { 
	0x10, 0x00, 
	// Recommended values for the Reserved Bit locations address 0x4D to 0xF9	
	ADV7183RESERVEDVALUES4D_F9

};

static ADI_DEVICE_ACCESS_RESERVED_VALUES ReservedValuesP1[] = {
	sizeof(ADV7183RegResvAddrP1)/2, 	// 'Count' of Register addresses containing Reserved Locations
	ADV7183RegResvAddrP1,   			// array of ADV7183 Register address containing Reserved Locations
 	ADV7183ReservedBitsP1,  		// array of reserved bit locations in the corresponding register	
	ADV7183ReservedValuesP1, 		// array of Recommended values for the Reserved Bit locations
};

/*********************************************************************
Table for ADV7183 Register Error check for invalid & read-only register(s) access
Structure - 'Count' of invalid Register addresses in ADV7183
   Invalid Register addresses in ADV7183
   'Count' of Read-only Register addresses in ADV7183
   Read-only Register addresses in ADV7183
*********************************************************************/

// array of read-only registers addresses in ADV7183.
static u16 ADV7183ReadOnlyRegsP1[]={ 
	// array of read-only registers address 0x4D to 0xF9.
	ADV7183READONLYREGS4D_F9	
};

// array of invalid registers addresses in ADV7183.
static u16 ADV7183InvalidRegsP1[]={ 
	0x40, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
	// array of invalid registers address 0x4D to 0xF9.
	ADV7183INVALIDREGS4D_F9	
};


static ADI_DEVICE_ACCESS_VALIDATE_REGISTER ValidateRegisterP1[] = {
	sizeof(ADV7183InvalidRegsP1)/2, 	// 'Count' of Invalid Register addresses in ADV7183 
 	ADV7183InvalidRegsP1,   		// array of Invalid Register addresses in ADV7183
 	sizeof(ADV7183ReadOnlyRegsP1)/2, 	// 'Count' of Read-only Register addresses in ADV7183
 	ADV7183ReadOnlyRegsP1,      	// pointer to array of Read-only Register addresses in ADV7183
};

/*****************************************************************************************************************/

/*********************************************************************
Table for Page 2 Register Field Error check and Register field access
Table structure -  'Count' of Register addresses containing individual fields
     Register address containing individual fields, 
     Register field locations in the corresponding register, 
     Reserved bit locations in the corresponding register
Register field location - Bit indicating start of new field in a register will be 1
Reserved bit locations will be 1
*********************************************************************/

// ADV7183 page 2 register address to perform Register Field Error check and Register field access
static u16 ADV7183RegAddrP2[] = {  
	// following register addresses reside in page 2( address 0x0E bit6 = 1 )
	ADV7183_INT_CONFIG0,ADV7183_INT_STATUS1,ADV7183_INT_CLEAR1,ADV7183_INT_MASK1,ADV7183_INT_STATUS2,
	ADV7183_INT_CLEAR2,ADV7183_INT_MASK2,ADV7183_RAW_STATUS3,ADV7183_INT_STATUS3,ADV7183_INT_CLEAR3,
	ADV7183_INT_MASK2 

};

// Register Field start locations corresponding to the entries in ADV7183RegAddrP2 (Reserved Bit locations marked as 1)
static u16 ADV7183RegFieldP2[]= { 
	0x5d, 0xFF, 0xFF, 0xFF, 0xFF,0xFF, 0xFF, 0xFF, 0xFF, 0xFF,0xFF
};

static ADI_DEVICE_ACCESS_REGISTER_FIELD RegisterFieldP2[] = {
	sizeof(ADV7183RegAddrP2)/2, 	// 'Count' of Register addresses containing individual fields
 	ADV7183RegAddrP2,   			// array of ADV7183 register addresses containing individual fields
 	ADV7183RegFieldP2,   			// array of register field locations in the corresponding registers
};

/*********************************************************************
Table to configure the reserved bits in the device to its recommended values
Table structure -  'Count' of Register addresses containing Reserved Locations
     Register address containing Reserved Locations
     Recommended value for the Reserved Locations
*********************************************************************/

// ADV7183 Register address containing Reserved Locations
static u16 ADV7183RegResvAddrP2[] = { 
	ADV7183_INT_CONFIG0,ADV7183_INT_STATUS1,ADV7183_INT_CLEAR1,ADV7183_INT_MASK1,ADV7183_INT_STATUS2,
	ADV7183_INT_CLEAR2,ADV7183_INT_MASK2,ADV7183_RAW_STATUS3,ADV7183_INT_STATUS3,ADV7183_INT_CLEAR3,
	ADV7183_INT_MASK2
};

// ADV71831 register Reserved Bit locations corresponding to the entries in ADV7183RegAddr1
static u16 ADV7183ReservedBitsP2[]= { 
	0x08,0x9C,0x9C,0x9C,0x70,0x70,0x70,0xE8,0xC0,0xC0,0xC0
};

// Recommended values for the Reserved Bit locations corresponding to the entries in ADV7183RegAddr2
static u16 ADV7183ReservedValuesP2[] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

static ADI_DEVICE_ACCESS_RESERVED_VALUES ReservedValuesP2[] = {
	sizeof(ADV7183RegResvAddrP2)/2, 	// 'Count' of Register addresses containing Reserved Locations
	ADV7183RegResvAddrP2,   			// array of ADV7183 Register address containing Reserved Locations
 	ADV7183ReservedBitsP2,  		// array of reserved bit locations in the corresponding register	
	ADV7183ReservedValuesP2, 		// array of Recommended values for the Reserved Bit locations
};

/*********************************************************************
Table for ADV7183 Register Error check for invalid & read-only register(s) access
Structure - 'Count' of invalid Register addresses in ADV7183
   Invalid Register addresses in ADV7183
   'Count' of Read-only Register addresses in ADV7183
   Read-only Register addresses in ADV7183
*********************************************************************/

// array of read-only registers addresses in ADV7183.
static u16 ADV7183ReadOnlyRegsP2[]={ 
	ADV7183_INT_STATUS1,ADV7183_INT_STATUS2,ADV7183_RAW_STATUS3,ADV7183_INT_STATUS3
	
};

// array of invalid registers addresses in ADV7183.
static u16 ADV7183InvalidRegsP2[]={ 
	0x141, 0x145
};

static ADI_DEVICE_ACCESS_VALIDATE_REGISTER ValidateRegisterP2[] = {
	sizeof(ADV7183InvalidRegsP2)/2, 	// 'Count' of Invalid Register addresses in ADV7183 
 	ADV7183InvalidRegsP2,   		// array of Invalid Register addresses in ADV7183
 	sizeof(ADV7183ReadOnlyRegsP2)/2, 	// 'Count' of Read-only Register addresses in ADV7183
 	ADV7183ReadOnlyRegsP2,      	// pointer to array of Read-only Register addresses in ADV7183
};

// Table to select Device access type
static ADI_DEVICE_ACCESS_SELECT SelectTWIAccess[] = {
 	0,   								// Don't care in case of TWI access
 	ADI_DEVICE_ACCESS_LENGTH0,			// 'Device' Global address (Don't care for TWI Access)
 	ADI_DEVICE_ACCESS_LENGTH1,			// 'Device' register address length (1 byte)
 	ADI_DEVICE_ACCESS_LENGTH1,			// 'Device' register data length (1 byte)
 	ADI_DEVICE_ACCESS_TYPE_TWI,			// Select TWI access
};

/*********************************************************************/


/*********************************************************************
*
*   Function:       PPI_Open
*
*   Description:    Opens the PPI/EPPI device for ADV7183 video dataflow
*
*********************************************************************/
static u32 PPI_Open( 
    ADI_DEV_PDD_HANDLE PDDHandle    // Physical Device Driver Handle
) {
    
    ADI_ADV7183  *pADV7183;   // pointer to the device we're working on
    // default return code
    u32 Result = ADI_DEV_RESULT_SUCCESS; 
    // avoid casts
    pADV7183 = (ADI_ADV7183 *)PDDHandle; // Pointer to ADV7183 device driver instance
    
#if defined (__ADSP_EDINBURGH__)    ||\
    defined (__ADSP_BRAEMAR__)      ||\
    defined (__ADSP_STIRLING__)     ||\
    defined (__ADSP_TETON__)        ||\
    defined (__ADSP_KOOKABURRA__)   ||\
    defined (__ADSP_MOCKINGBIRD__)
    // table of configuration values for the PPI on input	
    ADI_DEV_CMD_VALUE_PAIR InboundConfigurationTable [] = {
        { ADI_PPI_CMD_SET_CONTROL_REG,          (void *)pADV7183->ADV7183Ppi.PPIControlReg},// PPI Control Register
		{ ADI_PPI_CMD_SET_LINES_PER_FRAME_REG,	(void *)pADV7183->ADV7183Ppi.ppiF_Lines	},
		{ ADI_DEV_CMD_END, NULL						},
	};
#elif defined (__ADSP_MOAB__)
    // table of configuration values for the EPPI on input	
    ADI_DEV_CMD_VALUE_PAIR InboundConfigurationTable [] = {
        { ADI_EPPI_CMD_SET_SAMPLES_PER_LINE,    (void *)pADV7183->ADV7183Ppi.PpiSamplesPerLine	},  /* Samples per line register*/
		{ ADI_EPPI_CMD_SET_LINES_PER_FRAME,	    (void *)pADV7183->ADV7183Ppi.ppiF_Lines	        },  /* Lines per frame register */
        { ADI_EPPI_CMD_SET_CONTROL_REG,         (void *)pADV7183->ADV7183Ppi.PPIControlReg      },  /* EPPI Control Register    */
		{ ADI_DEV_CMD_END,                      NULL						                    },
	};
#endif

    // Open the PPI/EPPI driver
    Result = adi_dev_Open( 
        pADV7183->ManagerHandle,	// device manager handle
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
        pADV7183->ADV7183Ppi.ppiDeviceNumber,  // PPI/EPPI device number
        pADV7183,                   // client handle - passed to internal callback function
        &pADV7183->ADV7183Ppi.ppiHandle,       // pointer to DM handle (for PPI/EPPI driver) location
        ADI_DEV_DIRECTION_INBOUND,  // PPI/EPPI used only to receive video data
        pADV7183->DMAHandle,        // handle to the DMA manager
        pADV7183->DCBHandle,        // handle to the callback manager
        ppiCallbackFunction         // internal callback function
    );

    // return with appropriate code if PPI/EPPI driver fails to open
#ifdef ADI_DEV_DEBUG
    if (Result != ADI_DEV_RESULT_SUCCESS) return (Result);
#endif

    // configure the PPI/EPPI driver with the values from the inbound configuration table
    Result = adi_dev_Control(pADV7183->ADV7183Ppi.ppiHandle, ADI_DEV_CMD_TABLE, InboundConfigurationTable);

    return (Result);
}

/*********************************************************************
*
* Function:  adi_pdd_Open
*
* Description: Opens the ADV7183 device for use
*
*********************************************************************/
static u32 adi_pdd_Open(                            // Open a device
    ADI_DEV_MANAGER_HANDLE  ManagerHandle,          // device manager handle
    u32                     DeviceNumber,           // device number
    ADI_DEV_DEVICE_HANDLE   DMHandle,               // device handle
    ADI_DEV_PDD_HANDLE      *pPDDHandle,            // pointer to PDD handle location 
    ADI_DEV_DIRECTION       Direction,              // data direction
    void                    *pEnterCriticalArg,     // enter critical region parameter
    ADI_DMA_MANAGER_HANDLE  DMAHandle,              // handle to the DMA manager
    ADI_DCB_HANDLE          DCBHandle,              // callback handle
    ADI_DCB_CALLBACK_FN     DMCallback              // device manager callback function
) {
 
    u32 		Result;             // return code
    ADI_ADV7183	*pADV7183;   		// pointer to the ADV7183 device we're working on
    void 		*pExitCriticalArg;  // exit critical region parameter

    // Check for a valid device number
#ifdef ADI_DEV_DEBUG
    if (DeviceNumber >= ADI_ADV7183_NUM_DEVICES) return (ADI_DEV_RESULT_BAD_DEVICE_NUMBER);
    	// accept only inbound data flow.
	if (Direction != ADI_DEV_DIRECTION_INBOUND) {
    	return(ADI_DEV_RESULT_DIRECTION_NOT_SUPPORTED); // data direction not supported
	}

#endif




/********************************
Configure ADV7183 device instance
********************************/

    // assign the pointer to the device instance
    pADV7183 = &Device[DeviceNumber];
    // and store the Manager handle
    pADV7183->ManagerHandle = ManagerHandle;    
    // and store the Device Manager handle
    pADV7183->DMHandle = DMHandle;
    // and store the DMA Manager handle
    pADV7183->DMAHandle = DMAHandle;
    // and store the DCallback Manager handle
    pADV7183->DCBHandle = DCBHandle;
    // and callback function
    pADV7183->DMCallback = DMCallback;
    
    // insure the device the client wants is available    
    Result = ADI_DEV_RESULT_DEVICE_IN_USE;
    // Check that this device instance is not already in use. 
    // If not, assign flag to indicate that it is now.
    pExitCriticalArg = adi_int_EnterCriticalRegion(pEnterCriticalArg);
    if (pADV7183->InUseFlag == FALSE) {
        pADV7183->InUseFlag = TRUE;
        Result = ADI_DEV_RESULT_SUCCESS;
    }
    adi_int_ExitCriticalRegion(pExitCriticalArg);

#ifdef ADI_DEV_DEBUG
    if (Result != ADI_DEV_RESULT_SUCCESS) return (Result);  
#endif
    
    // save the physical device handle in the client supplied location
    *pPDDHandle = (ADI_DEV_PDD_HANDLE *)pADV7183;
    
  
	// return after successful completion
 	return(Result);
}


/*********************************************************************
*
* Function:  adi_pdd_Close
*
* Description: Closes down a PPI & twi device 
*
*********************************************************************/

static u32 adi_pdd_Close(                           // Closes a device
    ADI_DEV_PDD_HANDLE      PDDHandle               // PDD handle
)   {

	u32 		Result;  	// return value
 	ADI_ADV7183 *pADV7183 = (ADI_ADV7183 *)PDDHandle;

 // check for errors if required
#if defined(ADI_DEV_DEBUG)
    if ((Result = ValidatePDDHandle(PDDHandle)) != ADI_DEV_RESULT_SUCCESS) return (Result);
#endif

    // close PPI driver
    Result = adi_dev_Close(pADV7183->ADV7183Ppi.ppiHandle);

    // mark the device as closed
    pADV7183->InUseFlag = FALSE;

    return(Result);
}

/*********************************************************************
*
* Function:  adi_pdd_Read
*
* Description: Reads data or queues an inbound buffer to a device
*
*********************************************************************/

static u32 adi_pdd_Read(                            // Reads data or queues an inbound buffer to a device
    ADI_DEV_PDD_HANDLE      PDDHandle,              // PDD handle
    ADI_DEV_BUFFER_TYPE     BufferType,             // buffer type
    ADI_DEV_BUFFER          *pBuffer                // pointer to buffer
) {
    // check for errors if required
    u32 Result = ADI_DEV_RESULT_SUCCESS;
    ADI_ADV7183  *pADV7183 = (ADI_ADV7183 *)PDDHandle;
    
#if defined(ADI_DEV_DEBUG)
    if ((Result = ValidatePDDHandle(PDDHandle)) != ADI_DEV_RESULT_SUCCESS) return (Result);
#endif

    // pass the request on to PPI
    Result = adi_dev_Read (pADV7183->ADV7183Ppi.ppiHandle, BufferType, pBuffer);

 return(Result);
    
}

/*********************************************************************
*
* Function:  adi_pdd_Write
*
* Description: Writes data or queues an outbound buffer to a device
*
*********************************************************************/
 
static u32 adi_pdd_Write(                           // Writes data or queues an outbound buffer to a device
    ADI_DEV_PDD_HANDLE      PDDHandle,              // PDD handle
    ADI_DEV_BUFFER_TYPE     BufferType,             // buffer type
    ADI_DEV_BUFFER          *pBuffer                // pointer to buffer
) {

 // No write operation for ADV7183
    return(ADI_AD7183_RESULT_CMD_NOT_SUPPORTED);
 
}


/*********************************************************************
*
* Function:  adi_pdd_SeqIO
*
* Description: Queues a sequential buffer to a device
*
*********************************************************************/
 
static u32 adi_pdd_SeqIO(                 
    ADI_DEV_PDD_HANDLE      PDDHandle,              // PDD handle
    ADI_DEV_BUFFER_TYPE     BufferType,             // buffer type
    ADI_DEV_BUFFER          *pBuffer                // pointer to buffer
) {

 // No sequential operation for ADV7183
    return(ADI_AD7183_RESULT_CMD_NOT_SUPPORTED);
 
}

/*********************************************************************
*
* Function:  adi_pdd_Control
*
* Description: 
*
*********************************************************************/
    
static u32 adi_pdd_Control(                         // Sets or senses a device specific parameter
    ADI_DEV_PDD_HANDLE      PDDHandle,              // PDD handle
    u32                     Command,                // command ID
    void                    *Value                  // command specific value
) {

    ADI_ADV7183 *pADV7183;	// pointer to the device we're working on
    u32         Result;   	// return value
    u32         u32Value;  	// u32 type to avoid casts/warnings etc.
    u16         u16Value;  	// u16 type to avoid casts/warnings etc.
    u8          u8Value;    // u8 type to avoid casts/warnings etc. 
     
#if defined (__ADSP_EDINBURGH__) || defined (__ADSP_BRAEMAR__) || defined (__ADSP_STIRLING__) || defined (__ADSP_TETON__)
   	// configuration values for the PPI input (active field only)	
    ADI_DEV_CMD_VALUE_PAIR ActiveFieldConfig [] = {
    { ADI_PPI_CMD_SET_TRANSFER_TYPE,	 (void *)0x00 },// transfer type active fields only
	{ ADI_PPI_CMD_SET_ACTIVE_FIELD_SELECT,	(void *)0x01 },// field 1 & 2
	{ ADI_DEV_CMD_END, NULL						},
	};

	   	// configuration values for the PPI input (vertical blanking only)	
    	ADI_DEV_CMD_VALUE_PAIR VBlankingConfig [] = {
    	{ ADI_PPI_CMD_SET_TRANSFER_TYPE,	 (void *)0x10 },
		{ ADI_DEV_CMD_END, NULL						},
		};
#endif
	
	// Structure passed to device access service
	ADI_DEVICE_ACCESS_REGISTERS	access_device;
    
    // Client handle passed to Device Access Service
    ADV7183_HANDLE		adv7183_handle;
    
    // avoid casts
    pADV7183 = (ADI_ADV7183 *)PDDHandle; // Pointer to ADV7183 device driver instance
    // assign 8, 16 and 32 bit values for the Value argument
    u32Value = (u32) Value;
    u16Value = (u16) u32Value;
    u8Value  = (u8) u32Value;
      
   
    // check for errors if required
#if defined(ADI_DEV_DEBUG)
    if ((Result = ValidatePDDHandle(PDDHandle)) != ADI_DEV_RESULT_SUCCESS) return (Result);
#endif
 
    // assume we're going to be successful
    Result = ADI_DEV_RESULT_SUCCESS;
    
    switch (Command)    
    {

        /* CASE (control dataflow) */
        case (ADI_DEV_CMD_SET_DATAFLOW):
    
	        /* Cannot set dataflow when PPI is closed */
	        if (pADV7183->ADV7183Ppi.ppiHandle == NULL)
	        {
	            Result = ADI_AD7183_RESULT_PPI_NOT_OPENED;  /* return error */
	            break;
            }

            if(u16Value) 
            {
                pADV7183->ADV7183Ppi.DataflowFlag = 1;  /* Video dataflow enabled */
            }
            else 
            {
                pADV7183->ADV7183Ppi.DataflowFlag = 0;  /*  Video dataflow disabled */
            }
            /* Pass Dataflow flag to PPi/EPPi driver */
            Result = adi_dev_Control( pADV7183->ADV7183Ppi.ppiHandle, ADI_DEV_CMD_SET_DATAFLOW, (void*)u16Value );
        
            break;

    /* CASE (query for processor DMA support) */
            case (ADI_DEV_CMD_GET_PERIPHERAL_DMA_SUPPORT):
  
                /* PPI/EPPI needs DMA but ADV7183 doesn't */
                *((u32 *)Value) = FALSE;
   
                break;

            /* CASE (Set Dataflow method - applies only for PPI/EPPI) */
            case (ADI_DEV_CMD_SET_DATAFLOW_METHOD):
                /* Check if PPI/EPPI device to used by ADV7183 is already open */
                if (pADV7183->ADV7183Ppi.ppiHandle == NULL)
                {
                    /* if not, try to open the PPI/EPPI device corresponding to ppiDeviceNumber for ADV7183 */
                    Result = PPI_Open(PDDHandle);

/* Debug build only */
#if defined(ADI_DEV_DEBUG)
                    if (Result != ADI_DEV_RESULT_SUCCESS)
                    {
                        break; /* exit on error */
                    }
#endif
            }
            /* Pass the dataflow method to the PPI/EPPI device allocated to ADV7183 */
            Result = adi_dev_Control( pADV7183->ADV7183Ppi.ppiHandle, ADI_DEV_CMD_SET_DATAFLOW_METHOD, (void*)u16Value );
            break;
    
		
        
    	// CASE (Read a single register from the ADV7183)
    	case (ADI_DEV_CMD_REGISTER_READ):
    	// CASE (Configure a specific register in the ADV7183)
    	case(ADI_DEV_CMD_REGISTER_WRITE):
    	// CASE (Read a specific field from a single device register)
    	case(ADI_DEV_CMD_REGISTER_FIELD_READ):
    	// CASE (Write to a specific field in a single device register)
    	case(ADI_DEV_CMD_REGISTER_FIELD_WRITE):     	
    	// CASE (Read block of ADV7183 registers starting from first given address)
    	case(ADI_DEV_CMD_REGISTER_BLOCK_READ):  
    	// CASE (Write to a block of ADV7183 registers starting from first given address)
    	case(ADI_DEV_CMD_REGISTER_BLOCK_WRITE):
    	
    	// CASE (Read a table of selective device registers of ADV7183)
    	case(ADI_DEV_CMD_REGISTER_TABLE_READ):  
      	// CASE (Write to a table of selective device registers of ADV7183)
		case (ADI_DEV_CMD_REGISTER_TABLE_WRITE): 
  		// CASE (Read a table of selective device register(s) field of ADV7183)
    	case (ADI_DEV_CMD_REGISTER_FIELD_TABLE_READ):
  		// CASE (Write to a table of selective device register(s) field of ADV7183)
		case (ADI_DEV_CMD_REGISTER_FIELD_TABLE_WRITE):			
	
	adv7183_handle.pADV7183 		= pADV7183;			// Device we are working on
    	adv7183_handle.Device 			= &access_device;	// Structure passed to Device access service     
    	
	access_device.RegisterField		= NULL;      		// table will be updated in 'DeviceAccessFunction'
    	access_device.ReservedValues	= NULL;    			// table will be updated in 'DeviceAccessFunction'
    	access_device.ValidateRegister	= NULL;    			// table will be updated in 'DeviceAccessFunction'
    	access_device.FinalRegAddr		= ADV7183_COMMON_PAGE_END_ADDRESS;			// value will be updated in 'DeviceAccessFunction'
    	
  	
    	access_device.ManagerHandle		= pADV7183->ManagerHandle;	// device manager handle
    	access_device.ClientHandle		= &adv7183_handle;			// client handle - passed to the internal 'Device' specific function
    	access_device.DeviceNumber		= pADV7183->ADV7183Twi.twiDeviceNumber;	// TWI device number
	access_device.DeviceAddress		= pADV7183->ADV7183Twi.twiDeviceAddress;	// TWI address of ADV7183 Device
    	access_device.DCBHandle			= pADV7183->DCBHandle;        	// handle to the callback manager
    	access_device.DeviceFunction	= DeviceAccessFunction;       	// Function specific to ADV7183 driver passed to the 'Device' access service
	access_device.Command			= Command;                    	// command ID
	access_device.Value				= Value;                      	// command specific value
    	access_device.ConfigTable		= pADV7183->ADV7183Twi.twiConfigTable;	// TWI configuration table
	access_device.SelectAccess		= SelectTWIAccess;		// Device Access type    	    	
	access_device.pAdditionalinfo	= (void *)NULL;    		// No Additional info

	Result = adi_device_access (&access_device);
	
#ifdef ADI_DEV_DEBUG
    if (Result != ADI_DEV_RESULT_SUCCESS) return (Result);
#endif

        break;

 

/*************************************
PPI/EPPI related commands
*************************************/
    // CASE (Open PPI Device driver) 
    case (ADI_AD7183_CMD_OPEN_PPI):

		 // Update the PPI device number
        pADV7183->ADV7183Ppi.ppiDeviceNumber = u8Value;
        
            // open the PPI/EPPI driver
        Result = PPI_Open(PDDHandle);
 
  		break;

    // CASE (Set PPI operation mode of the ppi)
        case (ADI_AD7183_CMD_SET_VIDEO_FORMAT):
    
#ifdef ADI_DEV_DEBUG
    	    if (u32Value > 1)
    	    {
    	        Result = ADI_AD7183_RESULT_BAD_VIDEO_FORMAT;
    	        break;
            }
#endif    	
            if(u32Value == NTSC_FORMAT)
            {
    		    pADV7183->ADV7183Ppi.ppiF_Lines         = 525; // NTSC mode
#if defined (__ADSP_MOAB__)
                pADV7183->ADV7183Ppi.PpiSamplesPerLine  = 1716;
#endif
            }
    	    else
    	    {
     		    pADV7183->ADV7183Ppi.ppiF_Lines         = 625; // PAL mode
#if defined (__ADSP_MOAB__)
                pADV7183->ADV7183Ppi.PpiSamplesPerLine  = 1728;
#endif
            }

#if defined (__ADSP_EDINBURGH__)    ||\
    defined (__ADSP_BRAEMAR__)      ||\
    defined (__ADSP_STIRLING__)     ||\
    defined (__ADSP_TETON__)        ||\
    defined (__ADSP_KOOKABURRA__)   ||\
    defined (__ADSP_MOCKINGBIRD__)
    	    Result = adi_dev_Control(pADV7183->ADV7183Ppi.ppiHandle, ADI_PPI_CMD_SET_LINES_PER_FRAME_REG,(void *)pADV7183->ADV7183Ppi.ppiF_Lines);
#elif defined (__ADSP_MOAB__)
    	    Result = adi_dev_Control(pADV7183->ADV7183Ppi.ppiHandle, ADI_EPPI_CMD_SET_SAMPLES_PER_LINE,(void *)pADV7183->ADV7183Ppi.PpiSamplesPerLine);
#ifdef ADI_DEV_DEBUG
            if (Result != ADI_DEV_RESULT_SUCCESS)
            {
                break;  /* exit on error */
            }
#endif
    	    Result = adi_dev_Control(pADV7183->ADV7183Ppi.ppiHandle, ADI_EPPI_CMD_SET_LINES_PER_FRAME,(void *)pADV7183->ADV7183Ppi.ppiF_Lines);
#endif

            break;

        // CASE (Set PPI to read active video data) Command applicable only for PPI based devices
        case (ADI_AD7183_CMD_SET_ACTIVE_VIDEO):
    
#if defined (__ADSP_EDINBURGH__) || defined (__ADSP_BRAEMAR__) || defined (__ADSP_STIRLING__) || defined (__ADSP_TETON__)    		
            if (pADV7183->ADV7183Ppi.ppiHandle) // if PPI device is already opened
	        {
    	        // update the PPI control register
    	        Result = adi_dev_Control(pADV7183->ADV7183Ppi.ppiHandle, ADI_DEV_CMD_TABLE, ActiveFieldConfig);
	        }
	        else
	        {
     	        Result = ADI_AD7183_RESULT_PPI_NOT_OPENED; //  return error
            }
#endif
            break;
        
        // CASE (Set PPI to read vertical blanking only) Command applicable only for PPI based devices
        case (ADI_AD7183_CMD_SET_VERTICAL_BLANKING):

#if defined (__ADSP_EDINBURGH__) || defined (__ADSP_BRAEMAR__) || defined (__ADSP_STIRLING__) || defined (__ADSP_TETON__)    		     
    		if (pADV7183->ADV7183Ppi.ppiHandle) // if PPI device is already opened
			{
                // update the PPI control register
    			Result = adi_dev_Control(pADV7183->ADV7183Ppi.ppiHandle, ADI_DEV_CMD_TABLE, VBlankingConfig);
			}
  			else
  			{
            	Result = ADI_AD7183_RESULT_PPI_NOT_OPENED; //  return error
            }
#endif
            break;
     
        
        
/*************************************
TWI related commands
*************************************/
    // CASE (Set TWI Device Number that will be used access ADV7183 device registers)
    case (ADI_AD7183_CMD_SET_TWI_DEVICE):

    	// Update the TWI device number
        pADV7183->ADV7183Twi.twiDeviceNumber = u8Value;

  		break;

    // CASE (Set TWI Configuration table specific to the application)
    case (ADI_AD7183_CMD_SET_TWI_CONFIG_TABLE):

        // Update the TWI Configuration table
        pADV7183->ADV7183Twi.twiConfigTable = (ADI_DEV_CMD_VALUE_PAIR*) Value;

  		break;
  		
    // CASE (Set TWI address for the ADV7183 device)
    case (ADI_AD7183_CMD_SET_TWI_ADDRESS):

        // Update the TWI device number
        pADV7183->ADV7183Twi.twiDeviceAddress = u32Value;

  		break;
 				        
    default: 
// pass anything we don't specifically handle to the PPI driver
	Result = adi_dev_Control(pADV7183->ADV7183Ppi.ppiHandle, Command, Value); 
        break;
    }
    
    return(Result);
    
}

/*********************************************************************

 Function:  ppiCallbackFunction

 Description: Fields the callback from the PPI Driver

*********************************************************************/

static void ppiCallbackFunction(void* DeviceHandle, u32 Event, void* pArg)
{
 	ADI_ADV7183 *pADV7183 = (ADI_ADV7183 *)DeviceHandle;

#if defined(ADI_DEV_DEBUG)
	if (ValidatePDDHandle(DeviceHandle) != ADI_DEV_RESULT_SUCCESS)    // verify the DM handle
  		return;
#endif

	// simply pass the callback along to the Device Manager Callback without interpretation
 	(pADV7183->DMCallback)(pADV7183->DMHandle,Event,pArg);

}

/*********************************************************************

 Function:  twiCallbackFunction

 Description: Fields the callback from the twi Driver

*********************************************************************/

static void twiCallbackFunction (
	void* 	DeviceHandle, 
	u32 	Event, 
	void* 	pArg
) {
 	ADI_ADV7183 *pADV7183 = (ADI_ADV7183 *)DeviceHandle;
#if defined(ADI_DEV_DEBUG)
 	if (ValidatePDDHandle(DeviceHandle) != ADI_DEV_RESULT_SUCCESS)   // verify the DM handle
  		return;

#endif

 	// simply pass the callback along to the Device Manager Callback without interpretation
 	(pADV7183->DMCallback)(pADV7183->DMHandle,Event,pArg);

}


/*********************************************************************

 Function:  DeviceAccessFunction

 Description: Function specific to ADV7183 driver passed to Device access service


*********************************************************************/

static u32 DeviceAccessFunction (
    void   					*Handle,   			// AD7183 driver Argument passed back by the device access service
    u16						*ADV7183RegAddr,	// ADV7183 Register address being configured
    u16						ADV7183RegValue,	// Value of ADV7183 Register address being configured 
    ADI_DEVICE_ACCESS_MODE	access_mode			// present access mode of ADV7183
) {

	ADV7183_HANDLE	*adv7183_handle = (ADV7183_HANDLE *) Handle;	 
  	u32     Result;
 	u8		pageswitch;

 	// value to be passed to device access service
    ADI_DEV_ACCESS_REGISTER_FIELD regPageAccess;

 	// Structure passed to device access service for Page switch
 	ADI_DEVICE_ACCESS_REGISTERS		access_device;
 	
    // assume we're going to be successful
    Result = ADI_DEV_RESULT_SUCCESS; 
    pageswitch = 0;		// assume there is no page switch

/****************************************************************************************
RegisterAccessFlag Value	Indicates
	0						Present register is between 0x00 - 0x3F 
	1						Present register is in Page 1 
	2						Present register is in Page 2
****************************************************************************************/

    // Check for page switch
    // Page switch can occur in both read and write operation
	if( *ADV7183RegAddr > 0x100 )	// is this register in Page 2?
	{
		// Check prevailing page status
		if (adv7183_handle->pADV7183->RegisterAccessFlag != 2)	// is it already in page 2?
		{	// if not, Switch to Page 2
    		adv7183_handle->pADV7183->RegisterAccessFlag = 2;	// Mark Page status as Page 2
    		pageswitch = 1;		// Mark page switch falg to perform page switch operation
	   		// command ID to configure ADV7183_ADI_CTR in Page 2
			access_device.Command	= ADI_DEV_CMD_REGISTER_FIELD_WRITE; 	    		
    	}
    }
   	else if( *ADV7183RegAddr > 0x3F )	// is this register in Page 1?
   	{
		// Check prevailing page status
		if (adv7183_handle->pADV7183->RegisterAccessFlag != 1)	// is it already in page 1?
		{	// if not, Switch to Page 1
	   		adv7183_handle->pADV7183->RegisterAccessFlag = 1;	// Mark Page status as Page 1
	   		// Mark page switch flag to perform page switch operation
	   		pageswitch = 1;		
	   		// command ID to configure ADV7183_ADI_CTR in Page 1
			access_device.Command	= ADI_DEV_CMD_REGISTER_FIELD_WRITE; 	
	   	}
 	}
  	else	// this register must be in common register area (0x00 to 0x3F)
		// Mark Page status as in common register area
		adv7183_handle->pADV7183->RegisterAccessFlag = 0;
  	
  	// Special case - Block Read/Block write
  	// Check if the Operation is Block access. If yes, read ADV7183_ADI_CTR to get present page status
  	if ((access_mode == ADI_DEVICE_ACCESS_PRE_ADDR_GENERATE) && ( *ADV7183RegAddr == ADV7183_COMMON_PAGE_END_ADDRESS))
  	{
  		// command ID to configure ADV7183_ADI_CTR in Page 1
		access_device.Command	= ADI_DEV_CMD_REGISTER_FIELD_READ;
		pageswitch = 1;		// Mark page switch to read ADV7183_ADI_CTR
	}	
	    		   		
	// Check pageswitch flag
	if (pageswitch)
	{	// perform page switch operation
		
		regPageAccess.Address 	= ADV7183_ADI_CTR;// Page Switch Register
		regPageAccess.Field = ADV7183_SUB_USR_EN;	// Page Indication bit

	
		if (adv7183_handle->pADV7183->RegisterAccessFlag == 1)	// Switch to Page 1
			regPageAccess.Data = 0;		// Mark SUB_USR_EN to page 1
	    else	// Switch to Page 2
			regPageAccess.Data = 1;		// Mark SUB_USR_EN to page 2
	    		    	
	
		access_device.ManagerHandle  	= adv7183_handle->pADV7183->ManagerHandle;     	// device manager handle
    	access_device.ClientHandle  	= NULL;      					// client handle - passed to the internal 'Device' specific function
    	access_device.DeviceNumber  	= adv7183_handle->pADV7183->ADV7183Twi.twiDeviceNumber; 	// TWI device number
  		access_device.DeviceAddress  	= adv7183_handle->pADV7183->ADV7183Twi.twiDeviceAddress; 	// TWI address of ADV7183 Device
    	access_device.DCBHandle   		= adv7183_handle->pADV7183->DCBHandle;         	// handle to the callback manager
    	access_device.DeviceFunction 	= NULL;        					// Function specific to ADV7183 driver passed to the 'Device' access service
    	access_device.Value    			= (void *)&regPageAccess;  				// command specific value    	
		access_device.FinalRegAddr		= ADV7183_PAGE2_END_ADDRESS; 	// Address of the last register in ADV7183    	
    	access_device.RegisterField  	= RegisterField;         		// table for ADV7183 Register Field Error check and Register field access
    	access_device.ReservedValues 	= NULL;       			// table to configure reserved bits in ADV7183 to recommended values
    	access_device.ValidateRegister 	= NULL;          		// table containing reserved and read-only registers in ADV7183
    	access_device.ConfigTable  		= adv7183_handle->pADV7183->ADV7183Twi.twiConfigTable;  	// TWI configuration table
    	access_device.SelectAccess  	= SelectTWIAccess;    			// Device Access type
  		access_device.pAdditionalinfo 	= (void *)NULL;        			// No Additional info

		pageswitch = 0;		// Clear page switch flag
    	Result = adi_device_access (&access_device);
	}

  	// Special case - Block Read/Block write
  	// Check if the present page status read from ADV7183_ADI_CTR
  	if ((access_mode == ADI_DEVICE_ACCESS_PRE_ADDR_GENERATE) && ( *ADV7183RegAddr == ADV7183_COMMON_PAGE_END_ADDRESS))
  	{
  		if (regPageAccess.Data)	// is ADV7183 in Page 2?
  		{
  			// Mark Page status as Page 2
  			adv7183_handle->pADV7183->RegisterAccessFlag = 2;
	   		// Update the present register address with (starting address - 1) of Page 2
	   		*ADV7183RegAddr = 0x13F;
	   	}
	   	else // ADV7183 must be in Page 1
	   	{
  			// Mark Page status as Page 1
  			adv7183_handle->pADV7183->RegisterAccessFlag = 1;
	   		// Update the present register address with (starting address - 1) of Page 1
	   		*ADV7183RegAddr = 0x3F;
	   	}
	}	
	
	// Update Device Access Service tables
	if (adv7183_handle->pADV7183->RegisterAccessFlag == 0)
	// Update tables to be in Common Register Area
	{
		// Update the table locations with Common register area values in the present Device Access Service structure
		adv7183_handle->Device->FinalRegAddr	= ADV7183_COMMON_PAGE_END_ADDRESS; 	// Address of the last register in ADV7183 Common Register Area
		adv7183_handle->Device->RegisterField	= RegisterField;      				// table for ADV7183 Register Field Error check and Register field access
    	adv7183_handle->Device->ReservedValues	= ReservedValues;    				// table to configure reserved bits in ADV7183 to recommended values
	    adv7183_handle->Device->ValidateRegister= ValidateRegister;    				// table containing reserved and read-only registers in ADV7183
	}
	else if (adv7183_handle->pADV7183->RegisterAccessFlag == 1)
	// Update tables to be in Page 1
	{
		// Update the table locations with Page1 values in the present Device Access Service structure
		adv7183_handle->Device->FinalRegAddr	= ADV7183_PAGE1_END_ADDRESS; 	// Address of the last register in ADV7183								
		adv7183_handle->Device->RegisterField	= RegisterFieldP1;      		// table for ADV7183 Register Field Error check and Register field access
    	adv7183_handle->Device->ReservedValues	= ReservedValuesP1;    			// table to configure reserved bits in ADV7183 to recommended values
	   	adv7183_handle->Device->ValidateRegister= ValidateRegisterP1;    		// table containing reserved and read-only registers in ADV7183
	}
    else	// Update tables to be in Page 2
    {	
		// Update the table locations with Page2 values in the present Device Access Service structure
		adv7183_handle->Device->FinalRegAddr	= ADV7183_PAGE2_END_ADDRESS; 	// Address of the last register in ADV7183								
		adv7183_handle->Device->RegisterField	= RegisterFieldP2;      		// table for ADV7183 Register Field Error check and Register field access
    	adv7183_handle->Device->ReservedValues	= ReservedValuesP2;    			// table to configure reserved bits in ADV7183 to recommended values
	   	adv7183_handle->Device->ValidateRegister= ValidateRegisterP2;    		// table containing reserved and read-only registers in ADV7183
	}

	return (Result);
}

// Debug build only
#ifdef ADI_DEV_DEBUG

/*********************************************************************

 Function:  ValidatePDDHandle

 Description: Validates a PDD handle

*********************************************************************/

static int ValidatePDDHandle(
	ADI_DEV_PDD_HANDLE 	PDDHandle
) {
	
 	unsigned int i;
 	for (i = 0; i < ADI_ADV7183_NUM_DEVICES; i++) 
 	{
  		if (PDDHandle == (ADI_DEV_PDD_HANDLE)&Device[i]) 
  			return (ADI_DEV_RESULT_SUCCESS);
  	}
	return (ADI_DEV_RESULT_BAD_PDD_HANDLE);
}

#endif // Debug build only

/*********************************************************************

Entry point for device manager

*********************************************************************/

ADI_DEV_PDD_ENTRY_POINT ADIADV7183EntryPoint = {
 	adi_pdd_Open,
 	adi_pdd_Close,
 	adi_pdd_Read,
 	adi_pdd_Write,
 	adi_pdd_Control,
 	adi_pdd_SeqIO
};


