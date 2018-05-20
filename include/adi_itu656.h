/*****************************************************************************

Copyright (c) 2006 Analog Devices, Inc.	All	Rights Reserved. 

This software is proprietary and confidential to Analog	Devices, Inc. 
and	its	licensors.

******************************************************************************

$File: adi_itu656.h $
$Revision: 1.1 $
$Date: 2007/05/22 18:24:36 $

Project:    ITU 656 Video Utilities
Title:      ITU 656 Video definitions
Author(s):  bmk
Revised by: bmk

Description:
            Header file for ITU 656 Video Utilities.
            Suppored video formats:
            ITU 656 NTSC - Interlaced and Progressive
            ITU 656 PAL - Interlaced and Progressive

Video Utility Functions:

			1. adi_video_FrameFormat() - Formats a given memory area to a specified video frame
			
			Prototype:
			void adi_video_FrameFormat ( char *frame_ptr, FRAME_TYPE frametype)
			
			frame_ptr - Pointer to memory area to be formated (start of the video frame)
			frametype - required video Frame type (NTSC/PAL)

			2. adi_video_FrameFill() - Fills active video portions of a formated frame to specified color
			
			Prototype:
			void adi_video_FrameFormat ( char *frame_ptr, FRAME_TYPE frametype, char *ycbcr_data)
			
			frame_ptr - Pointer to the formated video frame (start of the video frame)
			frametype - Formated video Frame type (NTSC/PAL)
			ycbcr_data- 4 byte array of 32 bit color value of YCbCr data to be filled

			3. adi_video_RowFill() - Fills a row of active video portion of formated frame with specified color
			
			Prototype:
			void adi_video_RowFill ( char *frame_ptr, FRAME_TYPE frametype, unsigned long row_value, char *ycbcr_data)
			
			frame_ptr - Pointer to the formated video frame (start of the video frame)
			frametype - Formated video Frame type (NTSC/PAL)
			row_value - 32 bit value corresponding to row number to be filled
			ycbcr_data- 4 byte array of 32 bit color value of YCbCr data to be filled
			
			4. adi_video_ColumnFill() - Fills a Column of active video portion of formated frame with specified color
			
			Prototype:
			void adi_video_ColumnFill ( char *frame_ptr, FRAME_TYPE frametype, unsigned long column_value, char *ycbcr_data)
			
			frame_ptr - Pointer to the formated video frame (start of the video frame)
			frametype - Formated video Frame type (NTSC/PAL)
			column_value - 32 bit value corresponding to column number to be filled
			ycbcr_data- 4 byte array of 32 bit color value of YCbCr data to be filled

			5. adi_video_CopyField() - Copies Field 1 or Field 2 data to Field 2 or Field 1 location in 
									   a selected video frame. Function can be used while pausing video
			
			Prototype:
			bool adi_video_CopyField(char *frame_ptr, FRAME_TYPE frametype, char fieldsrc, bool ActiveDataOnly)
			
			frame_ptr - Pointer to the formated video frame (start of the video frame)
			frametype - Formated video Frame type (NTSC/PAL)
			fieldsrc  - Source Field to be copied (value must be either 1 or 2, corresponds to Field 1 or Field 2)			
			ActiveDataOnly - when TRUE copies only the active data from/to the field
							 when FALSE copies whole field including blanking information
			Return value: 0 in case of success, 1 in case of failure
			
References:
******************************************************************************

Modification History:
====================
$Log: adi_itu656.h,v $
Revision 1.1  2007/05/22 18:24:36  gstephan
Creating VDSP 5.00 release folder.  Created from the head copy of
VDSP 4.50 release folder.

Revision 1.4  2006/07/07 00:37:58  bmk
Removed unwanted typedefs

Revision 1.3  2006/07/06 07:28:10  bmk
Added few more macros

Revision 1.2  2006/07/03 00:20:28  bmk
Added few more macros

Revision 1.1  2006/06/29 07:55:22  bmk
Initial entry


*****************************************************************************/

#ifndef ADI_ITU656_H     // define adi_itu656.h
#define ADI_ITU656_H

/***************************************************************************** 

Common ITU656 definitions

*****************************************************************************/

#define EAV_SIZE            		4               	// EAV size (bytes)
#define SAV_SIZE            		4            		// SAV size (bytes)
#define	ITU_ACTIVE_DATA_PER_LINE	1440				// Active Data per line	for	ITU 656 video modes (bytes)
#define	ITU_PIXEL_PER_LINE			720					// Number of pixels per line for ITU video
#define	ITU_U_OFFSET				0					// Offset to reach blue chroma (U) value
#define	ITU_Y_OFFSET				1					// Offset to reach luma (Y) value
#define	ITU_V_OFFSET				2					// Offset to reach red chroma (V) value

/***************************************************************************** 

NTSC definitions

Resoultion - 720x486, 525/60 video system

*****************************************************************************/

#define NTSC_DATA_PER_LINE		1716					// Data per line for NTSC
#define NTSC_HEIGHT         	525                     // Including active & blank lines
#define	NTSC_FRAME_LINES		525						// Number of lines per frame (bytes)
#define NTSC_ACTIVE_FLINES  	243                     // Active Field lines
#define NTSC_ACTIVE_LINES   	486						// Active lines in a frame
#define NTSC_BLANKING       	268                     // Size blanking data (bytes) per line
// Number of bytes to skip to reach active video data in a single line
#define NTSC_ACTIVE_VIDEO_SKIP	(NTSC_BLANKING + EAV_SIZE + SAV_SIZE)
// Size	of an entire NTSC video frame (bytes)
#define	NTSC_VIDEO_FRAME_SIZE	(NTSC_DATA_PER_LINE * NTSC_HEIGHT)

// Interlaced NTSC definitions
#define NTSC_ILBF1_START1  		4                      	// Start line number for Field 1 - Blank 1
#define NTSC_ILBF1_END1  		19                      // End Line number for Field 1 - Blank 1
#define NTSC_ILBF1_START2  		264                   	// Start line number for Field 1 - Blank 2
#define NTSC_ILBF1_END2  		265                     // End Line number for Field 1 - Blank 2
#define NTSC_ILBF2_START1  		1                      	// Start line number for Field 2 - Blank 1
#define NTSC_ILBF2_END1  		3                      	// End Line number for Field 2 - Blank 1
#define NTSC_ILBF2_START2  		266                   	// Start line number for Field 2 - Blank 2
#define NTSC_ILBF2_END2  		282                     // End Line number for Field 2 - Blank 2
#define NTSC_ILAF1_START    	20                     	// Active video field1 start line
#define NTSC_ILAF1_END      	263                    	// Active video field1 end line
#define NTSC_ILAF2_START    	283                     // Active video field2 start line
#define NTSC_ILAF2_END      	525                     // Active video field2 end line

// Number of bytes to skip to reach NTSC active frame field1
#define	NTSC_F1_SKIP			((NTSC_ILAF1_START-1) * NTSC_DATA_PER_LINE)
// Number of bytes to skip to reach NTSC active frame field2
#define	NTSC_F2_SKIP			((NTSC_ILAF2_START-1) * NTSC_DATA_PER_LINE)
// NTSC Interlaced active field size
#define	NTSC_ILA_FIELD_SIZE		(NTSC_ACTIVE_FLINES * NTSC_DATA_PER_LINE)
// NTSC Interlaced active video size
#define	NTSC_ILA_VIDEO_SIZE		(NTSC_ACTIVE_FLINES * ITU_ACTIVE_DATA_PER_LINE)

// Progressive NTSC definitions
#define NTSC_PRF_START     		46                      // NTSC Progressive active frame start line
#define NTSC_PRF_END        	525                     // NTSC Progressive active frame finish line
// NTSC Progressive active field size
#define	NTSC_PRA_FIELD_SIZE		((NTSC_PRF_END-NTSC_PRF_START) * NTSC_DATA_PER_LINE)
// NTSC Progressive active video size
#define	NTSC_PRA_VIDEO_SIZE		((NTSC_PRF_END-NTSC_PRF_START) * ITU_ACTIVE_DATA_PER_LINE)

/***************************************************************************** 

PAL definitions

Resoultion - 720x576, 625/50 video system

*****************************************************************************/

#define PAL_DATA_PER_LINE		1728					// Data per line for PAL
#define PAL_HEIGHT          	625                     // Including active & blank lines
#define	PAL_FRAME_LINES			625						// Number of lines per frame (bytes)
#define PAL_ACTIVE_FLINES   	288                     // Active Field lines
#define PAL_ACTIVE_LINES    	576 					// Active lines in a frame
#define PAL_BLANKING        	280                     // Size blanking data (bytes) per line
// Number of bytes to skip to reach active video data in a single line
#define PAL_ACTIVE_VIDEO_SKIP	(PAL_BLANKING + EAV_SIZE + SAV_SIZE)
// Size	of an entire PAL video frame (bytes)
#define	PAL_VIDEO_FRAME_SIZE	(PAL_DATA_PER_LINE * PAL_HEIGHT)

// Interlaced PAL definitions
#define PAL_ILBF1_START1  		1                      	// Start line number for Field 1 - Blank 1
#define PAL_ILBF1_END1  		22                      // End Line number for Field 1 - Blank 1
#define PAL_ILBF1_START2  		311                   	// Start line number for Field 1 - Blank 2
#define PAL_ILBF1_END2  		312                     // End Line number for Field 1 - Blank 2
#define PAL_ILBF2_START1  		313                     // Start line number for Field 2 - Blank 1
#define PAL_ILBF2_END1  		335                     // End Line number for Field 2 - Blank 1
#define PAL_ILBF2_START2  		624                   	// Start line number for Field 2 - Blank 2
#define PAL_ILBF2_END2  		625                     // End Line number for Field 2 - Blank 2
#define PAL_ILAF1_START    		23                     	// Active video field1 start line
#define PAL_ILAF1_END      		310                    	// Active video field1 end line
#define PAL_ILAF2_START    		336                     // Active video field2 start line
#define PAL_ILAF2_END      		623                     // Active video field2 end line

// Number of bytes to skip to reach PAL active frame field1
#define	PAL_F1_SKIP				((PAL_ILAF1_START-1) * PAL_DATA_PER_LINE)
// Number of bytes to skip to reach PAL active frame field2
#define	PAL_F2_SKIP				((PAL_ILAF2_START-1) * PAL_DATA_PER_LINE)
// PAL Interlaced active field size
#define	PAL_ILA_FIELD_SIZE		(PAL_ACTIVE_FLINES * PAL_DATA_PER_LINE)
// PAL Interlaced active video size
#define	PAL_ILA_VIDEO_SIZE		(PAL_ACTIVE_FLINES * ITU_ACTIVE_DATA_PER_LINE)

// Progressive PAL definitions
#define PAL_PRF_START       	45                      // PAL Progressive active frame start line
#define PAL_PRF_END         	620                     // PAL Progressive active frame finish line
// PAL Progressive active field size
#define	PAL_PRA_FIELD_SIZE		((PAL_PRF_END-PAL_PRF_START) * PAL_DATA_PER_LINE)
// PAL Progressive active video size
#define	PAL_PRA_VIDEO_SIZE		((PAL_PRF_END-PAL_PRF_START) * ITU_ACTIVE_DATA_PER_LINE)

/***************************************************************************** 

Enumarations for video formats

*****************************************************************************/

typedef enum    {           // Video formats
    NTSC_IL,                // NTSC Interlaced frame
    PAL_IL,                 // PAL Interlaced frame
    NTSC_PR,                // NTSC progressive frame
    PAL_PR                  // PAL progressice frame
}FRAME_TYPE;

/***************************************************************************** 

API Function Declerations

*****************************************************************************/

void	adi_video_FrameFormat (	// Formats a given memory area to a specified video frame
char 			*frame_ptr,     // Pointer to memory area to be formated (start of the video frame)
FRAME_TYPE 		frametype       // required video Frame type
);

void 	adi_video_FrameFill	( 	// Fills active video portions of a formated frame to specified color
char 			*frame_ptr,     // Pointer to the formated video frame (start of the video frame)
FRAME_TYPE 		frametype,      // Formated video Frame type
char 			*ycbcr_data     // 4 byte array of 32 bit color value of YCbCr data to be filled
); 

void 	adi_video_RowFill (    	// Fills a row of active video portion of formated frame with specified color
char 			*frame_ptr,     // Pointer to the formated video frame (start of the video frame)
FRAME_TYPE 		frametype,      // Formated Video Frame type
unsigned long 	row_value,     	// 32 bit value corresponding to row number to be filled
char 			*ycbcr_data     // 4 byte array of 32 bit color value of YCbCr data to be filled
); 

void 	adi_video_ColumnFill (  // Fills a Column of active video portion of formated frame with specified color
char 			*frame_ptr,     // Pointer to the formated video frame (start of the video frame)
FRAME_TYPE 		frametype,      // Formated Video Frame type
unsigned long 	column_value,   // 32 bit value corresponding to column number to be filled
char 			*ycbcr_data     // 4 byte array of 32 bit color value of YCbCr data to be filled
); 

/************************
*Added by liyao
*Start
*************************/
void adi_video_PixelSet(char* frame_ptr, unsigned long x, unsigned long y, char *ycbcr_data);
void adi_video_RegionSet(char* frame_ptr, unsigned long x, unsigned long y, unsigned long width, unsigned long height, char *ycbcr_data);
/*************************
*Added by liyao
*End
************************/


bool 	adi_video_CopyField (	// Copies Field 0 or Field 1 data to Field 1 or Field 0 location in a video frame
char 			*frame_ptr, 	// Pointer to the formated video frame (start of the video frame)
FRAME_TYPE 		frametype, 		// Formated video Frame type (NTSC/PAL)
char 			fieldsrc,		// Source Field to be copied (value must be either 0 or 1, corresponds to Field 0 or Field 1)
bool 			ActiveDataOnly	// Copies only active data in a field(TRUE)/Copies entire field (FALSE)
);
			
#endif                          // end ADI_ITU656_H

/*****/

