/*****************************************************************************

Copyright (c) 2006 Analog Devices, Inc.	All	Rights Reserved. 

This software is proprietary and confidential to Analog	Devices, Inc. 
and	its	licensors.

******************************************************************************

$File: adi_itu656.c $
$Revision: 1.1 $
$Date: 2007/05/22 18:20:07 $

Project:    ITU 656 Video Utilities 
Title:      ITU 656 Video Utilities Source code
Author(s):  bmk
Revised by: bmk

Description:
            Source file for ITU 656 Video Utilities.
            Supported video formats:
            ITU 656 NTSC - Interlaced and Progressive
            ITU 656 PAL - Interlaced and Progressive
            
References:
******************************************************************************

Modification History:
====================
$Log: adi_itu656.c,v $
Revision 1.1  2007/05/22 18:20:07  gstephan
Creating VDSP 5.00 release folder.  Created from the head copy of
VDSP 4.50 release folder.

Revision 1.2  2006/07/06 07:28:23  bmk
Added few more macros

Revision 1.1  2006/06/29 07:55:45  bmk
Initial entry


****************************************************************************** 
*                          INCLUDES                                         *
*****************************************************************************/

#include "adi_itu656.h"      	// ITU 656 Video Utility includes

/***************************************************************************** 
*                          DEFINES                                          *
*****************************************************************************/

// Local Definitions

#define     EAV     1           // Defines End of Active Video
#define     SAV     2           // Defines Start of Active video

/***************************************************************************** 
*                          STATIC FUNCTIONS                                  *
******************************************************************************/

void 	generate_XY ( 
unsigned long 	scanline,       // Current scanline number
FRAME_TYPE 		frametype,      // Video frame type
char 			*preambleXY,    // holds the calculated XY value for EAV / SAV
unsigned long 	videostatus     // indicates XY calculation for EAV or SAV
);
   
void 	calculate_address ( 
char 			*frame_ptr,     // Pointer to the formated video frame in memory
FRAME_TYPE 		frametype,  	// Frame type of the formated memory
char 			**address1,   	// Holds address of Field 1 first active line start address (for interlaced frame type)
                        		// OR First active line start address (for Progressive frame type)
char 			**address2,     // Holds address of Field 2 first active line start address (for interlaced frame type)
unsigned long 	*f1start,       // Holds Field1 active line start value (for interlaced frameformat) 
                        		// OR Active line start value (for progressive frameformat)
unsigned long 	*f1end,         // Holds Field1 active line end value (for interlaced frameformat) 
                        		// OR Active line end value (for progressive frameformat)
unsigned long 	*f2start,       // Holds Field2 active line start value (for interlaced frameformat) 
unsigned long 	*f2end,         // Holds Field2 active line end value (for interlaced frameformat) 
unsigned long 	*SkipBlank      // Number of bytes to skip per line to reach active data 
);

/***************************************************************************** 
*                          FUNCTION DEFINITIONS                             *
******************************************************************************
**
** Function:            adi_video_FrameFormat
**
** Description:         This function formats an area in memory into a video frame
**                      active fields set blank.
**
** Arguments:           
**
**  frame_ptr           Pointer to an area in memory that will be used to build a video frame
**                      
**  frametype           Type of frame to be formatted
**
** Return value:        None
**                      
**
**
*/
			
void adi_video_FrameFormat ( char *frame_ptr,FRAME_TYPE frametype )
{

    unsigned long	i,j,linecount,blankcount,DataPerLine;
    char      		preambleXY;
    
    switch (frametype) {    // Switch to frame type

    // Format frame as NTSC Interlaced or NTSC Progressive
    case (NTSC_IL):
    case (NTSC_PR):
        linecount = NTSC_HEIGHT;
        blankcount = NTSC_BLANKING;
        DataPerLine = NTSC_DATA_PER_LINE;
        break;

    // Format frame as PAL Interlaced or PAL Progressive
    case (PAL_IL):
    case (PAL_PR):
        linecount = PAL_HEIGHT;
        blankcount = PAL_BLANKING;
        DataPerLine = PAL_DATA_PER_LINE;
        break;

    default:    // Default as NTSC frame
        linecount = NTSC_HEIGHT;
        blankcount = NTSC_BLANKING;
        DataPerLine = NTSC_DATA_PER_LINE;
        break;
    }
    
    // Formats frame memory as EAV,Blanking,SAV,Active lines (Blanked)
    for(i = 1; i <= linecount; i++)    // loop to format frame memory
    {

        // Generate BT656 Preamble

        // EAV - FF 00 00 XY
        generate_XY(i,frametype,&preambleXY,EAV);

        *frame_ptr++ = 0xFF;
        *frame_ptr++ = 0x00;
        *frame_ptr++ = 0x00;
        *frame_ptr++ = preambleXY;
        
        // Blanking
        for(j = 0; j < (blankcount / 2); j++)
        {
            *frame_ptr++ = 0x80;
            *frame_ptr++ = 0x10;
        }

        // SAV - FF 00 00 XY
        generate_XY(i,frametype,&preambleXY,SAV);

        *frame_ptr++ = 0xFF;
        *frame_ptr++ = 0x00;
        *frame_ptr++ = 0x00;
        *frame_ptr++ = preambleXY;

        // Output empty horizontal data to blank all lines
        for(j = 0; j < (ITU_ACTIVE_DATA_PER_LINE/2); j++)
        {
            *frame_ptr++ = 0x80;
            *frame_ptr++ = 0x10;
        }
    }
}

/***************************************************************************** 
**
** Function:            adi_video_FrameFill
**
** Description:         This function fills the active video portion(s)
**                      of a formatted frame with a specified color.
**
** Arguments:           
**
**  frame_ptr           Pointer to the formated video frame in memory
**                      
**  frametype           Frame type of the formated memory
**
**  ycbcr_data          32 bit value color value corresponding to a 4 byte array of 8-bit YCbYCr data
**
** Return value:        None
**                      
**
**
*/
void adi_video_FrameFill ( char *frame_ptr,FRAME_TYPE frametype,char *ycbcr_data )
{

    unsigned long   i,j,f1start,f1end,f2start,f2end,widthcount,SkipBlank;
    char      		*address1,*address2;
	
    // initialise the pointers
    address1 = frame_ptr;
    address2 = frame_ptr;
	
    // Calculate the active line address & update widthcount, frame field start and end values 
    calculate_address (frame_ptr,frametype,&address1,&address2,&f1start,&f1end,&f2start,&f2end,&SkipBlank);
	
    // Paints active lines with provided YCbCr color value
    // Paints Field1 if frameformat is interlaced OR whole frame is frameformat is Progressive
    for(i = f1start; i <= f1end; i++)    
    {
        address1 += SkipBlank;
        // Output YCbCr data (4:2:2 format)
        for(j = 0; j < (ITU_ACTIVE_DATA_PER_LINE / 4); j++)
        {
            *address1++ = *ycbcr_data;
            *address1++ = *(ycbcr_data+1);
            *address1++ = *(ycbcr_data+2);
            *address1++ = *(ycbcr_data+3);
        }
    }
	
    // Paints Field2 only when frametype is Interlaced
    if ((frametype == NTSC_IL) || (frametype == PAL_IL))    
    {    	
        for(i = f2start; i <= f2end; i++)
        {
            address2 += SkipBlank;            
            // Output YCbCr data (4:2:2 format)
            for(j = 0; j < (ITU_ACTIVE_DATA_PER_LINE / 4); j++)
            {
                *address2++ = *ycbcr_data;
                *address2++ = *(ycbcr_data+1);
                *address2++ = *(ycbcr_data+2);
                *address2++ = *(ycbcr_data+3);
            }
        }
    }
}

/***************************************************************************** 
**
** Function:            adi_video_RowFill
**
** Description:         This function fills a row of pixels in active video 
**                      portion of formated frame with specified color
**
** Arguments:           
**
**  frame_ptr           Pointer to the formated video frame in memory
**                      
**  frametype           Frame type of the formated memory
**
**  row_value           32 bit value corresponding to row number of active field
**
**  ycbcr_data          32 bit value color value corresponding to a 4 byte array of 8-bit YCbYCr data
**
** Return value:        None
**                      
**
**
*/
void adi_video_RowFill ( char *frame_ptr,FRAME_TYPE frametype,unsigned long row_value,char *ycbcr_data )
{

    unsigned long   i,j,f1start,f1end,f2start,f2end,SkipBlank;
    char      		*address1,*address2;
          
    // initialise the pointers
    address1 = frame_ptr;
    address2 = frame_ptr;
	
    // Calculate the active line address & update widthcount, frame field start and end values 
    calculate_address (frame_ptr,frametype,&address1,&address2,&f1start,&f1end,&f2start,&f2end,&SkipBlank);

    // Paints active lines with provided YCbCr color value
    // Paints Field1 if frameformat is interlaced OR whole frame is frameformat is Progressive
    for(i = f1start; i <= f1end; i++)    
    {
		address1 += SkipBlank;        
        if (i == row_value)    // is this the row to be painted with YCbCr data?
        {
            // Output YCbCr data (4:2:2 format)
            for(j = 0; j < (ITU_ACTIVE_DATA_PER_LINE / 4); j++)
            {
                *address1++ = *ycbcr_data;
                *address1++ = *(ycbcr_data+1);
                *address1++ = *(ycbcr_data+2);
                *address1++ = *(ycbcr_data+3);
            }   
        }
        else    // Paint all other rows as blank
        {
            for(j = 0; j < (ITU_ACTIVE_DATA_PER_LINE / 2); j++)
            {
                *address1++ = 0x80;
                *address1++ = 0x10;
            }   
        }
    }
    
    // Paints Field2 only when frametype is Interlaced
    if ((frametype == NTSC_IL) || (frametype == PAL_IL))    
    {
        for(i = f2start; i <= f2end; i++)
        {
			address2 += SkipBlank;            
            if (i == row_value)     // is this the row to be painted with YCbCr data?
            {
                // Output YCbCr data (4:2:2 format)
                for(j = 0; j < (ITU_ACTIVE_DATA_PER_LINE / 4); j++)
                {
                    *address2++ = *ycbcr_data;
                    *address2++ = *(ycbcr_data+1);
                    *address2++ = *(ycbcr_data+2);
                    *address2++ = *(ycbcr_data+3);
                }   
            }
            else    // Paint all other rows as blank
            {
                for(j = 0; j < (ITU_ACTIVE_DATA_PER_LINE / 2); j++)
                {
                    *address2++ = 0x80;
                    *address2++ = 0x10;
                }   
            }
        }
    }
}

/***************************************************************************** 
**
** Function:            adi_video_ColumnFill
**
** Description:         This function fills a column of pixels in active video 
**                      portion of formated frame with specified color
**
** Arguments:           
**
**  frame_ptr           Pointer to the formated video frame in memory
**                      
**  frametype           Frame type of the formated memory
**
**  column_value        32 bit value corresponding to column number of active field
**
**  ycbcr_data          32 bit value color value corresponding to a 4 byte array of 8-bit YCbYCr data
**
** Return value:        None
**                      
**
**
*/
void adi_video_ColumnFill ( char *frame_ptr,FRAME_TYPE frametype,unsigned long column_value,char *ycbcr_data )
{

    unsigned long	i,j,f1start,f1end,f2start,f2end,SkipBlank;
    char      		*address1,*address2;
    
	// initialise the pointers
    address1 = frame_ptr;
    address2 = frame_ptr;
	
    // Calculate the active line address & update widthcount, frame field start and end values 
    calculate_address (frame_ptr,frametype,&address1,&address2,&f1start,&f1end,&f2start,&f2end,&SkipBlank);

    // Paints active lines with provided YCbCr color value
    // Paints Field1 if frameformat is interlaced OR whole frame is frameformat is Progressive
    for(i = f1start; i <= f1end; i++)    
    {
		address1 += SkipBlank;        
        for(j = 0; j < (ITU_ACTIVE_DATA_PER_LINE / 4); j++)
        {            
            if (j == column_value)      // is this the column to be painted with YCbCr data?
            {                           // Yes, Output YCbCr data (4:2:2 format)
                *address1++ = *ycbcr_data;
                *address1++ = *(ycbcr_data+1);
                *address1++ = *(ycbcr_data+2);
                *address1++ = *(ycbcr_data+3);
            }
            else
            {                           // No - Paint the column as blank
                *address1++ = 0x80;
                *address1++ = 0x10;
                *address1++ = 0x80;
                *address1++ = 0x10;
            }
        }
    }
    
    // Paints Field2 only when frametype is Interlaced
    if ((frametype == NTSC_IL) || (frametype == PAL_IL))    
    {
        for(i = f2start; i <= f2end; i++)
        {
			address2 += SkipBlank;            
            // Output YCbCr data (4:2:2 format)
            for(j = 0; j < (ITU_ACTIVE_DATA_PER_LINE / 4); j++)
            {
                if (j == column_value)      // is this the column to be painted with YCbCr data?
                {                           // Yes, Output YCbCr data (4:2:2 format)
                    *address2++ = *ycbcr_data;
                    *address2++ = *(ycbcr_data+1);
                    *address2++ = *(ycbcr_data+2);
                    *address2++ = *(ycbcr_data+3);
                }
                else
                {                           // No - Paint the column as blank
                    *address2++ = 0x80;
                    *address2++ = 0x10;
                    *address2++ = 0x80;
                    *address2++ = 0x10;
                }
            }           
        }
    }
}




    /*case (PAL_IL):

        *f1start = PAL_ILAF1_START;   // active line start - Field1
        *f1end = PAL_ILAF1_END;       // active line end - Field1
        // Calculate Field 1 first active line's active data start address
        *address1 = frame_ptr + ((PAL_ILAF1_START-1) * PAL_DATA_PER_LINE);

        *f2start = PAL_ILAF2_START;   // active line start - Field2
        *f2end = PAL_ILAF2_END;       // active line end - Field2
        // Calculate Field 2 first active line start address
        *address2 = frame_ptr + ((PAL_ILAF2_START-1) * PAL_DATA_PER_LINE);
		// Number of bytes to skip per line to reach active data
		*SkipBlank	= PAL_BLANKING + EAV_SIZE + SAV_SIZE;
		
        break;*/
        

void adi_video_PixelSet(char* frame_ptr, unsigned long x, unsigned long y, char *ycbcr_data)
{
	unsigned long skipBlank = PAL_BLANKING + EAV_SIZE + SAV_SIZE;
	unsigned long quo = y >> 1;
	unsigned long rem = y & 1;
	
	if(rem == 0)	//field1
	{
		frame_ptr += ((PAL_ILAF1_START - 1 + quo) * PAL_DATA_PER_LINE) + skipBlank + (x << 2);
		
		*frame_ptr++  = *ycbcr_data++;
		*frame_ptr++ = *ycbcr_data++;
		*frame_ptr++ = *ycbcr_data++;
		*frame_ptr = *ycbcr_data;
	}
	else	//field2
	{
		frame_ptr += ((PAL_ILAF2_START - 1 + quo) * PAL_DATA_PER_LINE) + skipBlank + (x << 2);
		*frame_ptr++ = *ycbcr_data++;
		*frame_ptr++ = *ycbcr_data++;
		*frame_ptr++ = *ycbcr_data++;
		*frame_ptr = *ycbcr_data;
	}
}

void adi_video_RegionSet(char* frame_ptr, unsigned long x, unsigned long y, unsigned long width, unsigned long height, char *ycbcr_data)
{
	unsigned long xBeg = x, xEnd = x + width, yBeg = y, yEnd = y + height;
	
	for(x = xBeg; x < xEnd; ++x)
		for(y = yBeg; y < yEnd; ++y)
			adi_video_PixelSet(frame_ptr, x, y, ycbcr_data);
}

/***************************************************************************** 
**
** Function:            adi_video_CopyField
**
** Description:         This function copies Field 1 or Field 2 data to 
						Field 2 or Field 1 location in a video frame 
**
** Arguments:           
**
**  frame_ptr           Pointer to the video frame to be copied
**                      
**  frametype           Type of video frame that needs field copy
**
**  fieldsrc			Source Field to be copied 
						(value must be either 1 or 2, corresponds to Field 1 or Field 2)
**	ActiveDataOnly		TRUE - copies only the active data from/to the field
						FALSE- copies whole data including blanking information
**
** Return value:        0 in case of success, 1 in case of failure
**                      
**
**
*/

bool adi_video_CopyField ( char *frame_ptr, FRAME_TYPE frametype, char fieldsrc, bool ActiveDataOnly)
{
	char      		*SrcFieldAddress,*DestFieldAddress;
	unsigned long 	SkipBlank,RowCount,DataPerLine,Field1Start,Field2Start;
	
	unsigned long	i,j;
	
	// check frame type
	if (frametype == NTSC_IL)
	{
	    // NTSC Interlaced video frame
	    // # of data (in bytes) to be skipped per line to reach active data
	    SkipBlank	= NTSC_BLANKING + EAV_SIZE + SAV_SIZE;
	    // # of rows per field
	    RowCount	= NTSC_ACTIVE_FLINES;
	    // Data per line
	    DataPerLine	= NTSC_DATA_PER_LINE;
	    // Field 1 Start
	    Field1Start	= NTSC_ILAF1_START;
	    // Field 2 Start
	    Field2Start	= NTSC_ILAF2_START;
	}
	else if (frametype == PAL_IL)
	{
		// PAL Interlaced video frame
	    // # of data (in bytes) to be skipped per line to reach active data
	    SkipBlank	= PAL_BLANKING + EAV_SIZE + SAV_SIZE;
	    // # of rows per field
	    RowCount	= PAL_ACTIVE_FLINES;
	    // Data per line
	    DataPerLine	= PAL_DATA_PER_LINE;
	    // Field 1 Start
	    Field1Start	= PAL_ILAF1_START;
	    // Field 2 Start
	    Field2Start	= PAL_ILAF2_START;
	}
	else	// video mode is not supported by this function
		return 1;
	
	// check field source
    if (fieldsrc == 1)
    {
        // Field 1 is the source
        // Calculate Field 1 first active line start address
        SrcFieldAddress = frame_ptr + ((Field1Start-1) * DataPerLine);
        // Field 2 is the destination
        // Calculate Field 2 first active line start address
        DestFieldAddress = frame_ptr + ((Field2Start-1) * DataPerLine);    	    
    }
    else if (fieldsrc == 2)
    {
        // Field 2 is the source
        // Calculate Field 2 first active line start address
        SrcFieldAddress = frame_ptr + ((Field2Start-1) * DataPerLine);
        // Field 2 is the destination
        // Calculate Field 1 first active line start address
        DestFieldAddress = frame_ptr + ((Field1Start-1) * DataPerLine);    	    
    }
    else	// field source value is not valid
    	return 1;
	
	// check Active Data Only flag
	if (ActiveDataOnly)
	    // copy active data only
	    DataPerLine	= ITU_ACTIVE_DATA_PER_LINE;
	else
		// copy entire field
		SkipBlank	= 0;
	    
	for (i=0;i<RowCount;i++)
	{
	    DestFieldAddress += SkipBlank;
	    SrcFieldAddress  += SkipBlank;
	    for (j=0;j<DataPerLine;j++)
	    {
	        *DestFieldAddress = *SrcFieldAddress;
	        ++DestFieldAddress;
	        ++SrcFieldAddress;
	    }
	}
	
	return 0;	
}

/***************************************************************************** 
**
** Function:            generate_XY
**
** Description:         This function generates the XY preamble value for EAV & SAV 
**
** Arguments:           
**
**  scanline            Current scanline number
**                      
**  frametype           Video frame type
**
**  preambleXY          holds the calculated XY value for EAV / SAV
**
**  videostatus         indicates XY calculation for EAV or SAV
**
** Return value:        None
**                      
**
**
*/

void generate_XY ( unsigned long scanline,FRAME_TYPE frametype,char *preambleXY,unsigned long videostatus )
{

    if(frametype == NTSC_IL)        // Frame type is NTSC interlaced 
    {
        if((scanline >= NTSC_ILBF2_START1) && (scanline <= NTSC_ILBF2_END1))  // Blank 1 Field 2
        {
            if(videostatus == EAV)  
            {
                *preambleXY = 0xF1;
            }
            else if(videostatus == SAV)
            {
                *preambleXY = 0xEC;
            }
        }
        else if((scanline >= NTSC_ILBF1_START1) && (scanline <= NTSC_ILBF1_END1))    // Blank 1 Field 1
        {
            if(videostatus == EAV)
            {
                *preambleXY = 0xB6;
            }
            else if(videostatus == SAV)
            {
                *preambleXY = 0xAB;
            }
        }
        else if((scanline >= NTSC_ILAF1_START) && (scanline <= NTSC_ILAF1_END))  // Active Video Field 1
        {
            if(videostatus == EAV)
            {
                *preambleXY = 0x9D;
            }
            else if(videostatus == SAV)
            {
                *preambleXY = 0x80;
            }
        }
        else if((scanline >= NTSC_ILBF1_START2) && (scanline <= NTSC_ILBF1_END2))  // Blank 2 Field 1
        {
            if(videostatus == EAV)
            {
                *preambleXY = 0xB6;
            }
            else if(videostatus == SAV)
            {
                *preambleXY = 0xAB;
            }
        }
        else if((scanline >= NTSC_ILBF2_START2) && (scanline <= NTSC_ILBF2_END2)) // Blank 2 Field 2
        {
            if(videostatus == EAV)
            {
                *preambleXY = 0xF1;
            }
            else if(videostatus == SAV)
            {
                *preambleXY = 0xEC;
            }
        }
        else if((scanline >= NTSC_ILAF2_START) && (scanline <= NTSC_ILAF2_END)) // Active Video Field 2
        {
            if(videostatus == EAV)
            {
                *preambleXY = 0xDA;
            }
            else if(videostatus == SAV)
            {
                *preambleXY = 0xC7;
            }
        }
    }
    else if(frametype == PAL_IL)        // Frame type is PAL interlaced
    {
        if((scanline >= PAL_ILBF1_START1) && (scanline <= PAL_ILBF1_END1)) // Blank 1 Field 1
        {
            if(videostatus == EAV)
            {
                *preambleXY = 0xB6;
            }
            else if(videostatus == SAV)
            {
                *preambleXY = 0xAB;
            }
        }
        else if((scanline >= PAL_ILAF1_START) && (scanline <= PAL_ILAF1_END))  // Active Video Field 1
        {
            if(videostatus == EAV)
            {
                *preambleXY = 0x9D;
            }
            else if(videostatus == SAV)
            {
                *preambleXY = 0x80;
            }
        }
        else if((scanline >= PAL_ILBF1_START2) && (scanline <= PAL_ILBF1_END2)) // Blank 2 Field 1
        {
            if(videostatus == EAV)
            {
                *preambleXY = 0xB6;
            }
            else if(videostatus == SAV)
            {
                *preambleXY = 0xAB;
            }
        }
        else if((scanline >= PAL_ILBF2_START1) && (scanline <= PAL_ILBF2_END1)) // Blank 1 Field 2
        {
            if(videostatus == EAV)
            {
                *preambleXY = 0xF1;
            }
            else if(videostatus == SAV)
            {
                *preambleXY = 0xEC;
            }
        }
        else if((scanline >= PAL_ILAF2_START) && (scanline <= PAL_ILAF2_END)) // Active Video Field 2
        {
            if(videostatus == EAV)
            {
                *preambleXY = 0xDA;
            }
            else if(videostatus == SAV)
            {
                *preambleXY = 0xC7;
            }
        }
        else if((scanline >= 624) && (scanline <= 625)) // Blank 2 Field 2
        {
            if(videostatus == EAV)
            {
                *preambleXY = 0xF1;
            }
            else if(videostatus == SAV)
            {
                *preambleXY = 0xEC;
            }
        }
    }
    else if(frametype == NTSC_PR)       // Frame type is NTSC Progressive   
    {
        if((scanline >= 1) && (scanline < NTSC_PRF_START)) // Blanking
        {
            if(videostatus == EAV)
            {
                *preambleXY = 0xB6;
            }
            else if(videostatus == SAV)
            {
                *preambleXY = 0xAB;
            }
        }
        else if((scanline >= NTSC_PRF_START) && (scanline <= NTSC_PRF_END))  // Active Video
        {
            if(videostatus == EAV)
            {
                *preambleXY = 0x9D;
            }
            else if(videostatus == SAV)
            {
                *preambleXY = 0x80;
            }
        }
    }
    else if(frametype == PAL_PR)       // Frame type is PAL Progressive   
    {
        if((scanline >= 1) && (scanline < PAL_PRF_START)) // Blanking
        {
            if(videostatus == EAV)
            {
                *preambleXY = 0xB6;
            }
            else if(videostatus == SAV)
            {
                *preambleXY = 0xAB;
            }
        }
        else if((scanline >= PAL_PRF_START) && (scanline <= PAL_PRF_END))  // Active Video
        {
            if(videostatus == EAV)
            {
                *preambleXY = 0x9D;
            }
            else if(videostatus == SAV)
            {
                *preambleXY = 0x80;
            }
        }
        else if((scanline >PAL_PRF_END) && (scanline <= PAL_HEIGHT))  // Blanking
        {
            if(videostatus == EAV)
            {
                *preambleXY = 0xB6;
            }
            else if(videostatus == SAV)
            {
                *preambleXY = 0xAB;
            }
        }
    }
}

/***************************************************************************** 
**
** Function:            calculate_address
**
** Description:         This function calculates active line address & 
**                      updates widthcount, frame field start and end values
**
** Arguments:           
**
**  frame_ptr           Pointer to the formated video frame in memory
**                      
**  frametype           Frame type of the formated memory
**
**  address1            Holds address of Field 1 first active line start address (for interlaced frame type)
**                      OR First active line start address (for Progressive frame type)
**
**  address2            Holds address of Field 2 first active line start address (for interlaced frame type)
**
**  f1start             Holds Field1 active line start value (for interlaced frameformat) 
**                      OR Active line start value (for progressive frameformat)
**
**  f1end               Holds Field1 active line end value (for interlaced frameformat) 
**                      OR Active line end value (for progressive frameformat)
**
**  f2start             Holds Field2 active line start value (for interlaced frameformat) 
**
**  f2end               Holds Field2 active line end value (for interlaced frameformat) 
**
**	SkipBlank			Number of bytes to skip per line to reach active data
**
**
** Return value:        None
**                      
**
**/

void calculate_address ( char *frame_ptr,FRAME_TYPE frametype,char **address1,char **address2,unsigned long *f1start,
unsigned long *f1end,unsigned long *f2start,unsigned long *f2end, unsigned long *SkipBlank)
{

    switch (frametype) {    // Switch to frame type

    // Frame format is NTSC Interlaced
    case (NTSC_IL):

        *f1start = NTSC_ILAF1_START;  // active line start - Field1
        *f1end = NTSC_ILAF1_END;      // active line end - Field1
        // Calculate Field 1 first active line's active data start address
        *address1 = frame_ptr + ((NTSC_ILAF1_START-1) * NTSC_DATA_PER_LINE);

        *f2start = NTSC_ILAF2_START;  // active line start - Field2
        *f2end = NTSC_ILAF2_END;      // active line end - Field2 
        // Calculate Field 2 first active line start address
        *address2 = frame_ptr + ((NTSC_ILAF2_START-1) * NTSC_DATA_PER_LINE);
		// Number of bytes to skip per line to reach active data
       	*SkipBlank	= NTSC_BLANKING + EAV_SIZE + SAV_SIZE;

        break;

    // Frame format is PAL Interlaced
    case (PAL_IL):

        *f1start = PAL_ILAF1_START;   // active line start - Field1
        *f1end = PAL_ILAF1_END;       // active line end - Field1
        // Calculate Field 1 first active line's active data start address
        *address1 = frame_ptr + ((PAL_ILAF1_START-1) * PAL_DATA_PER_LINE);

        *f2start = PAL_ILAF2_START;   // active line start - Field2
        *f2end = PAL_ILAF2_END;       // active line end - Field2
        // Calculate Field 2 first active line start address
        *address2 = frame_ptr + ((PAL_ILAF2_START-1) * PAL_DATA_PER_LINE);
		// Number of bytes to skip per line to reach active data
		*SkipBlank	= PAL_BLANKING + EAV_SIZE + SAV_SIZE;
		
        break;

    // Frame format is NTSC Progressive
    case (NTSC_PR):

        *f1start = NTSC_PRF_START;   // active line start
        *f1end = NTSC_PRF_END;       // active line end
        // Calculate First active line start address
        *address1 = frame_ptr + ((NTSC_PRF_START-1)* NTSC_DATA_PER_LINE);
		// Number of bytes to skip per line to reach active data
        *SkipBlank	= NTSC_BLANKING + EAV_SIZE + SAV_SIZE;
        
        break;

    // Frame format is PAL Progressive
    case (PAL_PR):

        *f1start = PAL_PRF_START;   // active line start
        *f1end = PAL_PRF_END;       // active line end
        // Calculate First active line start address
        *address1 = frame_ptr + ((PAL_PRF_START-1)* PAL_DATA_PER_LINE);        
        // Number of bytes to skip per line to reach active data
		*SkipBlank	= PAL_BLANKING + EAV_SIZE + SAV_SIZE;
		
        break;

    default:    // Default as NTSC Progressive

        *f1start = NTSC_PRF_START;   // active line start
        *f1end = NTSC_PRF_END;       // active line end
        // Calculate First active line start address
        *address1 = frame_ptr + ((NTSC_PRF_START-1)* NTSC_DATA_PER_LINE);
		// Number of bytes to skip per line to reach active data
        *SkipBlank	= NTSC_BLANKING + EAV_SIZE + SAV_SIZE;
                
        break;
    }
}

/*****/

