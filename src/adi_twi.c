/******************************************************************************

Copyright (c) 2005 Analog Devices.  All Rights Reserved.

This software is proprietary and confidential.  By using this software you agree
to the terms of the associated Analog Devices License Agreement.
*******************************************************************************

$File: adi_twi.c $
$Revision: 14802 $
$Date: 2013-08-07 09:24:44 -0400 (Wed, 07 Aug 2013) $

Project:    TWI device driver
Title:      TWI definitions


Description:
   This is the primary include file for the TWI driver..

Note:
    define ADI_TWI_PSEUDO or ADI_TWI_PSEUDO_ARBITRATION to allow pseudo TWI
    devices


******************************************************************************/



/******************************************************************************

Include files

******************************************************************************/

#include <services/services.h>      // system service includes
#include <drivers/adi_dev.h>        // device manager includes
#include <drivers/twi/adi_twi.h>
#include <string.h>

/*****************************************************************************/



/******************************************************************************

Macros

******************************************************************************/

// alias for the actual device structure
#define ADI_PDD_DRIVER twi_driver


// number of drivers that have been defined
#define ADI_PDD_DRIVERS (sizeof(Device)/sizeof(Device[0]))


// PDD_CHECK verifies a pointer to a driver (as received from the Device Manager)
// indeed points to one of the driver struct's internal to this file.
#if defined(ADI_DEV_DEBUG)
#define ADI_PDD_CHECK(pdd,rv) {int i=0; while (pdd!=&Device[i++]) if (i>=ADI_PDD_DRIVERS) return rv;}
#else
#define ADI_PDD_CHECK(pdd,rv)
#endif

// uncomment to manually set the hardware TWI ivg
//#define ADI_TWI_IVG 0x0b

#if defined(TWI_CLKDIV) || defined(TWI0_CLKDIV)
#define ADI_TWI_HARDWARE
#else
#define ADI_TWI_PSEUDO
#undef ADI_TWI_HARDWARE
#endif

#if defined(ADI_TWI_PSEUDO_ARBITRATION)
#define ADI_TWI_PSEUDO
#endif

#define ADI_TWI_PSEUDO_PERIOD 128

/*****************************************************************************/



/******************************************************************************

Internal data structures:

Note: This code uses C shifts and masks to access fields within the TWI
    registers. While the processor has opcodes to access bit fields, few of the
    TWI registers actually have multi-bit fields in locations other than the
    bottom bits. Further, this field (in master_ctl) is generally accessed in
    combination with other bits in the same register, leaving little to no
    difference in performance. For all the single-bit fields, the C compiler
    converts masks into one of the processor's bit manipulation opcodes. Using C
    bit fields to access the registers requires (with this compiler) the clutter
    of lots of little struct's each bent with a #pragma, so I chose the shifts
    and masks instead.

******************************************************************************/

typedef union {
    volatile u16 reg;
    u32 alignment;      // the 16 bit reg's are on 32 bit boundaries
} reg32;

typedef struct {        // could also access as an array of reg32
    reg32 clkdiv;       // 1400  [0]
    reg32 control;      // 1404
    reg32 slave_ctl;    // 1408
    reg32 slave_stat;   // 140C
    reg32 slave_addr;   // 1410
    reg32 master_ctl;   // 1414  [5]
    reg32 master_stat;  // 1418
    reg32 master_addr;  // 141C
    reg32 int_stat;     // 1420
    reg32 int_mask;     // 1424
    reg32 fifo_ctl;     // 1428
    reg32 fifo_stat;    // 142C  [11]
    reg32 pad[20];      //       [12-31]
    reg32 xmt_data8;    // 1480  [32]
    reg32 xmt_data16;   // 1484
    reg32 rcv_data8;    // 1488
    reg32 rcv_data16;   // 148C  [36]
    //    TWI's bank has a further [28]
} twi_registers;

// the pseudo TWI port structure, all that is needed to emulate the master twi
#if defined(ADI_TWI_PSEUDO)
typedef struct {
    adi_twi_pseudo_port config;
    twi_registers twi;
    u32 data_byte;
    u8 clock;
    u8 state;
    u16 flags;
    u32 busy_count;     // used to detect whether the bus is busy or not,
                // it is reset when either the scl or sda line changes
    u32 busy_reload;    // set to count to 4.7us is standard mode and 1.3us in fast
    volatile bool run_once; // used by timer handle to make sure not run while running
    u8 unstick;     // used by timer handle to prevent bus lockup
} twi_pseudo_port;
#endif

typedef struct
{
    // these MUST be initialized where the static is defined

    // for hardware twi point to the memory mapped registers
    // otherwise it points to a memory address that is allocated
    // for the pseudo registers
    twi_registers *twi;     // eg. twi->master_ctl.reg
    ADI_INT_PERIPHERAL_ID pid;  // interrupt's peripheral ID

    // if pseudo is not null then the device is a pseudo TWI port and twi
    // will point to a memory address where
#if defined(ADI_TWI_PSEUDO)
    twi_pseudo_port *pseudo;
#endif


    // the remainder will be set by adi_pdd_open() - the first 4 are parameters to Open
    void *critical;             // critical region argument
    ADI_DEV_DEVICE_HANDLE DevHandle;    //  which requires this handle
    ADI_DCB_HANDLE DCBHandle;       //  and which may go via this DCB
    ADI_DCB_CALLBACK_FN DMCallback;     // Device Manager's callback (void return)

    u32 ivg;                // which IVG are hooked to (retrieved upon Open)

    // configuration options
    struct {
        ADI_DEV_MODE mode;  // dataflow mode/method (Control call)
        u16 frequency;      // the frequency of the twi in kHz (default=100)
        u16 duty_cycle;     // the duty cycle of the twi in % (default=50)
        u8 fifo;        // byte or word per interrupt, set by ADI_TWI_CMD_FIFO Control
        u8 lostarb;     // error count limits (default=3)
        u8 anak;        // (default=1)
        u8 dnak;        // (default=1)
    } config;

    // only master OR slave is permitted
    union
    {
        struct
        {
            ADI_DEV_SEQ_1D_BUFFER *buffer;
            u8 lostarb;         // counters to keep track of the number of errors
            u8 anak;
            u8 dnak;
        };
        struct
        {
            ADI_DEV_1D_BUFFER *buffer_rx;
            ADI_DEV_1D_BUFFER *buffer_tx;
            // making these 4 bytes bit fields would save no RAM due to packing
            u8 pend_rx;     // next adi_pdd_read() should move data from TWI FIFO
            u8 pend_tx;     // next adi_pdd_write() should move data to TWI FIFO
            u8 sdir;        // a copy of SDIR at SINIT
            u8 gcall;       // gen. call data is being received
        };
    } type;                 // (for want of a better name)
} twi_driver;                   // physical device driver data

// enumeration used by pseudo twi state machine
#if defined(ADI_TWI_PSEUDO)
enum {
    TWI_STATE_PSEUDO_START,
    TWI_STATE_PSEUDO_RSTART,
    TWI_STATE_PSEUDO_ADDRESS,
    TWI_STATE_PSEUDO_TRANSMIT,
    TWI_STATE_PSEUDO_RECEIVE,
    TWI_STATE_PSEUDO_STOP,
    TWI_STATE_PSEUDO_WAIT,
    TWI_STATE_PSEUDO_DISABLED,
};

// enumeration is a list of events that can be raised by the pseudo state machine
enum {
    TWI_EVENT_PSEUDO_STARTED,
    TWI_EVENT_PSEUDO_RSTARTED,
    TWI_EVENT_PSEUDO_ADDRESS=TWI_STATE_PSEUDO_ADDRESS,
    TWI_EVENT_PSEUDO_TRANSMIT=TWI_STATE_PSEUDO_TRANSMIT,
    TWI_EVENT_PSEUDO_RECEIVE=TWI_STATE_PSEUDO_RECEIVE,
    TWI_EVENT_PSEUDO_STOPPED,
#if defined(ADI_TWI_PSEUDO_ARBITRATION)
    TWI_EVENT_PSEUDO_LOSTARB,
#endif
};
#endif

/*****************************************************************************/



/******************************************************************************

Device specific data

******************************************************************************/

/*********************
    Edinburgh
**********************/

#if defined(__ADSP_EDINBURGH__)     // settings for Edinburgh class devices

static ADI_PDD_DRIVER Device[] = {      // actual TWI devices
#if defined(ADI_TWI_PSEUDO)         // gets compiled in if building the pseudo driver
    {               // device 0
        NULL,           // register base address (CLKDIV register)
        (ADI_INT_PERIPHERAL_ID)NULL,// peripheral ID
    },
    {               // device 1
        NULL,           // register base address (CLKDIV register)
        (ADI_INT_PERIPHERAL_ID)NULL,// peripheral ID
    },
    {               // device 2
        NULL,           // register base address (CLKDIV register)
        (ADI_INT_PERIPHERAL_ID)NULL,// peripheral ID
    },
#endif
};

#endif


/*********************
    Braemar
**********************/

#if defined(__ADSP_BRAEMAR__)           // settings for Braemar class devices

static ADI_PDD_DRIVER Device[] = {      // actual TWI devices
    {                       // device 0
        (twi_registers *)TWI_CLKDIV,    // register base address (CLKDIV register)
        ADI_INT_TWI,            // peripheral ID
    },
#if defined(ADI_TWI_PSEUDO)         // gets compiled in if building the pseudo driver
    {                       // device 1
        NULL,               // register base address (CLKDIV register)
        (ADI_INT_PERIPHERAL_ID)NULL,    // peripheral ID
    },
    {                       // device 2
        NULL,               // register base address (CLKDIV register)
        (ADI_INT_PERIPHERAL_ID)NULL,    // peripheral ID
    },
    {                       // device 3
        NULL,               // register base address (CLKDIV register)
        (ADI_INT_PERIPHERAL_ID)NULL,    // peripheral ID
    },
#endif
};

#endif


/*********************
    Teton-Lite
**********************/

#if defined(__ADSP_TETON__)     // settings for Teton class devices

static ADI_PDD_DRIVER Device[] = {      // actual TWI devices
#if defined(ADI_TWI_PSEUDO)         // gets compiled in if building the pseudo driver
    {                   // device 0
        NULL,               // register base address (CLKDIV register)
        (ADI_INT_PERIPHERAL_ID)NULL,    // peripheral ID
    },
    {                       // device 1
        NULL,               // register base address (CLKDIV register)
        (ADI_INT_PERIPHERAL_ID)NULL,    // peripheral ID
    },
    {                       // device 2
        NULL,               // register base address (CLKDIV register)
        (ADI_INT_PERIPHERAL_ID)NULL,    // peripheral ID
    },
#endif
};

#endif


/*********************
    Stirling
**********************/

#if defined(__ADSP_STIRLING__)          // settings for Stirling class devices

static ADI_PDD_DRIVER Device[] = {          // actual TWI devices
    {                   // device 0
        (twi_registers *)TWI0_CLKDIV,   // register base address (CLKDIV register)
        ADI_INT_TWI0,           // peripheral ID
    },
    {                   // device 1
        (twi_registers *)TWI1_CLKDIV,   // register base address (CLKDIV register)
        ADI_INT_TWI1,           // peripheral ID
    },
#if defined(ADI_TWI_PSEUDO)         // gets compiled in if building the pseudo driver
    {                   // device 2
        NULL,               // register base address (CLKDIV register)
        (ADI_INT_PERIPHERAL_ID)NULL,    // peripheral ID
    },
    {                       // device 3
        NULL,               // register base address (CLKDIV register)
        (ADI_INT_PERIPHERAL_ID)NULL,    // peripheral ID
    },
    {                       // device 4
        NULL,               // register base address (CLKDIV register)
        (ADI_INT_PERIPHERAL_ID)NULL,    // peripheral ID
    },
#endif
};

#endif



/******************************************************
    Settings for Moab
*******************************************************/

#if defined(__ADSP_MOAB__)

static ADI_PDD_DRIVER Device[] = {      // actual TWI devices
    {                   // device 0
        (twi_registers *)0xFFC00700,    // register base address (CLKDIV register)
        ADI_INT_TWI0,           // peripheral ID
    },
    {                   // device 1
        (twi_registers *)0xFFC02200,    // register base address (CLKDIV register)
        ADI_INT_TWI1,           // peripheral ID
    },
#if defined(ADI_TWI_PSEUDO)         // gets compiled in if building the pseudo driver
    {                   // device 2
        NULL,               // register base address (CLKDIV register)
        (ADI_INT_PERIPHERAL_ID)NULL,    // peripheral ID
    },
    {                       // device 3
        NULL,               // register base address (CLKDIV register)
        (ADI_INT_PERIPHERAL_ID)NULL,    // peripheral ID
    },
    {                       // device 4
        NULL,               // register base address (CLKDIV register)
        (ADI_INT_PERIPHERAL_ID)NULL,    // peripheral ID
    },
#endif
};

#endif


/****************************************************************
    settings for Kookaburra/Mockingbird/Brodie/Moy class devices

*************************************************************/

#if defined(__ADSP_KOOKABURRA__) || defined(__ADSP_MOCKINGBIRD__) || defined(__ADSP_BRODIE__) || defined(__ADSP_MOY__) || defined(__ADSP_DELTA__)

static ADI_PDD_DRIVER Device[] = {          // actual TWI devices
    {                   // device 0
        (twi_registers *)TWI_CLKDIV,    // register base address (CLKDIV register)
        ADI_INT_TWI,            // peripheral ID
    },
#if defined(ADI_TWI_PSEUDO)         // gets compiled in if building the pseudo driver
    {                   // device 1
        NULL,               // register base address (CLKDIV register)
        (ADI_INT_PERIPHERAL_ID)NULL,    // peripheral ID
    },
    {                       // device 2
        NULL,               // register base address (CLKDIV register)
        (ADI_INT_PERIPHERAL_ID)NULL,    // peripheral ID
    },
    {                       // device 3
        NULL,               // register base address (CLKDIV register)
        (ADI_INT_PERIPHERAL_ID)NULL,    // peripheral ID
    },
#endif
};

#endif




/*********************************************************************/



/********************************************************************

Device driver function declerations

********************************************************************/

// phyisical device driver hooks
static u32 adi_pdd_open(
    ADI_DEV_MANAGER_HANDLE,
    u32,
    ADI_DEV_DEVICE_HANDLE,
    ADI_DEV_PDD_HANDLE *,
    ADI_DEV_DIRECTION,
    void *,
    ADI_DMA_MANAGER_HANDLE,
    ADI_DCB_HANDLE,
    ADI_DCB_CALLBACK_FN);

static u32 adi_pdd_close(ADI_DEV_PDD_HANDLE);
static u32 adi_pdd_control(ADI_DEV_PDD_HANDLE,u32,void *);
static u32 adi_pdd_read(ADI_DEV_PDD_HANDLE,ADI_DEV_BUFFER_TYPE,ADI_DEV_BUFFER *);
static u32 adi_pdd_write(ADI_DEV_PDD_HANDLE,ADI_DEV_BUFFER_TYPE,ADI_DEV_BUFFER *);
static u32 adi_pdd_sequential(ADI_DEV_PDD_HANDLE,ADI_DEV_BUFFER_TYPE,ADI_DEV_BUFFER *);

// general twi control functions
u32 twi_start(ADI_PDD_DRIVER *);
u32 twi_stop(ADI_PDD_DRIVER *);
u32 twi_flush(ADI_PDD_DRIVER *);
u32 twi_callback(ADI_PDD_DRIVER *,u32,void *);
ADI_INT_HANDLER_RESULT twi_handler(ADI_PDD_DRIVER *,u16);

// buffer functions
u32 twi_buffer_transmit(ADI_PDD_DRIVER *);
u32 twi_buffer_receive(ADI_PDD_DRIVER *);

// hardware specific functions
#if defined(ADI_TWI_HARDWARE)
ADI_INT_HANDLER(twi_hw_handler);
#endif

// pseudo driver specific functions
#if defined(ADI_TWI_PSEUDO)
u32 twi_pseudo_state(ADI_PDD_DRIVER *,u32);
void twi_pseudo_handler(ADI_PDD_DRIVER *,u32);
void twi_pseudo_callback_timer(void *,u32,void *);
#if defined(ADI_TWI_PSEUDO_ARBITRATION)
void twi_pseudo_callback_clock(void *,u32,void *);
#endif
u32 twi_pseudo_scl(twi_pseudo_port *,u32);
u32 twi_pseudo_sda(twi_pseudo_port *,u32);
void twi_pseudo_tx8(ADI_PDD_DRIVER *,u8);
u8 twi_pseudo_rx8(ADI_PDD_DRIVER *);
#endif

// master twi functions
void twi_master_service(ADI_PDD_DRIVER *,u16);
void twi_master_processed(ADI_PDD_DRIVER *,int);
void twi_master_kill(ADI_PDD_DRIVER *,int);

// slave twi functions
void twi_slave_service(ADI_PDD_DRIVER *,u16);
void twi_slave_rx_processed(ADI_PDD_DRIVER *,u32,u32);
void twi_slave_rx_need(ADI_PDD_DRIVER *);
void twi_slave_rx(ADI_PDD_DRIVER *);
void twi_slave_tx_processed(ADI_PDD_DRIVER *);
void twi_slave_tx_need(ADI_PDD_DRIVER *);
void twi_slave_tx(ADI_PDD_DRIVER *);

/*********************
    Moab
**********************/

#if defined(__ADSP_MOAB__)      /* Static functions for Moab class devices */

static u32 TwiSetPortControl(   /* configures the port control registers   */
    u8  TwiDeviceNumber     /* TWI Device Number */
);

#endif
/****************************************************************/



/*****************************************************************

TWI device driver interface functions: Standard device driver
    driver interface functions supported by the TWI device driver

    adi_pdd_open()      --  opens a TWI device
    adi_pdd_close()     --  closes a TWI device
    adi_pdd_control()   --  setup a TWI device
    adi_pdd_read()      --  read (slave mode only)
    adi_pdd_write()     --  write (slave mode only)
    adi_pdd_sequential()    --  sequential (master mode only)

******************************************************************/


/****************************************************************

Device driver entry points:

*****************************************************************/

ADI_DEV_PDD_ENTRY_POINT ADITWIEntryPoint={
    adi_pdd_open,
    adi_pdd_close,
    adi_pdd_read,
    adi_pdd_write,
    adi_pdd_control,
    adi_pdd_sequential
};


/********************************************************************

    Function: adi_pdd_open

    Description: entry point to prepare both driver and the TWI for
        control and then use.

********************************************************************/

static u32 adi_pdd_open(
    ADI_DEV_MANAGER_HANDLE  MgrHandle,  // device manager handle
    u32             DevNumber,  // device number
    ADI_DEV_DEVICE_HANDLE   DevHandle,  // device handle
    ADI_DEV_PDD_HANDLE  *pPDDHandle,    // pointer to PDD handle location
    ADI_DEV_DIRECTION   Direction,  // data direction

    void        *pCriticalRegionArg,    // critical region imask storage location
    ADI_DMA_MANAGER_HANDLE  DMAHandle,  // handle to the DMA manager
    ADI_DCB_HANDLE      DCBHandle,  // callback handle
    ADI_DCB_CALLBACK_FN DMCallback) // device manager callback function
{
    ADI_PDD_DRIVER *drv=&Device[DevNumber];

#if defined(ADI_DEV_DEBUG)
    if (DevNumber>=ADI_PDD_DRIVERS) return ADI_DEV_RESULT_BAD_DEVICE_NUMBER;
    // check that data direction is valid
    switch (Direction)
    {
        case ADI_DEV_DIRECTION_BIDIRECTIONAL: break;
        default: return ADI_DEV_RESULT_DIRECTION_NOT_SUPPORTED;
    }
#endif

    *pPDDHandle=(ADI_DEV_PDD_HANDLE *)drv;      // let DM know which we are
    {
        void *exit_critical=adi_int_EnterCriticalRegion(pCriticalRegionArg);
        bool in_use=drv->DevHandle?TRUE:FALSE;
        if (in_use==FALSE) drv->DevHandle=DevHandle;
        adi_int_ExitCriticalRegion(exit_critical);
        if (in_use) return ADI_DEV_RESULT_DEVICE_IN_USE;
    }

    // setup the driver variables
    drv->DCBHandle=DCBHandle;
    drv->DMCallback=DMCallback;
    drv->critical=pCriticalRegionArg;

    // setup the default driver configuration
    drv->config.mode=ADI_DEV_MODE_UNDEFINED;
    drv->config.frequency=100;  // clock frequency
    drv->config.duty_cycle=50;  // clock duty cycle
    drv->config.fifo=0x000C;    // initialise the fifo_ctl default value to word
    drv->config.lostarb=3;      // lostarb retry count
    drv->config.anak=1;     // anak retry count
    drv->config.dnak=1;     // dnak retry count

#if defined(ADI_TWI_PSEUDO)
    drv->pseudo=0;
#endif

    memset((void *)&drv->type,0,sizeof(drv->type));

    return  ADI_DEV_RESULT_SUCCESS;
}


/********************************************************************

    Function: adi_pdd_close

    Description: entry point to terminate a driver's services
        -   set TWI hardware to a neutral state
        -   remove interrupt service
        -   mark driver as ready to open again

********************************************************************/

static u32 adi_pdd_close(ADI_DEV_PDD_HANDLE pdd)  // should be a adi_ADI_PDD_DRIVER *
{
    ADI_PDD_DRIVER *drv=(ADI_PDD_DRIVER *)pdd;

    ADI_PDD_CHECK(drv,ADI_DEV_RESULT_SUCCESS);

    if (drv->DevHandle==NULL) return ADI_DEV_RESULT_SUCCESS;

    twi_stop((ADI_PDD_DRIVER *)drv); // resets TWI leaving all interrupts masked
#if defined(ADI_TWI_PSEUDO)
    if (drv->pseudo)
    {
        free(drv->pseudo);
        drv->pseudo=0;
    }
#endif
    drv->DevHandle=NULL;
    drv->twi = 0;
    return  ADI_DEV_RESULT_SUCCESS ;
}


/********************************************************************

    Function: adi_pdd_control

    Description: entry point allowing configuration of the driver and/or
        TWI hardware.

********************************************************************/

static u32 adi_pdd_control(
    ADI_DEV_PDD_HANDLE pdd,         // will be a (adi_ADI_PDD_DRIVER *)
    u32 cmd,                // an enum
    void *opt)              // extra information depending on cmd
{
    ADI_PDD_DRIVER *drv=(ADI_PDD_DRIVER *)pdd;
    twi_registers *twi;
    u32 flag;
    u32 Result;

    ADI_PDD_CHECK(drv,ADI_DEV_RESULT_BAD_PDD_HANDLE);

    twi=drv->twi;
    switch (cmd)
    {
        case ADI_DEV_CMD_GET_PERIPHERAL_DMA_SUPPORT:
            *((u32 *)opt)=FALSE;
            break;

        case ADI_DEV_CMD_SET_DATAFLOW_METHOD:
            if (drv->config.mode!=(ADI_DEV_MODE)opt)
            {
                // stop the driver
                flag=twi_stop(drv);

                switch ((drv->config.mode=(ADI_DEV_MODE)opt))
                {
                    case ADI_DEV_MODE_CHAINED:  // slave mode
                    case ADI_DEV_MODE_SEQ_CHAINED:  // master mode
                        // restart driver if it was stopped,
                        // otherwise success
                        return flag==ADI_DEV_RESULT_SUCCESS
                            ?twi_start(drv):ADI_DEV_RESULT_SUCCESS;
                    default:
                        drv->config.mode=ADI_DEV_MODE_UNDEFINED;
                        return ADI_DEV_RESULT_NOT_SUPPORTED;
                }
            }
            break;

        case ADI_DEV_CMD_SET_DATAFLOW:
            // if opt is set then start the TWI, otherwise stop it
            opt?twi_start(drv):twi_stop(drv);
            Result = ADI_DEV_RESULT_SUCCESS;

#if defined(__ADSP_MOAB__)
            // validate device pointer
            if (drv != &Device[0] && drv != &Device[1])
                return ADI_DEV_RESULT_BAD_DEVICE_NUMBER;

            /* Enable TWI signals for Moab class device */
            if (drv == &Device[0])
            {
                Result = TwiSetPortControl(0);
            }
            else if (drv == &Device[1])
            {
                Result = TwiSetPortControl(1);
            }
#endif
            return(Result);

        case ADI_TWI_CMD_SET_RATE:
            // stop the driver
            flag=twi_stop(drv);

            // check the requested TWI frequency and duty cycle values
            if (((adi_twi_bit_rate *)opt)->frequency<20||
                ((adi_twi_bit_rate *)opt)->frequency>400||
                ((adi_twi_bit_rate *)opt)->duty_cycle<1||
                ((adi_twi_bit_rate *)opt)->duty_cycle>99)
                return ADI_TWI_RESULT_BAD_RATE;
            // requested values seem to be ok so save them
            drv->config.frequency=((adi_twi_bit_rate *)opt)->frequency;
            drv->config.duty_cycle=((adi_twi_bit_rate *)opt)->duty_cycle;

            // restart the driver if it was stopped, otherwise success
            return flag==ADI_DEV_RESULT_SUCCESS?twi_start(drv):ADI_DEV_RESULT_SUCCESS;

        // configure driver to a hardware TWI, opt points to the registers
#if defined(ADI_TWI_HARDWARE)
        case ADI_TWI_CMD_SET_HARDWARE:

            // stop the driver
            flag=twi_stop(drv);

            // if a pseudo configuration exists then remove it
#if defined(ADI_TWI_PSEUDO)
            if (drv->pseudo)
            {
                free(drv->pseudo);
                drv->pseudo=0;
            }
            drv->twi=0;
#endif  // ADI_TWI_PSEUDO

            // set the new TWI registers to point to memory mapped registers
#if defined(__ADSP_EDINBURGH__)
            return ADI_DEV_RESULT_NOT_SUPPORTED;
#endif  // __ADSP_EDINBURGH__

#if defined(__ADSP_BRAEMAR__)
            switch ((ADI_INT_PERIPHERAL_ID)opt)
            {
                case ADI_INT_TWI:
                    drv->pid=(ADI_INT_PERIPHERAL_ID)opt;
                    drv->twi=(twi_registers *)TWI_CLKDIV;
                    break;
                default:
                    return ADI_DEV_RESULT_NOT_SUPPORTED;
            }
#endif  // __ADSP_BRAEMAR__

#if defined(__ADSP_TETON__)
            return ADI_DEV_RESULT_NOT_SUPPORTED;
#endif  // __ADSP_TETON__

#if defined(__ADSP_STIRLING__)
            switch ((ADI_INT_PERIPHERAL_ID)opt)
            {
                case ADI_INT_TWI0:
                    drv->pid=(ADI_INT_PERIPHERAL_ID)opt;
                    drv->twi=(twi_registers *)TWI0_CLKDIV;
                    break;
                case ADI_INT_TWI1:
                    drv->pid=(ADI_INT_PERIPHERAL_ID)opt;
                    drv->twi=(twi_registers *)TWI1_CLKDIV;
                    break;
                default:
                    return ADI_DEV_RESULT_NOT_SUPPORTED;
            }
#endif  // __ADSP_STIRLING__

#if defined(__ADSP_MOAB__)  // settings for Moab class devices
            switch ((ADI_INT_PERIPHERAL_ID)opt)
            {
                case ADI_INT_TWI0:
                    drv->pid=(ADI_INT_PERIPHERAL_ID)opt;
                    drv->twi=(twi_registers *)0xFFC00700;
                    break;
                case ADI_INT_TWI1:
                    drv->pid=(ADI_INT_PERIPHERAL_ID)opt;
                    drv->twi=(twi_registers *)0xFFC02200;
                    break;
                default:
                    return ADI_DEV_RESULT_NOT_SUPPORTED;
            }
#endif  // Moabs

#if defined(__ADSP_KOOKABURRA__) || defined(__ADSP_MOCKINGBIRD__)
            switch ((ADI_INT_PERIPHERAL_ID)opt)
            {
                case ADI_INT_TWI:
                    drv->pid=(ADI_INT_PERIPHERAL_ID)opt;
                    drv->twi=(twi_registers *)TWI_CLKDIV;
                    break;
                default:
                    return ADI_DEV_RESULT_NOT_SUPPORTED;
            }
#endif  // __ADSP_KOOKABURRA__ or __ADSP_MOCKINGBIRD__

            // return after restarting the driver
            return flag==ADI_DEV_RESULT_SUCCESS?twi_start(drv):ADI_DEV_RESULT_SUCCESS;

#endif  //(ADI_TWI_HARDWARE)



        // configure driver to a software TWI, opt points to a adi_twi_pseudo
        case ADI_TWI_CMD_SET_PSEUDO:
#if defined(ADI_TWI_PSEUDO)
            if (!opt) return ADI_TWI_RESULT_BAD_PSEUDO;

            // stop the driver
            flag=twi_stop(drv);
            drv->twi=0;

            // check that there has been memory allocated for the pseudo driver
            if (!drv->pseudo)
            {
                // no! then allocate some memory for it then, if theres a problem
                // then return ADI_DEV_RESULT_NO_MEMORY
                if (!(drv->pseudo=calloc(sizeof(twi_pseudo_port),1)))
                    return ADI_DEV_RESULT_NO_MEMORY;
            }
            // copy the new pseudo settings into the driver configuration
            memcpy(&(drv->pseudo->config),opt,sizeof(adi_twi_pseudo_port));
            drv->twi=&(drv->pseudo->twi);

            // restart the driver if it was stopped, otherwise success
            return flag==ADI_DEV_RESULT_SUCCESS?twi_start(drv):ADI_DEV_RESULT_SUCCESS;
#else
            return ADI_DEV_RESULT_NOT_SUPPORTED;
#endif

        case ADI_TWI_CMD_SET_SLAVE_ADDR:
            // no harm in accepting this in Master mode
            if ((u32)opt<=7||(u32)opt>=0x7C) return ADI_TWI_RESULT_BAD_ADDR;
            twi->slave_addr.reg=(u16)(u32)opt;
            break ;

        case ADI_TWI_CMD_SET_GCALL:
            // needing an "off" seemed unlikely
            twi->slave_ctl.reg|=0x0010;
            break;

        case ADI_TWI_CMD_SET_SCCB:
            // needing an "off" seemed unlikely
            twi->control.reg|=0x0200;
            break;

        case ADI_TWI_CMD_SET_SCLOVR:
            if (opt) twi->master_ctl.reg|=0x8000;
            else twi->master_ctl.reg&=0x7FFF;
#if defined(ADI_TWI_PSEUDO)
            twi_pseudo_scl(drv->pseudo,FALSE);
#endif
            break;

        case ADI_TWI_CMD_SET_SDAOVR:
            if (opt) twi->master_ctl.reg|=0x4000;
            else twi->master_ctl.reg&=0xBFFF;
#if defined(ADI_TWI_PSEUDO)
            twi_pseudo_sda(drv->pseudo,FALSE);
#endif
            break;

        case ADI_TWI_CMD_SET_FIFO:
            twi->fifo_ctl.reg=drv->config.fifo=(u8)((u32)opt&0x000C);
            break;

        case ADI_TWI_CMD_SET_LOSTARB:
            drv->config.lostarb=(u8)(u32)opt;
            break;

        case ADI_TWI_CMD_SET_ANAK:
            drv->config.anak=(u8)(u32)opt;
            break;

        case ADI_TWI_CMD_SET_DNAK:
            drv->config.dnak=(u8)(u32)opt;
            break;

        case ADI_TWI_CMD_GET_SENSE:
            // return BUSBUSY, SCLSEN, and SDASEN bits (in their respective positions) from MASTER_STAT
#if defined(ADI_TWI_PSEUDO_ARBITRATION)
            // if pseudo and pseudo->config.scl_pid==NULL
            if (drv->pseudo&&drv->pseudo->config.scl_pid==(ADI_INT_PERIPHERAL_ID)NULL)
            {
                u32 scl,sda;
                adi_flag_Sense(drv->pseudo->config.scl,&scl);
                adi_flag_Sense(drv->pseudo->config.sda,&sda);
                drv->twi->master_stat.reg
                    =(drv->twi->master_stat.reg&0x00C0)|(scl?0x0000:0x0080)|(sda?0x0000:0x0040);
            }
#endif
            *(u16 *)opt=drv->twi->master_stat.reg&0x01C0;
            break;

        default:
            return ADI_DEV_RESULT_NOT_SUPPORTED;
    }
    return  ADI_DEV_RESULT_SUCCESS ;
}


/********************************************************************

    Function: adi_pdd_read

    Description: submit buffer(s) to receive inbound Slave mode data
        If the driver has marked slave.r_pend, then this function must
        restart the driver by pulling data from the inbound TWI FIFO.

    Note: used in TWI slave mode only

********************************************************************/

static u32 adi_pdd_read(
    ADI_DEV_PDD_HANDLE pdd,     // TWI device handle
    ADI_DEV_BUFFER_TYPE type,
    ADI_DEV_BUFFER *buf)
{
    ADI_PDD_DRIVER *drv=(ADI_PDD_DRIVER *)pdd;
    void *exit_critical;
    ADI_DEV_1D_BUFFER *buffer;

    ADI_PDD_CHECK(drv,ADI_DEV_RESULT_BAD_PDD_HANDLE);


#if defined(ADI_DEV_DEBUG)
    if (!buf) return ADI_TWI_RESULT_NO_DATA;

    // check for compatible mode
    switch (drv->config.mode)
    {
        case ADI_DEV_MODE_CHAINED: break;
        default: return  ADI_DEV_RESULT_DATAFLOW_INCOMPATIBLE;
    }

    // check for compatible buffer types
    switch (type)
    {
        case ADI_DEV_1D: break;
        default: return ADI_DEV_RESULT_BUFFER_TYPE_INCOMPATIBLE;
    }

    // check the new buffer
    for (buffer=&buf->OneD;buffer;buffer=buffer->pNext)
    {
        if (!buffer->ElementCount) return  ADI_TWI_RESULT_NO_DATA;
        if (buffer->ElementWidth!=1) return ADI_DEV_RESULT_BUFFER_TYPE_INCOMPATIBLE;
    }
#endif

    exit_critical=adi_int_EnterCriticalRegion(drv->critical);
    if (!(buffer=drv->type.buffer_rx))
    {
        // set the receive buffer to point to the new buffer chain and check if there
        // any receives pending
        drv->type.buffer_rx=&buf->OneD;
        if (drv->type.pend_rx) twi_slave_rx(drv);
    } else
    {
        // find the end of the receive buffer chain and add the new buffer chain
        while (buffer->pNext) buffer=buffer->pNext;
        buffer->pNext=&buf->OneD;
    }
    adi_int_ExitCriticalRegion(exit_critical);

    return ADI_DEV_RESULT_SUCCESS;
}


/********************************************************************

    Function: adi_pdd_write

    Description: submit buffer(s) containing outbound Slave mode data.
        Used buffers are returned by slave_xmt_dispatch()
        with the .ProcessedElementCount field showing
        how much was sent before SCOMP (I2C Stop bit).
        If the driver has marked .slave.w_pend, then this
        call must transfer outbound data to the TWI FIFO to
        get the driver going again.

    Note: used in TWI slave mode only

********************************************************************/

static u32 adi_pdd_write(
    ADI_DEV_PDD_HANDLE pdd,     // TWI device handle
    ADI_DEV_BUFFER_TYPE type,
    ADI_DEV_BUFFER *buf)
{
    ADI_PDD_DRIVER *drv=(ADI_PDD_DRIVER *)pdd;
    void *exit_critical;
    ADI_DEV_1D_BUFFER *buffer;

    ADI_PDD_CHECK(drv,ADI_DEV_RESULT_BAD_PDD_HANDLE);


#if defined(ADI_DEV_DEBUG)
    if (!buf) return ADI_TWI_RESULT_NO_DATA;

    // check for compatible mode
    switch (drv->config.mode)
    {
        case ADI_DEV_MODE_CHAINED: break;
        default: return  ADI_DEV_RESULT_DATAFLOW_INCOMPATIBLE;
    }

    // check for compatible buffer types
    switch (type)
    {
        case ADI_DEV_1D: break;
        default: return ADI_DEV_RESULT_BUFFER_TYPE_INCOMPATIBLE;
    }

    // check the new buffer
    for (buffer=&buf->OneD;buffer;buffer=buffer->pNext)
    {
        if (!buffer->ElementCount)
            return  ADI_TWI_RESULT_NO_DATA;
        if (buffer->ElementWidth!=1)
            return ADI_DEV_RESULT_BUFFER_TYPE_INCOMPATIBLE;
    }
#endif

    exit_critical=adi_int_EnterCriticalRegion(drv->critical);
    if (!(buffer=drv->type.buffer_tx))
    {
        // set the transmit buffer to the new buffer
        drv->type.buffer_tx=&buf->OneD;
        if (drv->type.pend_tx) twi_slave_tx(drv);
    } else
    {
        // find the end of the transmit buffer chain
        // and add the new buffer chain
        while (buffer->pNext) buffer=buffer->pNext;
        buffer->pNext=&buf->OneD;
    }
    adi_int_ExitCriticalRegion(exit_critical);

    return ADI_DEV_RESULT_SUCCESS;
}


/********************************************************************

    Function: adi_pdd_sequential

    Description: submit buffer(s) containing Master mode data.

    Note: Used in TWI master mode only

********************************************************************/

static u32 adi_pdd_sequential(
    ADI_DEV_PDD_HANDLE pdd,         // TWI device handle
    ADI_DEV_BUFFER_TYPE type,
    ADI_DEV_BUFFER *buf)
{
    ADI_PDD_DRIVER *drv=(ADI_PDD_DRIVER *)pdd;
    void *exit_critical;
    ADI_DEV_SEQ_1D_BUFFER *buffer;
    u8 start;
    u32 temp;

    ADI_PDD_CHECK(drv,ADI_DEV_RESULT_BAD_PDD_HANDLE);


    // check the buffer chain for ADI_TWI_RSTART buffers and if any are found
    // then update the address if it is 0x00 to point to the previous buffers
    // address
    for (temp=0,buffer=&buf->Seq1D;
        buffer;
        temp=(u32)buffer->Buffer.pAdditionalInfo&0x7F,
    // grab the address part
        buffer=(ADI_DEV_SEQ_1D_BUFFER *)(buffer->Buffer.pNext))
    {
        // if address is equal to ADI_TWI_RSTART and the address part is blank
        // then we use the previous transfer address, ADI_TWI_RSTART is not
        // valid for the first buffer so
        if ((u32)buffer->Buffer.pAdditionalInfo==ADI_TWI_RSTART)
        {
            // add previous buffer address
            buffer->Buffer.pAdditionalInfo
                = (void *)((u32)buffer->Buffer.pAdditionalInfo|temp);
        }
    }

#if defined(ADI_DEV_DEBUG)
    if (!buf) return ADI_TWI_RESULT_NO_DATA;

    // check for compatible mode
    switch (drv->config.mode)
    {
        case ADI_DEV_MODE_SEQ_CHAINED: break;
        default: return ADI_DEV_RESULT_DATAFLOW_INCOMPATIBLE;
    }

    // check for compatible buffer types
    switch (type)
    {
        case ADI_DEV_SEQ_1D: break;
        default: return ADI_DEV_RESULT_BUFFER_TYPE_INCOMPATIBLE;
    }

    buffer=&buf->Seq1D;
    if (((u32)buffer->Buffer.pAdditionalInfo&ADI_TWI_RSTART)==ADI_TWI_RSTART)
        return ADI_TWI_RESULT_BAD_ADDR; // can't start with RSTART

    // check the new buffer chain
    for (temp=0,buffer=&buf->Seq1D;
        buffer;
        buffer=(ADI_DEV_SEQ_1D_BUFFER *)(buffer->Buffer.pNext))
    {
        // grab the address part
        temp=(u32)buffer->Buffer.pAdditionalInfo&0x7F;
        if (!(temp>7||temp<0x7C
            || (temp==0&&buffer->Direction==ADI_DEV_DIRECTION_OUTBOUND)))
            return ADI_TWI_RESULT_BAD_ADDR;

        if (!buffer->Buffer.ElementCount)
            return  ADI_TWI_RESULT_NO_DATA;

        if (buffer->Buffer.ElementWidth!=1)
            return ADI_DEV_RESULT_BUFFER_TYPE_INCOMPATIBLE;

        switch (buffer->Direction)
        {
            case ADI_DEV_DIRECTION_INBOUND:
            case ADI_DEV_DIRECTION_OUTBOUND:
                break; // ok
            default: return ADI_DEV_RESULT_DIRECTION_NOT_SUPPORTED;
        }
    }
#endif

    // critical region protection is needed to update the queue used by the ISR

    exit_critical=adi_int_EnterCriticalRegion(drv->critical);
    if (!(start=drv->type.buffer?0:1))
    {
        // if there is already a buffer then find the last one and add the new
        // buffer to it
        buffer=drv->type.buffer;
        while (buffer->Buffer.pNext) buffer=(ADI_DEV_SEQ_1D_BUFFER *)(buffer->Buffer.pNext);
        buffer->Buffer.pNext=(ADI_DEV_1D_BUFFER *)(&buf->Seq1D);
    } else  drv->type.buffer=&buf->Seq1D; // otherwise update the buffer
    adi_int_ExitCriticalRegion(exit_critical);

    // if queue was empty, then get the driver going again
    // note: this need not be protected from interrupts since
    // the transfer does not begin until the last line of master_start(), thus even if
    // an interrupt occurs immediately after the transfer is started.
    if (start) twi_master_service(drv,0);

    return  ADI_DEV_RESULT_SUCCESS;
}

/*****************************************************************************/



/******************************************************************************

General TWI functions

    twi_start()
    twi_stop()
    twi_flush()
    twi_callback()
    twi_handler()

- Note.

******************************************************************************/


/********************************************************************

    Function: twi_start

    Description: Starts the TWI up
        -   uses the config part of the datastruct to setup the TWI
        -   set FAST mode if SCL is > 100 kHz

********************************************************************/

u32 twi_start(ADI_PDD_DRIVER *drv)
{
    u32 sclk,temp;
#if defined(ADI_TWI_HARDWARE)
    u16 hi,lo;
#endif
    u32 Code=ADI_DEV_RESULT_SUCCESS;

    ADI_PDD_CHECK(drv,ADI_DEV_RESULT_BAD_PDD_HANDLE);


    // check to see if its already started
    if (drv->twi->control.reg&0x0080) return ADI_DEV_RESULT_SUCCESS;

    // get the system clock frequency
    adi_pwr_GetFreq(&temp,&sclk,&temp);

    twi_flush(drv);

    // hook the interrupt code
#if defined(ADI_TWI_PSEUDO)
    if (drv->pseudo)
    {
        // configuration for pseudo TWI timer
        u32 period=sclk/(drv->config.frequency*2000);
        ADI_TMR_GP_CMD_VALUE_PAIR config[]=
        {
            {ADI_TMR_GP_CMD_SET_PERIOD,(void *)(period>ADI_TWI_PSEUDO_PERIOD?period:ADI_TWI_PSEUDO_PERIOD)},
            {ADI_TMR_GP_CMD_SET_WIDTH,(void *)(sclk)},

            // PWM mode
            {ADI_TMR_GP_CMD_SET_TIMER_MODE,(void *)1},

            // 0x70009 - set PULSE_HI
            // Value = TRUE - positive action pulse
            // value = FALSE - negative action pulse
            {ADI_TMR_GP_CMD_SET_PULSE_HI,(void *)0},

            // count to the end of the width
            {ADI_TMR_GP_CMD_SET_COUNT_METHOD,(void *)TRUE},

            // enable callback interrupt
            {ADI_TMR_GP_CMD_SET_INTERRUPT_ENABLE,(void *)TRUE},


            // 0x7000c - selects the timer input
            // Value = TRUE - UART_RX or PPI_CLK FALSE - TMRx or PF1
            {ADI_TMR_GP_CMD_SET_INPUT_SELECT,(void *)TRUE},

            // 0x7000d - disables the output pad in PWM mode
            // Value = TRUE - disabled, FALSE - ENABLED
            {ADI_TMR_GP_CMD_SET_OUTPUT_PAD_DISABLE,(void *)FALSE},

            // clock source is SCLK (120000000)
            {ADI_TMR_GP_CMD_SET_CLOCK_SELECT,(void *)0},

            // 0x7000f - sets toggle hi state
            // Value = TRUE - PULSE_HI alternates each period
            // value = FALSE - use programmed state
            {ADI_TMR_GP_CMD_SET_TOGGLE_HI,(void *)FALSE},

            // 0x70010 - run timer during emulation mode
            // Value = TRUE - timer runs during emulation mode, FALSE - timer doesn't run
            {ADI_TMR_GP_CMD_RUN_DURING_EMULATION,(void *)FALSE},

            // clear any pending interrupts
            {ADI_TMR_GP_CMD_CLEAR_INTERRUPT,(void *)TRUE},

            // dont start right away
            {ADI_TMR_GP_CMD_ENABLE_TIMER,(void *)FALSE},
            {ADI_TMR_GP_CMD_END,(void *)0},
        };
        twi_pseudo_port *pseudo=drv->pseudo;

        // initialise the pseudo port variables
        pseudo->busy_count=pseudo->busy_reload=2;
        pseudo->state=TWI_STATE_PSEUDO_DISABLED;
        pseudo->flags=0x0000;
        pseudo->run_once=false;

        Code=adi_flag_Open(pseudo->config.sda);
        if (Code==ADI_DEV_RESULT_SUCCESS) Code=adi_flag_Open(pseudo->config.scl);
        if (Code==ADI_DEV_RESULT_SUCCESS) Code=adi_tmr_Open(pseudo->config.timer);

        if (Code==ADI_DEV_RESULT_SUCCESS)
        {
            // set the pseudo twi flags to high
            twi_pseudo_scl(pseudo,TRUE);
            twi_pseudo_sda(pseudo,TRUE);

#if defined(ADI_TWI_PSEUDO_ARBITRATION)
            // setup scl callback fucntion, only for master arbitration
            if (pseudo->config.scl_pid&&!drv->twi->control.reg&0x0200)
            {
                Code=adi_flag_InstallCallback(
                    pseudo->config.scl,
                    pseudo->config.scl_pid,
                    ADI_FLAG_TRIGGER_BOTH_EDGES,
                    TRUE,
                    (void *)drv,
                    NULL, // dont use callback manager for this, because it needs to react fast
                    twi_pseudo_callback_clock);
            }
#endif
        }

        // install and setup the pseudo twi timer
        if (Code==ADI_DEV_RESULT_SUCCESS) Code=adi_tmr_GPControl(pseudo->config.timer,ADI_TMR_GP_CMD_TABLE,(void *)config);
        if (Code==ADI_DEV_RESULT_SUCCESS) Code=adi_tmr_InstallCallback(
            pseudo->config.timer,
            TRUE,
            (void *)drv,
            NULL, //drv->DCBHandle,
            twi_pseudo_callback_timer);
        if (Code==ADI_DEV_RESULT_SUCCESS)
            Code=adi_tmr_GPControl(drv->pseudo->config.timer,ADI_TMR_GP_CMD_ENABLE_TIMER,(void *)TRUE);
    } else
    {
#endif
#if defined(ADI_TWI_HARDWARE)
        // set pre-scaler in CONTROL for the 10 Mhz rate needed by the TWI
        temp=(sclk+9999999)/10000000;
        if (temp&0xFF80) return ADI_TWI_RESULT_BAD_RATE;

        // set the control prescalar
        drv->twi->control.reg=(drv->twi->control.reg&0xFF00)|temp;

        // set divisors in CLKDIV
        // total cycle divisor; round UP; 10000 = the 10 MHz from pre-scaler
        temp=10000/drv->config.frequency;
        hi=(temp*drv->config.duty_cycle)/100;
        lo=temp-hi;

        // check for over or under flows and bad combination of kHz and duty cycle
        if (hi>255||lo>255||hi==0||lo==0) return ADI_TWI_RESULT_BAD_RATE;
        drv->twi->clkdiv.reg=(u16)((hi<<8)|lo);

        // set FAST mode bit if over 100 kHz
        if (drv->config.frequency>100) drv->twi->master_ctl.reg|=0x0008;

        // allocate IVG handle for and attach TWI service to it
#if defined(ADI_TWI_IVG)
        drv->ivg=ADI_TWI_IVG;
        adi_int_SICSetIVG(drv->pid,drv->ivg);
#else
        adi_int_SICGetIVG(drv->pid,&drv->ivg);
#endif
        Code==adi_int_CECHook(drv->ivg,twi_hw_handler,drv,TRUE);
        if (Code==ADI_DEV_RESULT_SUCCESS) adi_int_SICWakeup(drv->pid,TRUE);
        if (Code==ADI_DEV_RESULT_SUCCESS) adi_int_SICEnable(drv->pid);
        Code=Code==ADI_DEV_RESULT_SUCCESS?Code:ADI_DEV_RESULT_CANT_HOOK_INTERRUPT;
#endif
#if defined(ADI_TWI_PSEUDO)
    }
#endif
    // the driver has now started so set TWI_ENA flag
    drv->twi->control.reg|=0x0080;

    // enable all either slave or master mode in the twi
    switch (drv->config.mode)
    {
        case ADI_DEV_MODE_CHAINED:      // slave mode
#if defined(ADI_TWI_PSEUDO)
            // slave mode not supported with pseudo twi
            if (drv->pseudo)
            {
                Code=ADI_DEV_RESULT_NOT_SUPPORTED;
                break;
            }
#endif
            drv->twi->fifo_ctl.reg=0x000C; //drv->config.fifo;
            drv->type.buffer_tx=drv->type.buffer_rx=0;
            drv->type.pend_tx=drv->type.pend_rx=
                drv->type.sdir=
                drv->type.gcall=0;
            drv->twi->int_mask.reg=0x00CF;      // pass slave interrupts only
            drv->twi->slave_ctl.reg|=0x0005;    // set STDVAL and SEN
            drv->DCBHandle==NULL;               // dont use DCB for slave mode
            break;

        case ADI_DEV_MODE_SEQ_CHAINED:  // master mode
            // setup master interrupts only
            drv->twi->int_mask.reg=0x00F0;
            break;

        default:
            // otherwise stop the driver and return
            twi_stop(drv);
            Code=ADI_DEV_RESULT_NOT_SUPPORTED;
    }

    if (Code!=ADI_DEV_RESULT_SUCCESS) twi_stop(drv);
    return Code;
}


/********************************************************************

    Function: twi_stop

    Description: set TWI to a neutral state, return TRUE if the TWI
        driver was running, otherwise return FALSE
        -   disable I2C services (both Master and Slave)
        -   flush FIFOs
        -   clear interrupts

********************************************************************/

u32 twi_stop(ADI_PDD_DRIVER *drv)
{
    ADI_PDD_CHECK(drv,ADI_DEV_RESULT_BAD_PDD_HANDLE);

    // if stopped the we're done
    if (!drv->twi||!(drv->twi->control.reg&0x0080)) return ADI_DEV_RESULT_FAILED;

    drv->twi->int_mask.reg=0x0000;          // mask all interrupts
    drv->twi->slave_ctl.reg=0x0000;         // kill SEN
    drv->twi->master_ctl.reg=0x0000;        // kill MEN
    drv->twi->master_stat.reg=0x003E;       // clear all 5 W1C error bits
    drv->twi->int_stat.reg=0x00FF;          // clear all interrupts

    twi_flush(drv);

#if defined(ADI_TWI_PSEUDO)
    if (drv->pseudo)
    {
        twi_pseudo_port *pseudo=drv->pseudo;

#if defined(ADI_TWI_PSEUDO_ARBITRATION)
        if (pseudo->config.scl_pid&&!drv->twi->control.reg&0x0200)
            adi_flag_RemoveCallback(pseudo->config.scl);
#endif
        // unhook the timer and flag callbacks
        adi_tmr_GPControl(pseudo->config.timer,ADI_TMR_GP_CMD_ENABLE_TIMER,(void *)FALSE);
        adi_tmr_RemoveCallback(pseudo->config.timer);
        adi_tmr_Close(pseudo->config.timer);
        adi_flag_Close(pseudo->config.scl);
        adi_flag_Close(pseudo->config.sda);
    } else if (drv->pid)
    {
#endif
        // unhook the interrupt service
#if defined(ADI_TWI_HARDWARE)
        if (adi_int_SICDisable(drv->pid)) return ADI_DEV_RESULT_CANT_UNHOOK_INTERRUPT;
        if (adi_int_SICWakeup(drv->pid,FALSE)) return ADI_DEV_RESULT_CANT_UNHOOK_INTERRUPT;
        if (adi_int_CECUnhook(drv->ivg,twi_hw_handler,drv)) return ADI_DEV_RESULT_CANT_UNHOOK_INTERRUPT;
#endif
#if defined(ADI_TWI_PSEUDO)
    }
#endif

    // the driver has now been stopped so clear TWI_ENA flag
    drv->twi->control.reg&=0xFF7F;

    // disable all either slave or master mode in the twi
    switch (drv->config.mode)
    {
        case ADI_DEV_MODE_CHAINED:      // slave mode
            // if a move was in progress dispatch the buffer by faking SCOMP
            if ((drv->type.buffer_rx&&drv->type.buffer_rx->ProcessedElementCount<drv->type.buffer_rx->ElementCount)||
                (drv->type.buffer_tx&&drv->type.buffer_tx->ProcessedElementCount<drv->type.buffer_tx->ElementCount))
                twi_slave_service(drv,0x0002);
            break;

        case ADI_DEV_MODE_SEQ_CHAINED:  // master mode
            break;

        default: return ADI_DEV_RESULT_NOT_SUPPORTED;
    }

    // ok its all stopped now
    return ADI_DEV_RESULT_SUCCESS;
}


/********************************************************************

    Function: twi_flush

    Description: flush the both fifo buffers and clears the current
        data buffer

********************************************************************/

u32 twi_flush(ADI_PDD_DRIVER *drv)
{
    ADI_PDD_CHECK(drv,ADI_DEV_RESULT_BAD_PDD_HANDLE);


#if defined(ADI_TWI_PSEUDO)
    if (!drv->pseudo)
    {
#endif
        // flush both rx and tx fifo's
        drv->twi->fifo_ctl.reg|=0x0003;
        drv->twi->fifo_ctl.reg&=0xFFFC;
#if defined(ADI_TWI_PSEUDO)
    } else drv->twi->fifo_stat.reg&=0xFFF0; // flush the pseudo rx and tx fifo
#endif
    return ADI_DEV_RESULT_SUCCESS;
}


/********************************************************************

    Function: twi_callback

    Description: generate a callback, it checks for callback type etc.

********************************************************************/

u32 twi_callback(ADI_PDD_DRIVER *Handle,u32 Event,void *pArg)
{
    if (!Handle->DMCallback) return ADI_DEV_RESULT_SUCCESS;
    (Handle->DCBHandle)?adi_dcb_Post(Handle->DCBHandle,0,Handle->DMCallback,Handle->DevHandle,Event,pArg):
        (Handle->DMCallback)(Handle->DevHandle,Event,pArg);
    return ADI_DEV_RESULT_SUCCESS;
}


/********************************************************************

    Function: twi_handler

    Description: call the appropriate service routine, used by both
        hardware and pseudo TWI ports

********************************************************************/

ADI_INT_HANDLER_RESULT twi_handler(ADI_PDD_DRIVER *drv,u16 stat)
{
    // if drv is null then return error
    if (drv) (drv->config.mode==ADI_DEV_MODE_SEQ_CHAINED)?
        twi_master_service(drv,stat):twi_slave_service(drv,stat);
    return ADI_INT_RESULT_PROCESSED;
}

/*****************************************************************************/



/******************************************************************************

TWI buffer functions

    twi_buffer_transmit()
    twi_buffer_receive()

- Note. Buffer transmit and receive select between the hardware twi and pseudo
    twi. The pseudo twi only supports byte transfers at the moment. They also
    select beteewn the sequential io buffers and the read and write buffers in
    masster and slave mode.

******************************************************************************/


/********************************************************************

    Function: twi_buffer_transmit

    Description: Loads the next byte in the data buffer into the TWI
        fifo register and returns any errors or success

********************************************************************/

u32 twi_buffer_transmit(ADI_PDD_DRIVER *drv)
{
    ADI_DEV_1D_BUFFER *buffer;

    // if the fifo buffer is full or buffer data is null then we're done here
    if (!drv->twi||drv->twi->fifo_stat.reg&0x0003==3) return ADI_DEV_RESULT_SUCCESS;

    switch (drv->config.mode)
    {
        case ADI_DEV_MODE_CHAINED: buffer=drv->type.buffer_tx; break;
        case ADI_DEV_MODE_SEQ_CHAINED: buffer=drv->type.buffer?&(drv->type.buffer->Buffer):0; break;
    }

    // if nothing left in buffer return no data
    if (!buffer||buffer->ProcessedElementCount==buffer->ElementCount)
        return ADI_TWI_RESULT_NO_DATA;

    // transmit word or byte, depending on fifo control and fifo status register
#if defined(ADI_TWI_PSEUDO)
    if (!drv->pseudo)
    {
#endif
        switch (drv->twi->fifo_stat.reg&0x0003)
        {
            case 0:
                // check if word transfers are enabled and that there is more than
                // one byte to send, if so then load the xmt_data16 register
                if (drv->twi->fifo_ctl.reg&0x0004&&(buffer->ProcessedElementCount-buffer->ElementCount)>=2)
                {
                    // load the twi xmt_data16 register
                    drv->twi->xmt_data16.reg=
                        ((u8 *)buffer->Data)[buffer->ProcessedElementCount]+
                        (((u8 *)buffer->Data)[buffer->ProcessedElementCount+1]<<8);
                    buffer->ProcessedElementCount+=2;
                    break;
                }
            case 1:
                // load the twi xmt_data8 register
                drv->twi->xmt_data8.reg=((u8 *)buffer->Data)[buffer->ProcessedElementCount++];
        }
#if defined(ADI_TWI_PSEUDO)
    } else twi_pseudo_tx8(drv,((u8 *)buffer->Data)[buffer->ProcessedElementCount++]);
#endif

    // after the first byte is sent then set the fifo control register to the default
    drv->twi->fifo_ctl.reg=drv->config.fifo;

    // if its a larger (>254 byte) master transfer and we've send every thing then signal stop
    if (drv->twi->master_ctl.reg&0x0001&&buffer->ElementCount>254&&
        !(buffer->ElementCount-buffer->ProcessedElementCount))
        drv->twi->master_ctl.reg|=0x0010;

    return ADI_DEV_RESULT_SUCCESS;
}


/********************************************************************

    Function: twi_buffer_receive

    Description: adds a byte into the buffer and returns
        ADI_DEV_RESULT_SUCCESS for success
        ADI_TWI_RESULT_TOO_MUCH_DATA for buffer full

********************************************************************/

u32 twi_buffer_receive(ADI_PDD_DRIVER *drv)
{
    ADI_DEV_1D_BUFFER *buffer;
    u16 data,len;

    if (!drv->twi) return ADI_DEV_RESULT_SUCCESS;

    // get the number of bytes in the receive fifo
    if (!(len=(drv->twi->fifo_stat.reg>>2)&0x0003)) return ADI_DEV_RESULT_SUCCESS;

    switch (drv->config.mode)
    {
        case ADI_DEV_MODE_CHAINED:
            // if there is no buffer in slave mode then dont go any further
            // and wait for the buffer
            if (!(buffer=drv->type.buffer_rx)) return ADI_DEV_RESULT_SUCCESS;
            break;
        case ADI_DEV_MODE_SEQ_CHAINED:
            buffer=drv->type.buffer?&(drv->type.buffer->Buffer):0;
            break;
    }

#if defined(ADI_TWI_PSEUDO)
    if (drv->pseudo)
    {
        // if its a large transfer and there is under 3 bytes to send then set the
        // stop bit
        if (buffer&&buffer->ElementCount>254&&(buffer->ElementCount-buffer->ProcessedElementCount)<3)
            drv->twi->master_ctl.reg|=0x0010;
        data=twi_pseudo_rx8(drv);
        len=1;
    } else
    {
#endif
        data=len==1?drv->twi->rcv_data8.reg:drv->twi->rcv_data16.reg;

        // if its a large transfer and there is under 2 bytes to send then set the
        // stop bit
        if (buffer&&drv->twi->master_ctl.reg&0x0001&&buffer->ElementCount>254&&
            (buffer->ElementCount-buffer->ProcessedElementCount)<2)
            drv->twi->master_ctl.reg|=0x0010;
#if defined(ADI_TWI_PSEUDO)
    }
#endif

    // after the first byte is received then set the fifo control register to the default
    drv->twi->fifo_ctl.reg=drv->config.fifo;

    // if no buffer or buffer is full then return ADI_DEV_RESULT_FAILURE
    if (!buffer||buffer->ProcessedElementCount==buffer->ElementCount)
        return ADI_DEV_RESULT_FAILED;

    // otherwise write data to buffer
    switch (len)
    {
        case 3:
            if (buffer->ElementCount-buffer->ProcessedElementCount>=2)
            {
                ((u8 *)buffer->Data)[buffer->ProcessedElementCount++]=data;
                data>>=8;
            }
        case 1:
            ((u8 *)buffer->Data)[buffer->ProcessedElementCount++]=data;
            break;
    }

    return ADI_DEV_RESULT_SUCCESS;
}

/*****************************************************************************/



/******************************************************************************

Hardware TWI functions:

    twi_hw_handler()            -- interrupt callback

- Note. The hardware TWI functions consist only of the hardware handler callback,
    which clears the interrupt status register for the twi.

******************************************************************************/


/********************************************************************

    Function: twi_hw_handler

    Description: this is the interrupt handler. The main TWI interrupt handler
        hasn't much to do. It records the INT_STAT register and passes it on to
        the appropriate service routine, of which there are two, one for Master
        mode and the other for Slave mode. Master mode is marked by
        ADI_DEV_MODE_SEQ_CHAINED. Before exit, the interrupts serviced are
        cleared.

********************************************************************/

#if defined(ADI_TWI_HARDWARE)
ADI_INT_HANDLER(twi_hw_handler)
{
    // note: #define  ADI_INT_HANDLER(NAME)  ADI_INT_HANDLER_RESULT NAME (void *ClientArg)
    ADI_PDD_DRIVER *drv=(ADI_PDD_DRIVER *)ClientArg; // from ADI_INT_HANDLER macro
    u16 stat;

    {
        int i=0;
        while (drv!=&Device[i++]) if (i>=ADI_PDD_DRIVERS) return ADI_INT_RESULT_NOT_PROCESSED;
    }

    if (drv->twi->int_stat.reg&drv->twi->int_mask.reg==0) return ADI_INT_RESULT_NOT_PROCESSED;

    // process all relevent interrupts
    while ((stat=(drv->twi->int_stat.reg&drv->twi->int_mask.reg)))
    {
        // clear interrupts about to be serviced and execute the twi handler
        drv->twi->int_stat.reg=stat;
        twi_handler(drv,stat);
    }
    return ADI_INT_RESULT_PROCESSED;
}
#endif

/*****************************************************************************/



/******************************************************************************

Pseudo TWI functions:

    twi_pseudo_state()
    twi_pseudo_handler()
    twi_pseudo_callback_timer()
    twi_pseudo_callback_clock()
    twi_pseudo_scl()
    twi_pseudo_sda()
    twi_pseudo_tx8()
    twi_pseudo_rx8()


Note: The pseudo TWI uses
    1 x timer
    1 x timer callback
    2 x flags
    1 x flag callback (if master arbitration is used only)

- Currently the pseudo TWI only supports master mode, not slave. The pseudo TWI
    is made to behave like the hardware TWI in the BF537, it uses a pseudo
    register structure that operates in a similar fashion as the hardware one.
    Because of this the front end interface is the same as the hardware TWI.

******************************************************************************/


/********************************************************************

    Function: twi_pseudo_state

    Description: start the pseudo driver doing something

********************************************************************/

#if defined(ADI_TWI_PSEUDO)
u32 twi_pseudo_state(ADI_PDD_DRIVER *drv,u32 state)
{
    twi_pseudo_port *pseudo;

    // if MEN flag is not set then return
    if (!drv||!drv->twi||!(pseudo=drv->pseudo)||!(drv->twi->master_ctl.reg&0x0001))
        return ADI_DEV_RESULT_FAILED;

    switch (state)
    {
        case TWI_STATE_PSEUDO_START:
            // initialise some pseudo TWI registers
            drv->twi->master_stat.reg&=0xFFC1;
            break;

        case TWI_STATE_PSEUDO_RECEIVE:
            // setup pseudo transfer, if SCCB is set then ack bit is always one,
            // otherwise set only on last byte, note: this will stop the SCCB
            // transfer to read only one byte.
            pseudo->data_byte=(
                (((drv->twi->master_ctl.reg>>6)&0xFF)<=1)||
                (drv->twi->master_ctl.reg&0x0010)||
                (drv->twi->control.reg&0x0200)
                )?0x01FF:0x01FE;
            break;

        case TWI_STATE_PSEUDO_TRANSMIT:
            // setup pseudo transfer and update the pseudo TWI registers,
            // doesn't matter what the ack bit is in SCCB mode
            pseudo->data_byte=(drv->twi->xmt_data8.reg<<1)|0x0001;
            drv->twi->xmt_data8.reg=drv->twi->xmt_data8.reg>>8;
            drv->twi->fifo_stat.reg=(drv->twi->fifo_stat.reg&0xFFFC)|((drv->twi->fifo_stat.reg>>1)&0x0003);
            break;

        case TWI_STATE_PSEUDO_DISABLED:
            // clear the master transfer in progress (MPROG) bit
            drv->twi->master_stat.reg&=0xFFFE;

            // clear the master enable (MEN) bit
            drv->twi->master_ctl.reg&=0xFFFE;
    }

    // clear the clock and set the pseudo TWI drvier state
    drv->pseudo->clock=0;
    drv->pseudo->state=state;

    return ADI_DEV_RESULT_SUCCESS;
}


/********************************************************************

    Function: twi_pseudo_handler

    Description: the pseudo twi handler, this controls the pseudo
        twi state machine that is in the pseudo twi timer callback.
        This function is controlled by the pseudo twi registers, and
        updates the appropriate registers and calls the generic
        twi_handler function that is also used by the hardware twi.

********************************************************************/

void twi_pseudo_handler(ADI_PDD_DRIVER *drv,u32 event)
{
    twi_pseudo_port *pseudo=drv->pseudo;
    twi_registers *twi=drv->twi;

    pseudo->state=TWI_STATE_PSEUDO_WAIT;
    switch(event)
    {
        case TWI_EVENT_PSEUDO_STARTED:
            // load data_out with the master address and direction of transfer
            pseudo->data_byte=(twi->master_addr.reg<<2)|((twi->master_ctl.reg&0x0004)?0x0003:0x0001);

            // send the address
            twi_pseudo_state(drv,TWI_STATE_PSEUDO_ADDRESS);
            break;

        case TWI_EVENT_PSEUDO_RSTARTED:
            // the repeat start has been sent, so set MCOMP for last transfer
            twi->int_stat.reg=0x0010; // set MCOMP
            break;

        case TWI_EVENT_PSEUDO_ADDRESS:
            // check for ANAK in TWI mode, don't worry if in SCCB mode
            if (pseudo->data_byte&0x0001&&!(twi->control.reg&0x0200))
            {
                // ANAK detected so set the appropriate bit, and stop the transfer
                twi->master_stat.reg|=0x0004;
                twi->master_ctl.reg&=0xC01F;
            }

            // start the transfer unless if dlen is 0, if it is then stop
            twi_pseudo_state(drv,twi->master_ctl.reg&0x3FC0?
                (twi->master_ctl.reg&0x0004?TWI_STATE_PSEUDO_RECEIVE:TWI_STATE_PSEUDO_TRANSMIT):
                (drv,twi->master_ctl.reg&0x0020?TWI_STATE_PSEUDO_RSTART:TWI_STATE_PSEUDO_STOP));
            break;

        case TWI_EVENT_PSEUDO_RECEIVE:
            // put received byte into the fifo register
            twi->rcv_data8.reg=(pseudo->data_byte>>1)&0x00FF;
            twi->fifo_stat.reg|=0x0004; // set fifo status

            // check for end of transfer
            if (pseudo->data_byte&0x0001)
            {
                // end of transfer so clear dlen and either stop the transfer
                // or signal a repeat start
                twi->master_ctl.reg=twi->master_ctl.reg&0xC03F; // clear dlen
                twi_pseudo_state(drv,twi->master_ctl.reg&0x0020?TWI_STATE_PSEUDO_RSTART:TWI_STATE_PSEUDO_STOP);
            } else twi->int_stat.reg|=0x0080; // signal byte has been received
            break;

        case TWI_EVENT_PSEUDO_TRANSMIT:
            // check for DNAK
            if (pseudo->data_byte&0x0001)
            {
                // DNAK detected so set the appropriate bit, and stop the transfer
                twi->master_stat.reg|=0x0008;
                twi->master_ctl.reg&=0xC01F;
            }

            // check if stop bit has been set and fifo transmit buffer is empty or
            // dlen=0, if so then either signal stop or repeat start
            if (((twi->master_ctl.reg&0x0010)&&!(twi->fifo_stat.reg&0x03))||!(twi->master_ctl.reg&0x3FC0))
                twi_pseudo_state(drv,twi->master_ctl.reg&0x0020?TWI_STATE_PSEUDO_RSTART:TWI_STATE_PSEUDO_STOP);
            else twi->int_stat.reg|=0x0040; // signal byte has been sent
            break;

        case TWI_EVENT_PSEUDO_STOPPED:
            twi_pseudo_state(drv,TWI_STATE_PSEUDO_DISABLED);

            // if ANAK or DNAK then set MERR, otherwise set MCOMP
            twi->int_stat.reg|=(twi->master_stat.reg&0x000C)?0x0020:0x0010;
            break;

#if defined(ADI_TWI_PSEUDO_ARBITRATION)
        case TWI_EVENT_PSEUDO_LOSTARB:
            // set lostarb (LOSTARB) flag
            twi->master_stat.reg|=0x0002;

            // clear master transfer in progress (MPROG) flag
            twi->master_stat.reg&=0xFFFE;

            // set master transfer error (MERRM) flag
            twi->int_stat.reg|=0x0020;

            // clear master enable (MEN) flag
            twi->master_ctl.reg&=0xFFFE;

            // set the current pseudo TWI state to disabled
            twi_pseudo_state(drv,TWI_STATE_PSEUDO_DISABLED);
            break;
#endif
    }

    // if there has been an interrupt then execute the twi_handler routine
    if (twi->int_stat.reg)
    {
        twi_handler(drv,twi->int_stat.reg);

        // clear the interrupt status register
        twi->int_stat.reg=0x0000;
    }
}


/********************************************************************

    Function: twi_pseudo_callback_timer

    Description: the pseudo twi timer callback is the state machine
        for the pseudo twi driver and does all the bit banging.
        The start, stop, address transmit, data transmt/receive and
        repeat start states are all defined here. The driver is
        designed to be quite robust and should recover from most
        problems.
        Once a state has completed its task the pseudo twi handler is
        called to control the higher level functionality of the
        pseudo driver

********************************************************************/

void twi_pseudo_callback_timer(void *handle,u32 event,void *arg)
{
    twi_pseudo_port *pseudo;
    twi_registers *twi;
    u32 flag,i=0;

    if (event!=ADI_TMR_EVENT_TIMER_EXPIRED) return;

    while (handle!=&Device[i++]) if (i>=ADI_PDD_DRIVERS) return;// ADI_INT_RESULT_NOT_PROCESSED;
    pseudo=((ADI_PDD_DRIVER *)handle)->pseudo;

    //if (!pseudo||pseudo->run_once) return;
    pseudo->run_once=true;

    twi=((ADI_PDD_DRIVER *)handle)->twi;

    switch (pseudo->state)
    {
        case TWI_STATE_PSEUDO_START:
            twi->master_stat.reg|=0x0001;
            switch (pseudo->clock)
            {
                default: // start
                    // wait till the line is not busy
                    if (twi->master_stat.reg&0x0100) break;
                    if (!twi_pseudo_scl(pseudo,TRUE)) break;
                    if (!twi_pseudo_sda(pseudo,TRUE))
                    {
                        // if after 5 clock ticks toggle the clock pin
                        if (pseudo->unstick++>20)
                        {
                            twi_pseudo_scl(pseudo,FALSE);
                            pseudo->unstick=0;
                        }
                        break;
                    }
                    pseudo->clock=1;
                case 1:
                    pseudo->unstick=0;
                    if (!twi_pseudo_sda(pseudo,FALSE)) break;
                    pseudo->clock++;
                case 2: // clear the clock bit and start the address transfer
                    if (!twi_pseudo_scl(pseudo,FALSE))  break;
                    twi_pseudo_handler((ADI_PDD_DRIVER *)handle,TWI_EVENT_PSEUDO_STARTED);
            }
            break;

        case TWI_STATE_PSEUDO_RSTART:
            if (!twi_pseudo_scl(pseudo,FALSE)) break;
            if (!twi_pseudo_sda(pseudo,TRUE)) break;
            twi_pseudo_handler((ADI_PDD_DRIVER *)handle,TWI_EVENT_PSEUDO_RSTARTED);
            break;

        case TWI_STATE_PSEUDO_ADDRESS:
        case TWI_STATE_PSEUDO_TRANSMIT:
        case TWI_STATE_PSEUDO_RECEIVE:
            switch (pseudo->clock%3)
            {
                case 2: // read data, if clock is high then ok to read, otherwise wait
                    if (!twi_pseudo_scl(pseudo,TRUE)) break;
#if defined(ADI_TWI_PSEUDO_ARBITRATION)
                    if (pseudo->config.scl_pid&&!twi->control.reg&0x0200)
                    {
                        flag=twi->master_stat.reg&0x0040?0:1;
                        if (pseudo->state==TWI_STATE_PSEUDO_RECEIVE)
                        {
                            // check for lost arbitration on receive
                            if (pseudo->clock>=24&&(((pseudo->data_byte>>8)^flag)&1))
                            {
                                twi_pseudo_handler((ADI_PDD_DRIVER *)handle,
                                    TWI_EVENT_PSEUDO_LOSTARB);
                                break;
                            }
                        } else
                        {
                            // check for lost arbitration on address and transmit
                            if (pseudo->clock<24&&(((pseudo->data_byte>>8)^flag)&1))
                            {
                                twi_pseudo_handler((ADI_PDD_DRIVER *)handle,
                                    TWI_EVENT_PSEUDO_LOSTARB);
                                break;
                            }
                        }
                    } else
#endif
                    {
                        adi_flag_Sense(pseudo->config.sda,&flag);
                        flag=flag?1:0;
                    }

                    pseudo->data_byte=(pseudo->data_byte<<1)|flag;
                    pseudo->clock++;
                case 0: // set the data
                    if (!twi_pseudo_scl(pseudo,FALSE)) break;
                    pseudo->clock++;
                    if ((pseudo->flags&0x0080?1:0)^(pseudo->data_byte&0x0100?1:0))
                    {
                        twi_pseudo_sda(pseudo,pseudo->data_byte&0x0100);
                        break;
                    }
                case 1: // set the clock
                    twi_pseudo_scl(pseudo,TRUE);
                    pseudo->clock++;
                    break;
            }
            if (pseudo->clock>=27)
            {
                // if we're transmitting or receiving a byte decrement the length
                // on the last clock cycle of the transfer
                if (pseudo->state==TWI_STATE_PSEUDO_TRANSMIT||pseudo->state==TWI_STATE_PSEUDO_RECEIVE)
                    twi->master_ctl.reg-=((twi->master_ctl.reg&0x3FC0)!=0x3FC0)?0x0040:0x0000;
                twi_pseudo_handler((ADI_PDD_DRIVER *)handle,pseudo->state);
            }
            break;

        case TWI_STATE_PSEUDO_STOP:
            switch (pseudo->clock)
            {
                default: // stop
                    if (!twi_pseudo_sda(pseudo,FALSE)) break;
                    if (!twi_pseudo_scl(pseudo,TRUE)) break;
                    pseudo->clock=1;
                case 1:
                    if (!twi_pseudo_sda(pseudo,TRUE)) break;
                    twi_pseudo_handler((ADI_PDD_DRIVER *)handle,TWI_EVENT_PSEUDO_STOPPED);
            }
            break;

        case TWI_STATE_PSEUDO_WAIT:
            if (!(twi->master_ctl.reg&0x0001)) twi_pseudo_state((ADI_PDD_DRIVER *)handle,TWI_STATE_PSEUDO_DISABLED);
            else if (!twi->int_stat.reg) twi_pseudo_state((ADI_PDD_DRIVER *)handle,TWI_STATE_PSEUDO_START);
            break;

        case TWI_STATE_PSEUDO_DISABLED:
            pseudo->unstick=0;
            if (twi->master_ctl.reg&0x0001) twi_pseudo_state((ADI_PDD_DRIVER *)handle,TWI_STATE_PSEUDO_START);
            break;
    }

#if defined(ADI_TWI_PSEUDO_ARBITRATION)
    if (pseudo->config.scl_pid&&!twi->control.reg&0x0200)
        flag=twi->master_stat.reg&0x0040?FALSE:TRUE;
    else
#endif
    adi_flag_Sense(pseudo->config.sda,&flag);

    if (flag||pseudo->state==TWI_STATE_PSEUDO_START)
    {
        if (pseudo->busy_count) pseudo->busy_count--;
        else twi->master_stat.reg&=0xFEFF;
    }
    pseudo->run_once=false;
    adi_tmr_GPControl(pseudo->config.timer, ADI_TMR_GP_CMD_CLEAR_INTERRUPT, NULL);
}


/********************************************************************

    Function: twi_pseudo_callback_clock

    Description: Monitors rising and falling edges of the clock pin
        for master arbitration. When pseudo master arbitration is
        enabled this function reads the sda pin on the rising edge of
        the clock, which is then used in the pseudo twi clock callback
        instead of the state of the pin. This function will also
        cause the scl pin to be driver low when a falling clock edge
        is detected. It will also reload the bus busy timer, which is
        decremented in the pseudo twi clock callback function.

********************************************************************/

#if defined(ADI_TWI_PSEUDO_ARBITRATION)
void twi_pseudo_callback_clock(void *handle,u32 event,void *arg)
{
    twi_pseudo_port *pseudo;
    twi_registers *twi;
    u32 f;

    {
        int i=0;
        // ADI_INT_RESULT_NOT_PROCESSED;
        while (handle!=&Device[i++]) if (i>=ADI_PDD_DRIVERS) return;
    }

    pseudo=((ADI_PDD_DRIVER *)handle)->pseudo;
    twi=((ADI_PDD_DRIVER *)handle)->twi;

    // reload busy counter, and set the busy flag
    pseudo->busy_count=pseudo->busy_reload;
    twi->master_stat.reg|=0x0100;

    // if not pseudo mode or master is disabled then dont do anything else
    if (!pseudo||!(twi->master_ctl.reg&0x0001)) return;

    // check the status of the clock
    adi_flag_SetTrigger(pseudo->config.scl,ADI_FLAG_TRIGGER_LEVEL_HIGH);
    adi_flag_Sense(pseudo->config.scl,&f);
    adi_flag_SetTrigger(pseudo->config.scl,ADI_FLAG_TRIGGER_BOTH_EDGES);
    //adi_flag_Clear(pseudo->config.scl);

    // update the clock bit in the master status register
    twi->master_stat.reg=(twi->master_stat.reg|0x0080)^(f?0x0080:0x0000);
    if (f)
    {
        // if clock is high then read the data bit and update the master status register
        adi_flag_Sense(pseudo->config.sda,&f);
        twi->master_stat.reg=(twi->master_stat.reg|0x0040)^(f?0x0040:0x0000);
    } else  twi_pseudo_scl(pseudo,FALSE); // otherwise hold the clock low
}
#endif


/********************************************************************

    Function: twi_pseudo_scl

    Description: sets or clears the pseudo scl pin, if pseudo master
        arbitration is set then the flag callback is disabled and the
        flag mode is changed to read the state of the pin. Once the
        pin is read it is put back into edge detect mode and the
        callback is re-enabled.

        This function return TRUE if the desired pin level is not
        equal to the actual pin level, otherewise it returns FALSE.

********************************************************************/

u32 twi_pseudo_scl(twi_pseudo_port *pseudo,u32 flag)
{
    u32 f,flg=flag=flag?1:0;

    // if SCLOVR is set then set the flag to clear
    if (pseudo->twi.master_ctl.reg&0x8000) flag=0;

#if defined(ADI_TWI_PSEUDO_ARBITRATION)
    if (pseudo->config.scl_pid)
    {
        adi_flag_SuspendCallbacks(pseudo->config.scl,pseudo->config.scl_pid);
        adi_flag_SetTrigger(pseudo->config.scl,ADI_FLAG_TRIGGER_LEVEL_HIGH);
        adi_flag_Sense(pseudo->config.scl,&f);
        f=f?1:0;
        adi_flag_SetTrigger(pseudo->config.scl,ADI_FLAG_TRIGGER_BOTH_EDGES);
        adi_flag_Clear(pseudo->config.scl);
        adi_flag_ResumeCallbacks(pseudo->config.scl,pseudo->config.scl_pid);
    } else
#endif
    {
        adi_flag_Sense(pseudo->config.scl,&f);
        f=f?1:0;
    }

    // if there is a difference then change flag output
    if ((pseudo->flags&0x0100?1:0)^flag)
    {
        pseudo->flags=(pseudo->flags|0x0100)^(flag?0x0000:0x0100);
        adi_flag_SetDirection(pseudo->config.scl,flag?ADI_FLAG_DIRECTION_INPUT:ADI_FLAG_DIRECTION_OUTPUT);
        if (!flag) adi_flag_Clear(pseudo->config.scl);
    }

    // if there was a difference return false, otherwise true
    return !(flg^f);
}


/********************************************************************

    Function: twi_pseudo_sda

    Description: sets or clears the pseudo sda pin.

        This function return TRUE if the desired pin level is not
        equal to the actual pin level, otherewise it returns FALSE.

********************************************************************/

u32 twi_pseudo_sda(twi_pseudo_port *pseudo,u32 flag)
{
    u32 f,flg=flag=flag?1:0;

    // if SDAOVR is set then set the flag to clear
    if (pseudo->twi.master_ctl.reg&0x4000) flag=0;

    // check the SDA flag
    adi_flag_Sense(pseudo->config.sda,&f);
    f=f?1:0;

    // if there is a difference then change flag output
    if ((pseudo->flags&0x0080?1:0)^flag)
    {
        pseudo->flags=(pseudo->flags|0x0080)^(flag?0x0000:0x0080);
        adi_flag_SetDirection(pseudo->config.sda,
            flag?ADI_FLAG_DIRECTION_INPUT:ADI_FLAG_DIRECTION_OUTPUT);
        if (!flag) adi_flag_Clear(pseudo->config.sda);
    }
    // if there was a difference return false, otherwise true
    return !(flg^f);
}


/********************************************************************

    Function: twi_pseudo_tx8

    Description: Puts a byte into the twi pseudo receive register and
        updates the fifo control bits

********************************************************************/

void twi_pseudo_tx8(ADI_PDD_DRIVER *drv,u8 byte)
{
    twi_registers *twi=drv->twi;

    // add a new byte to the transmit fifo
    switch (twi->fifo_stat.reg&0x0003)
    {
        case 0:
            // if transmit fifo is empty then put the new byte into the lo byte of fifo
            twi->xmt_data8.reg=byte;
            twi->fifo_stat.reg|=0x0001;
            break;
        case 1:
            // if already a byte in the fifo then add a new one to to the high byte of fifo
            twi->xmt_data8.reg=(twi->xmt_data8.reg&0x00FF)|((byte<<8)&0xFF00);
            twi->fifo_stat.reg|=0x0003;
            break;
    }

    // transmit the next byte if master is enabled
    if (twi->master_ctl.reg&0x0001) twi_pseudo_state(drv,TWI_STATE_PSEUDO_TRANSMIT);
}


/********************************************************************

    Function: twi_pseudo_rx8

    Description: gets a byte from the twi pseudo receive register and
        updates the fifo control bits

********************************************************************/

u8 twi_pseudo_rx8(ADI_PDD_DRIVER *drv)
{
    twi_registers *twi=drv->twi;
    u8 rv=twi->rcv_data8.reg;

    // clear one fifo status bit and shift the high byte in the receive register to the low
    twi->fifo_stat.reg&=(twi->fifo_stat.reg&0xFFF3)|((twi->fifo_stat.reg>>1)&0x0003);
    twi->rcv_data8.reg>>=8;

    // restart the pseudo TWI and return the content of the low byte of the receive fifo
    if (twi->master_ctl.reg&0x0001) twi_pseudo_state(drv,TWI_STATE_PSEUDO_RECEIVE);
    return rv;
}
#endif

/*****************************************************************************/



/******************************************************************************

TWI master mode functions:

    twi_master_service()            -- interrupt service
    twi_master_processed()
    twi_master_kill()

Note: Some general notes on the operation of the TWI in Master mode:

- Currently, both slave and master operations are not permitted; the
    driver is configured for one or the other. If this is changed, there is some
    chance a slave read transfer might begin after loading a FIFO while starting
    a master transfer. Must either flush FIFO or could disable SEN for a while
    until MEN is set, but the flush seems better.

- One cannot MEN a XMT w/o something in the FIFO.

- For long transfers -- greater than which DCNT can tally -- I had thought
    I might be able to alter DCNT on the fly, but I found this can't be done
    while MEN.

******************************************************************************/


/********************************************************************

    Function: twi_master_service

    Description: this services interrupts generated in Master mode

    Note: For reference, the bits and fields in the registers used by
        master_service()

        master_stat
        -   0x0100  BUSBUSY     often marks TWI's own activities
        -   0x0080  SCLSEN
        -   0x0040  SDASEN
        -   0x0020  BUFWRERR    Rx overflow        W1C
        -   0x0010  BUFRDERR    Tx underflow       W1C
        -   0x0008  DNAK        data not ack'd     W1C
        -   0x0004  ANAK        addr. not ack'd    W1C
        -   0x0002  LOSTARB                        W1C
        -   0x0001  MPROG       master transfer in progress

        master_ctl
        -   0x8000  SCLOVR
        -   0x4000  SDAOVR
        -   0x3FC0  DCNT field
        -   0x0020  RSTART
        -   0x0010  STOP
        -   0x0008  FAST
        -   0x0004  MDIR        set to RCV
        -   0x0002  (none)
        -   0x0001  MEN

        fifo_stat
        -   0x0003  XMTSTAT     one bit for each FIFO position
        -   0x000C  RCVSTAT       ditto

        fifo_ctl
        -   0x0008  RCVINTLEN   1 means int. when full
        -   0x0004  XMTINTLEN   1 means int. when empty
        -   0x0002  RCVFLUSH    1 means flush and keep flushed
        -   0x0001  XMTFLUSH      ditto

        There are 4 interrupt sources, but I found only one need be processed each time.

********************************************************************/

void twi_master_service(ADI_PDD_DRIVER *drv,u16 stat)
{
    // local copies of stat's to reduce peripheral bus access
    u16 master_ctl,master_stat=drv->twi->master_stat.reg;
    ADI_DEV_1D_BUFFER *buffer;

    // MERR - can be set along w/ MCOMP
    if (stat&0x0020)
    {
        // set STOP: work around early silicon RSTART on MERR bug
        // s.a. notes at top of this file
        //if (drv->u.master.rstart) twi->master_ctl.reg|=0x0010;
        if (drv->twi->master_ctl.reg&0x0020) drv->twi->master_ctl.reg|=0x0010;
        if (master_stat&0x0020) twi_master_kill(drv,ADI_TWI_EVENT_BUFWRERR);
        if (master_stat&0x0010) twi_master_kill(drv,ADI_TWI_EVENT_BUFRDERR);
        if (master_stat&0x0008)
        {
            if (drv->type.dnak<drv->config.dnak) drv->type.dnak++;
            if (drv->config.dnak&&drv->type.dnak>=drv->config.dnak)
                twi_master_kill(drv,ADI_TWI_EVENT_DNAK);
        }
        if (master_stat&0x0004)
        {
            if (drv->type.anak<drv->config.anak) drv->type.anak++;
            if (drv->config.anak&&drv->type.anak>=drv->config.anak)
                twi_master_kill(drv,ADI_TWI_EVENT_ANAK);
        }
        if (master_stat&0x0002)
        {
            if (drv->type.lostarb<drv->config.lostarb) drv->type.lostarb++;
            if (drv->config.lostarb&&drv->type.lostarb>=drv->config.lostarb)
                twi_master_kill(drv,ADI_TWI_EVENT_LOSTARB);
        }
        //clear the master_stat register, this is done differently on the pseudo port
#if defined(ADI_TWI_PSEUDO)
        if (drv->pseudo) drv->twi->master_stat.reg&=~master_stat;
        else
#endif
        drv->twi->master_stat.reg=master_stat;
    } else if (stat&0x0010) // MCOMP (branch taken only if no MERR)
    {
        // make sure the last byte is received
        twi_buffer_receive(drv);

        // dispatch the buffer
        twi_master_processed(drv,ADI_DEV_EVENT_BUFFER_PROCESSED);

#if 0	// begin Sanmina-SCI patch to set MDIR control bit during XMTSERV/RCVSERV interrupt, not MCOMP (too late).
	// re: PR-56684 and TAR-45470
	// original ADI code...
	} else if (stat&0x0080) twi_buffer_receive(drv);
    else if (stat&0x0040) twi_buffer_transmit(drv);
#else
	// revised Sanmina-SCI code to set MDIR earlier than on MCOMP interrupt, i.e., during XMTSERV/RCVSERV, as documented in HRM
	} else if (stat&0x00C0) // for either XMTSERV/RCVSERV (branch taken only if no MCOMP and no MERR)
	{
		// get buffer info
		buffer = (drv->type.buffer) ? (&(drv->type.buffer->Buffer)) : (0);

		// set next buffer (if present) direction if repeat start mode is enabled
		if(buffer->pNext && ((u32)(buffer->pNext)->pAdditionalInfo & ADI_TWI_RSTART)) {

			// program direction bit (MDIR) for the next buffer
			drv->twi->master_ctl.reg |=
				(((ADI_DEV_SEQ_1D_BUFFER *)(buffer->pNext))->Direction == ADI_DEV_DIRECTION_INBOUND)
					? 0x0004 : 0x0000;
		}

		// dispatch to data handler depending on interrupt (as before)
		if (stat&0x0080) twi_buffer_receive(drv);
		else if (stat&0x0040) twi_buffer_transmit(drv);
	}
#endif	// Sanmina-SCI code...


    // if stat is 0 or MCOMP or MERR then prepare a new buffer, otherwise we're done
    if (stat&&!(stat&0x0030)) return;

    // setup a new buffer to send/receive
    buffer=drv->type.buffer?&(drv->type.buffer->Buffer):0;
    // if no buffers remain, then exit: adi_pdd_sequential() will re-start the TWI
    if (!buffer) return;

    buffer->ProcessedElementCount=0;
    buffer->ProcessedFlag=(u32)0;

    // grab the address for the transfer from the buffer
    drv->twi->master_addr.reg=(u8)(((u32)buffer->pAdditionalInfo)&0x7F);

    // if not an RSTART, then flush the twi buffers for peace of mind
    if (((u32)buffer->pAdditionalInfo&ADI_TWI_RSTART)!=ADI_TWI_RSTART)
    {
        twi_flush(drv);
    }

    // put the transfer buffer details in more convenient locations
    //twi_buffer_init(drv,(u16)(buffer->Buffer.ElementCount),(u8 *)buffer->Buffer.Data);

    // decide if it's a long or short transfer and mark DCNT appropriately
    master_ctl=(buffer->ElementCount>255?255:buffer->ElementCount)<<6;

    // set direction of transfer
    master_ctl|=drv->type.buffer->Direction==ADI_DEV_DIRECTION_INBOUND?0x0004:0x0000;

    // set repeat start, if there is another buffer and the address is ADI_TWI_RSTART
    // and the current buffer is less than 255 bytes long and SCCB bit is not set
    master_ctl|=(buffer->pNext && ((u32)(buffer->pNext)->pAdditionalInfo & ADI_TWI_RSTART)
        == ADI_TWI_RSTART && buffer->ElementCount<255 && !(drv->twi->control.reg & 0x0200))?0x0020:0x0000;

    // if the buffer is odd the send/receive the first byte, otherwise set the transfer to the
    // user defined fifo rate
    drv->twi->fifo_ctl.reg=buffer->ElementCount&1?0x0000:drv->config.fifo;
    if (drv->type.buffer->Direction==ADI_DEV_DIRECTION_OUTBOUND) twi_buffer_transmit(drv);

    // set the master control register, and enable the transfer
    drv->twi->master_ctl.reg=master_ctl|0x0001;
}


/********************************************************************

    Function: twi_master_processed

    Description: dispatch a processed sequential buffer (Master mode)

    Note: master_processed() is responsible for pulling buffers from the
        queue. It is coded without critical region processing in the expectation
        that higher level interrupts will not need to queue packets. It was expected
        the TWI will be used either by a threads or by the main loop in a single
        thread system. This should cover most cases, and it allows the ISR to
        dispense with critical region calls, improving system performance.

        In the event this restriction needs to be removed, the problem and its
        solution are:

        Problem: The call to submit sequential buffers assumes that if the Master
            queue is empty it must call master_start() to get the driver moving again.
            There is some time between when the queue is marked empty by
            master_processed() and when master_service() gets around to looking at
            whether the queue is empty. A new buffer submitted in this time could cause
            master_start() to be executed twice on the buffer.

        Solution: Have master_processed (and thus master_kill) return if the buffer
            was emptied (while under critical region) and have master_service test this
            to decide whether to go again.

********************************************************************/

void twi_master_processed(ADI_PDD_DRIVER *drv,int event)
{
    ADI_DEV_1D_BUFFER *buffer=&drv->type.buffer->Buffer;

    // pull buffer from the queue -- see warning notice above!
    drv->type.buffer=(ADI_DEV_SEQ_1D_BUFFER *)buffer->pNext;

    buffer->ProcessedFlag=(u32)event; // mark it done

    // tell client if he has requested so
    if (buffer->CallbackParameter) twi_callback(drv,event,buffer->CallbackParameter);

    // reset all the counters
    drv->type.lostarb=drv->type.anak=drv->type.dnak=0;
}


/********************************************************************

    Function: twi_master_kill

    Description: if a sequential buffer causes an error, this funtion
        is called to send it back to the application. Any buffer in the
        queue Rstart'd to it, is pulled as well, signalled with an
        ADI_TWI_EVENT_YANKED.

********************************************************************/

void twi_master_kill(ADI_PDD_DRIVER *drv,int event)
{
    twi_master_processed(drv,event);

    // yank all next in the chain which are RSTART'd to this buffer
    while (drv->type.buffer
        && ((u32)drv->type.buffer->Buffer.pAdditionalInfo&ADI_TWI_RSTART)==ADI_TWI_RSTART)
    {
        twi_master_processed(drv,ADI_TWI_EVENT_YANKED);
    }
}

/*****************************************************************************/



/******************************************************************************

TWI slave mode functions:

    twi_slave_service()             -- interrupt service
    twi_slave_rx_processed()
    twi_slave_rx_need()
    twi_slave_rx()
    twi_slave_tx_processed()
    twi_slave_tx_need()
    twi_slave_tx()

******************************************************************************/



/********************************************************************

    Function: twi_slave_service

    Description: this processes interrupts received in Slave mode
        Documented here are the bit fields in the various registers used in
        Slave mode:

        slave_stat
        -   0x0001  SDIR    1 means slave XMT is requested (interpret at SINIT)
        -   0x0002  GCALL   General Call received

        slave_ctl
        -   0x0010  GEN accept General Call addressing (address 0)
        -   0x0008  NAK send Not Ack instead of Ack with each byte received;
                    can use to kill a transfer
        -   0x0004  STDVAL  should always be 1 since combined Master
                    and Slave access is not done
        -   0x0001  SEN slave enable; can use to kill a transfer in progress

        int_stat
        -   0x0080  RCVSERV
        -   0x0040  XMTSERV

        fifo_ctl
        -   0x0008  RCVINTLEN   1 means int. when full
        -   0x0004  XMTINTLEN       1 means int. when empty
        -   0x0002  RCVFLUSH        1 means flush and keep flushed
        -   0x0001  XMTFLUSH        ditto

        fifo_stat
        -   0x0003  XMTSTAT         one bit for each FIFO position
        -   0x000C  RCVSTAT         ditto

********************************************************************/

void twi_slave_service(ADI_PDD_DRIVER *drv,u16 stat)
{
    twi_registers *twi=drv->twi;
    u16 slave_stat=twi->slave_stat.reg;

    // SCOMP or SERR or SOVF will end a transfer
    if (stat&0x000E)
    {
        u32 event;

        // "A restart or stop condition has occurred during the data receive
        // phase of a transfer." -- this probably implies SCOMP as well
        // this branch was NOT tested: how to force this event?
        if (stat&0x0004) event=ADI_TWI_EVENT_SERR; // SERR

        // "The Slave Transfer Complete (SCOMP) bit was set at the time a
        // subsequent transfer has acknowledged an address phase. The transfer
        // continues; however, it may be difficult to delineate data of one
        // transfer from another."
        // this branch was NOT tested: how to force this event?
        else if (stat&0x0008) event=ADI_TWI_EVENT_SOVF; // SOVF

        // "The transfer is complete and either a stop, or a restart was detected."
        else event=ADI_TWI_EVENT_RCV_PROCESSED; // only SCOMP (0x0002)

        if (drv->type.sdir)
        {
            // Flush the fifo's and dispatch the transmit buffer
            twi_flush(drv);
            twi_slave_tx_processed(drv);
        } else
        {
            // if there is any data in the RCV FIFO, store it
            if (twi->fifo_stat.reg&0x000C) twi_slave_rx(drv);
            twi_slave_rx_processed(drv,event,0); // send buffer back to the client
        }
    }
    if (stat&0x0001)                    // SINIT
    {
        // "The slave has detected an address match and a transfer has been initiated."
        drv->type.gcall=0;
        if (slave_stat&2)               // GCALL
        {
            if (slave_stat&1)           // do not support GCALL XMT
            {
                twi->slave_ctl.reg&=0xFFFE; // so kill transfer by
                twi->slave_ctl.reg|=0x0001; //  toggling SEN
            } else drv->type.gcall=1;       // process as normal receive until dispatch
        }

        drv->type.sdir=slave_stat&1;            // record SDIR

        // note: (1) int_stat was found to have XMTSERV on at SINIT.
        //       (2) XMT FIFO is flushed at SCOMP
        // else if necessary, alert client that he's on
        (drv->type.sdir)?twi_slave_tx_need(drv):twi_slave_rx_need(drv);
    }

    // SINIT was found to occur with either the XMTSERV or RCVSERV that caused it
    if (stat&0x0080) twi_slave_rx(drv);         // RCVSERV
    if (stat&0x0040) twi_slave_tx(drv);         // XMTSERV
}


/********************************************************************

    Function: twi_slave_rx_processed

    Description: makes a callback of a Read buffer, using DCB if so
        requested. The buffer at the head of the queue is pulled from the
        chain.

********************************************************************/

void twi_slave_rx_processed(ADI_PDD_DRIVER *drv,u32 event,u32 more_coming)
{
    ADI_DEV_1D_BUFFER *buffer=drv->type.buffer_rx;

    // queue manipulation: note - this is only called from the ISR so no critical region
    // protection is requested

    drv->type.buffer_rx=buffer->pNext;  // pull this buffer from the list

    buffer->ProcessedFlag=event;
    buffer->pAdditionalInfo=(void *)more_coming;

    // u.slave.gcall is cleared on SINIT
    if (drv->type.gcall&&event==ADI_TWI_EVENT_RCV_PROCESSED) event=ADI_TWI_EVENT_GCALL;

    if (buffer->CallbackParameter) twi_callback(drv,event,buffer->CallbackParameter);
}


/********************************************************************

    Function: twi_slave_rx_need

    Description: both slave_rx() and slave_service() call this to load the next
        receive buffer, or if it is not available, to make a callback telling the
        application one is badly needed

********************************************************************/

void twi_slave_rx_need(ADI_PDD_DRIVER *drv)
{
    // if there is no receive buffer then request one
    if (!drv->type.buffer_rx) twi_callback(drv,ADI_TWI_EVENT_RCV_NEED,0);
    drv->type.pend_rx=drv->type.buffer_rx?0:1;
}


/********************************************************************

    Function: twi_slave_rx()

    Description:
        -   slave_service() calls this to transfer received data to a Read buffer. The
            function will load a new buffer if one is not currently in progress. If the
            last buffer has filled and a new input is received, the old buffer is
            dispatched and a new one is requested, via slave_rx_need(). If a buffer
            fills with this byte, it will not be dispatched until either the next byte
            is received or by an SCOMP.
        -   adi_pdd_read() calls this to pull data when the TWI is stretching SCL.
            This must be done with RCV interrupts off lest new data arrive while Read()
            is doing its work.

********************************************************************/

void twi_slave_rx(ADI_PDD_DRIVER *drv)
{
    if (!drv->type.buffer_rx)
    {
        // if no buffer is being processed
        twi_slave_rx_need(drv);     // may get a read buffer right away
    } else if (drv->type.buffer_rx->ProcessedElementCount==drv->type.buffer_rx->ElementCount)
    {
        // last buffer has filled, so return it to the user noting more is coming
        twi_slave_rx_processed(drv,ADI_TWI_EVENT_RCV_PROCESSED,1);
        twi_slave_rx_need(drv);     // will likely have gotten the buffer right away
    } else drv->type.pend_rx=0;
    twi_buffer_receive(drv);
}


/********************************************************************

    Function: twi_slave_tx_processed()

    Description: makes a callback of a Write buffer, using DCB if so
        requested. The buffer at the head of the queue is pulled from the
        chain.

    Note: a transmit buffer is processed when:
        1) SCOMP signals the transmission is done, or
        2) the XMT FIFO is empty and the current buffer has emptied

********************************************************************/

void twi_slave_tx_processed(ADI_PDD_DRIVER *drv)
{
    ADI_DEV_1D_BUFFER *buffer=drv->type.buffer_tx;

    // queue manipulation: note - this is called either from the ISR or from
    // adi_pdd_Write(), only the latter needing critical region protection

    drv->type.buffer_tx=buffer->pNext;

    // now dispatch the buffer which was pulled from the head of the queue
    buffer->ProcessedFlag=ADI_TWI_EVENT_XMT_PROCESSED;

    if (buffer->CallbackParameter) twi_callback(drv,ADI_TWI_EVENT_XMT_PROCESSED,buffer->CallbackParameter);
}


/********************************************************************

    Function: twi_slave_tx_need

    Description: both slave_tx() and slave_service() call this to load
        the next transmit buffer from the queue, or if it is not available, to
        make a callback telling the appl. one is badly needed:
        the TWI stretches the SCL until data is in the hardware FIFO. If no buffer
        is produced, a marker is set to tell adi_pdd_write() it must restart
        TWI interrupts when the next Write buffer made available.

********************************************************************/

void twi_slave_tx_need(ADI_PDD_DRIVER *drv)
{
    // if there is no buffer then request one
    if (!drv->type.buffer_tx) twi_callback(drv,ADI_TWI_EVENT_XMT_NEED,0);
    drv->type.pend_tx=drv->type.buffer_tx?0:1;
}


/********************************************************************

    Function: twi_slave_tx

    Description: twi_slave_service() calls this to transfer received data to a
        Write buffer. The function will try to load a new buffer if one is not
        currently in progress. If the last buffer has been emptied, a new
        buffer is not requested until the FIFO has emptied as well, this
        eliminates unnecessary _XMT_NEED callbacks when the buffer submitted
        for transmit is exactly the size the Master is trying to pull. If the
        FIFO has room for 2 bytes, a 16 bit transfer is done to miminize
        peripheral bus traffic. adi_pdd_write() calls this if .slave.w_pend has
        been set (by slave_xmt_need()), which re-starts the flow of interrupts.

********************************************************************/

void twi_slave_tx(ADI_PDD_DRIVER *drv)
{
    // if no buffer is being processed
    if (!drv->type.buffer_tx)
    {
        // might get one right away
        twi_slave_tx_need(drv);
    } else if (drv->type.buffer_tx->ProcessedElementCount==drv->type.buffer_tx->ElementCount)
    {
        // wait for fifo to empty
        if (drv->twi->fifo_stat.reg&0x0003) return;

        // and if the FIFO is now empty, then return the transmitted buffer then request a new one
        twi_slave_tx_processed(drv);
        twi_slave_tx_need(drv);
    } else drv->type.pend_tx=0;
    twi_buffer_transmit(drv);
}

/*****************************************************************************/

#if defined(__ADSP_MOAB__)      /* Static functions for Moab class devices  */
/*********************************************************************

    Function:       TwiSetPortControl

    Description:    Configures the general purpose ports for Moab
                    class devices for TWI usage.

*********************************************************************/
static u32 TwiSetPortControl(   /* configures the port control registers    */
    u8  TwiDeviceNumber         /* TWI Device Number */
){

    /* TWI 0 Port Directives */
    ADI_PORTS_DIRECTIVE     Twi0Directives [] = {
        ADI_PORTS_DIRECTIVE_TWI0_SCL,
        ADI_PORTS_DIRECTIVE_TWI0_SDA
    };

    /* TWI 1 Port Directives */
    ADI_PORTS_DIRECTIVE     Twi1Directives [] = {
        ADI_PORTS_DIRECTIVE_TWI1_SCL,
        ADI_PORTS_DIRECTIVE_TWI1_SDA
    };

    ADI_PORTS_DIRECTIVE *pDirectives[] = {&Twi0Directives[0], &Twi1Directives[0]};

    return(adi_ports_Configure(pDirectives[TwiDeviceNumber],
        (sizeof(Twi0Directives)/sizeof(Twi0Directives[0]))));
}

#endif
