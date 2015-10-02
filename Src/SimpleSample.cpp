/*
	SimpleSample plugin for Avisynth -- a simple sample

	Copyright (C) 2002-2003 Simon Walters, All Rights Reserved

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	The author can be contacted at:
	Simon Walters
	siwalters@hotmail.com

	V1.0 - 1st release.
	V1.0a - Revised version to put colourspace checking in the right place.
	V1.0b - Added detailed comments.  // sh0dan
	V1.1 - Added sh0dan's planar (YV12) code
	V1.2 - Revert to RGB24 colourspace only to show simple pixel manipulation;
	V1.3 - add in a parameter to vary the size of the square.
	V1.4 - Add in RGB32 colourspace code
	V1.5 - Add in YUY2 colourspace code
	V1.5a - bug fix in max Y1 and Y2 values
	V1.6 - Add in YV12 colourspace - yippee.
	V1.7 - add in ability to use info from 2 clips only the YUY2 colorspace code altered
	V1.8 - merged with V2.6-compatible Wiki version,
			conditionally 'in-place' operation. Window clip removed. // m53

*/

//following 2 includes needed
#include <windows.h>
#include <cstdio>		//needed by OutputDebugString()
#include "avisynth.h"	//version 5, AviSynth 2.6 +

/****************************
 * The following is the header definitions.
 * For larger projects, move this into a .h file
 * that can be included.
 ****************************/

/***************************************************************
	CLASS DECLARATION
***************************************************************/

// SimpleSample defines the name of your filter class.
// This name is only used internally, and does not affect the name of your filter or similar.
// This filter extends GenericVideoFilter, which incorporates basic functionality.
// All functions present in the filter must also be present here.
class SimpleSample : public GenericVideoFilter {

	// define the class variables
	int SquareSize;
	bool InPlace;

// This defines that these functions are present in your class.
// These functions must be that same as those actually implemented.
// Since the functions are "public" they are accessible to other classes.
// Otherwise they can only be called from functions within the class itself.
public:

/***************************************************************
	CONSTRUCTOR DECLARATION
***************************************************************/

	// This is the constructor. It does not return any value, and is always used,
	// when an instance of the class is created.
	// Since there is no code in this, this is the declaration.
	SimpleSample(PClip _child, int _SquareSize, bool _InPlace, IScriptEnvironment* env);

/***************************************************************
	DESTRUCTOR DECLARATION
***************************************************************/

	// The is the destructor declaration. This is called when the filter is destroyed.
	// Destructors have no parameters (its so sad)
	~SimpleSample();

/***************************************************************
	WORKER FUNCTION DECLARATION
***************************************************************/

	// This is the function that AviSynth calls to get a given frame.
	// So when this functions gets called, the filter is supposed to return frame n.
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);

};	// end of Class SimpleSample

/***************************
 * The following is the implementation
 * of the defined functions.
 ***************************/

/***************************************************************
	CONSTRUCTOR DEFINITION
***************************************************************/

// This is the implementation of the constructor.
// The child clip (source clip) is inherited by the GenericVideoFilter,
//  where the following variables get defined:
//   PClip child;   // Contains the source clip.
//   VideoInfo vi;  // Contains videoinfo on the source clip.
SimpleSample::SimpleSample(PClip _child, int _SquareSize, bool _InPlace, IScriptEnvironment* env) :
	GenericVideoFilter(_child), SquareSize(_SquareSize), InPlace(_InPlace) {

	if (vi.width<SquareSize || vi.height<SquareSize) {
		env->ThrowError("SimpleSample: square doesn't fit into the clip!");
	}
}	// end of constructor

/***************************************************************
	DESTRUCTOR DEFINITION
***************************************************************/

SimpleSample::~SimpleSample() {
	// This is where any actual destructor code used goes
	// here you must deallocate any memory you might have used.
}	// end of destructor

/***************************************************************
	WORKER FUNCTION DEFINITION
***************************************************************/

// This is the implementation of the GetFrame function.
// See the header definition for further info.
PVideoFrame __stdcall SimpleSample::GetFrame(int n, IScriptEnvironment* env) {

	// Request frame 'n' from the child (source) clip.
	PVideoFrame src = child->GetFrame(n, env);
	PVideoFrame dst;

	if (!InPlace) {
		// Construct a frame based on the information of the current frame contained in the "vi" struct.
		// Note that only this allows to use source dimensions for writing loops.
		// If the destination frame dimensions are different from source dimensions,
		// care must be taken to have separate address calculations
		dst = env->NewVideoFrame(vi);
	} else {
		// As a cheap trick, a VideoFrame type variable dst is also defined for in-place operation.
		// This allows to write to dst in both operation modes.
		dst = src;
		// In in-place operation mode, the source clip must be made writable.
		env->MakeWritable(&dst);
	}

	// Request a Read pointer from the source frame.
	// This will return the position of the upperleft pixel in YUY2 images,
	// and return the lower-left pixel in RGB.
	// RGB images are stored upside-down in memory.
	// You should still process images from line 0 to height.
	const unsigned char* srcp = src->GetReadPtr();

	// Request a Write pointer from the newly created destination image.
	// You can request a writepointer to images that have just been
	// created by NewVideoFrame. If you recieve a frame from PClip->GetFrame(...)
	// you must call env->MakeWritable(&frame) be recieve a valid write pointer.
	unsigned char* dstp = dst->GetWritePtr();

	// Requests pitch (length of a line) of the destination image.
	// For more information on pitch see: http://www.avisynth.org/index.php?page=WorkingWithImages
	// (short version - pitch is always equal to or greater than width to allow for seriously fast assembly code)
	int dst_pitch = dst->GetPitch();

	// Requests rowsize (number of used bytes in a line.
	// See the link above for more information.
	int dst_width = dst->GetRowSize();

	// Requests the height of the destination image.
	int dst_height = dst->GetHeight();

	int src_pitch = src->GetPitch();
	int src_width = src->GetRowSize();
	int src_height = src->GetHeight();

	int w, h;

	// This code deals with RGB24 colourspace where each pixel is represented by 3 bytes, Blue, Green and Red.
	// Although this colourspace is the easiest to understand, it is very rarely used because
	// a 3 byte sequence (24bits) cannot be processed easily using normal 32 bit registers.
	if (vi.IsRGB24()) {

		if (!InPlace) {
			for (h=0; h<src_height; h++) {			// Loop from bottom line to top line
				for (w=0; w<src_width; w+=3) {		// Loop from left side of the image to the right side 1 pixel (3 bytes) at a time
													// stepping 3 bytes (a pixel width in RGB24 space)

					dstp[w]   = srcp[w];			// Copy each Blue byte from source to destination
					dstp[w+1] = srcp[w+1];			// Copy Green
					dstp[w+2] = srcp[w+2];			// Copy Red
				}

				srcp += src_pitch;					// Add the pitch (note use of pitch and not width) of one line (in bytes) to the source pointer
				dstp += dst_pitch;					// Add the pitch to the destination pointer
			}
		// end copy src to dst
		}

		//Now draw a white square in the middle of the frame
		// Normally you'd do this code within the loop above but here it is in a separate loop for clarity;

		// reset the destination pointer to the bottom, left pixel. (RGB colourspaces only)
		dstp = dst->GetWritePtr();
		// move pointer up to SquareSize/2 lines from the middle of the frame
		dstp += (dst_height - SquareSize)/2*dst_pitch;
		// move pointer right to SquareSize/2 columns from the middle of the frame
		dstp += (dst_width - 3*SquareSize)/2;

		for (h=0; h<SquareSize; h++) {			// only scan 100 lines
			for (w=0; w<3*SquareSize; w+=3) {	// only scans the middle SquareSize pixels of a line
				dstp[w]   = 255;				// Set Blue to maximum value
				dstp[w+1] = 255;				// Set Green
				dstp[w+2] = 255;				// Set Red
			}
			dstp += dst_pitch;
		}
	} // end vi.IsRGB24

	// This code deals with RGB32 colourspace where each pixel is represented by
	// 4 bytes, Blue, Green and Red and "spare" byte that could/should be used for alpha keying.
	// Although this colourspace isn't memory efficient, code end ups running much
	// quicker than RGB24 as you can deal with whole 32bit variables at a time
	// and easily work directly and quickly in assembler (if you know how to that is :-)
	if (vi.IsRGB32()) {

		if (!InPlace) {
			for (h=0; h<src_height; h++) {			// Loop from bottom line to top line.
				for (w=0; w<src_width/4; w++) {		// and from leftmost pixel to rightmost one.

					// Copy each whole pixel from source to destination
					// by temporarily treating the src and dst pointers as pixel pointers intead of byte pointers
					*((unsigned int *)dstp + w) = *((unsigned int *)srcp + w);
				}

				srcp += src_pitch; // Add the pitch (note use of pitch and not width) of one line (in bytes) to the source pointer
				dstp += dst_pitch; // Add the pitch to the destination pointer.
			}
			// end copy src to dst
		}

		//Now draw a white square in the middle of the frame

		// reset the destination pointer to the bottom, left pixel. (RGB colourspaces only)
		dstp = dst->GetWritePtr();
		// move pointer up to SquareSize/2 lines from the middle of the frame
		dstp += (dst_height - SquareSize)/2*dst_pitch;
		// move pointer right to SquareSize/2 columns from the middle of the frame
		dstp += (dst_width - 4*SquareSize)/2;

		for (h=0; h<SquareSize; h++) {			// only scan SquareSize number of lines
			for (w=0; w<SquareSize; w++) {		// only scans the middle SquareSize pixels of a line
				*((unsigned int *)dstp + w) = 0xFFFFFFFF;	// Set Red,Green and Blue to maximum value in 1 instruction.
															// LSB = Blue, MSB = alpha
			}
			dstp += dst_pitch;
		}
	}	// end vi.IsRGB32

	// This code deals with YUY2 colourspace where each 4 byte sequence represents
	// 2 pixels, (Y1, U, Y2 and then V).

	// This colourspace is more memory efficient than RGB32 but can be more awkward to use sometimes.
	// However, it can still be manipulated 32bits at a time depending on the
	// type of filter you are writing

	// There is no difference in code for this loop and the RGB32 code due to a coincidence :-)
	// 1) YUY2 frame_width is half of an RGB32 one
	// 2) But in YUY2 colourspace, a 32bit variable holds 2 pixels instead of the 1 in RGB32 colourspace.
	if (vi.IsYUY2()) {

		if (!InPlace) {
			for (h=0; h<src_height; h++) {       // Loop from top line to bottom line (opposite of RGB colourspace).
				for (w=0; w<src_width/4; w++) {	// and from leftmost double-pixel to rightmost one.

					// Copy 2 pixels worth of information from source to destination
					// at a time by temporarily treating the src and dst pointers as
					// 32bit (4 byte) pointers intead of 8 bit (1 byte) pointers
					*((unsigned int *)dstp + w) = *((unsigned int *)srcp + w);
				}																					
																								
				srcp += src_pitch; // Add the pitch (note use of pitch and not width) of one line (in bytes) to the source pointer
				dstp += dst_pitch; // Add the pitch to the destination pointer.
			}
			// end copy src to dst
		}

		//Now draw a white square in the middle of the frame

		// reset the destination pointer to the top, left pixel. (YUY2 colourspace only)
		dstp = dst->GetWritePtr();
		// move pointer up to SquareSize/2 lines from the middle of the frame
		dstp += (dst_height - SquareSize)/2*dst_pitch;
		// move pointer right to SquareSize/2 columns from the middle of the frame
		dstp += (dst_width - 2*SquareSize)/2;

		for (h=0; h<SquareSize; h++) { // only scan SquareSize number of lines
			for (w=0; w<SquareSize/2; w++) { // only scans the middle SquareSize pixels of a line
				*((unsigned int *)dstp + w) = 0x80EB80EB;
			}
			dstp += dst_pitch;
		}
	}	// end vi.IsYUY2

	// This code deals with planar YUV colourspaces where the Y, U and V information are
	// stored in completely separate memory areas
	if (vi.IsPlanar() && vi.IsYUV()) {

		int planes[] = {PLANAR_Y, PLANAR_U, PLANAR_V};
		int square_value[] = {235, 128, 128};
		int p;
		int width_sub, height_sub;

		// This section of code deals with the three Y, U and V planes of planar formats (e.g. YV12)
		// So first of all we have to get the additional info on the resp. plane

		for (p=0; p<3; p++) {

			srcp = src->GetReadPtr(planes[p]);
			dstp = dst->GetWritePtr(planes[p]);

			src_pitch = src->GetPitch(planes[p]);
			dst_pitch = dst->GetPitch(planes[p]);
			width_sub = vi.GetPlaneWidthSubsampling(planes[p]);
			height_sub = vi.GetPlaneHeightSubsampling(planes[p]);
/*
			char debugbuf[100];
			sprintf(debugbuf,"SimpleSample: plane=%d width_sub=%d height_sub=%d", p, width_sub, height_sub);
			OutputDebugString(debugbuf);
*/

			if (!InPlace) {
				//Copy actual plane src to dst
				for (h=0; h<(src_height>>height_sub); h++) {
					for (w=0; w<(src_width>>width_sub); w++)
						dstp[w] = srcp[w];
					srcp += src_pitch;
					dstp += dst_pitch;
				}
				// end copy plane src to dst
			}

			//Now draw a white square in the middle of the frame

			// reset the destination pointer to the top, left pixel
			dstp = dst->GetWritePtr(planes[p]);
			// note: by much do we divde SquareSize
			dstp += ((dst_height - SquareSize)>>height_sub)/2*dst_pitch;
			dstp += ((dst_width - SquareSize)>>width_sub)/2;

			for (h=0; h<SquareSize>>height_sub; h++) {
				for (w=0; w<SquareSize>>width_sub; w++) {
					dstp[w] = square_value[p];
				}
				dstp += dst_pitch;
			}	// end of plane code
		} // end for planes

	} // end if planar

	// As we now are finished processing the image, we return the destination image.
	return dst;

}	// end GetFrame function

// This is the function that creates the filter, when the filter is called.
// This can be used for parameter checking, so it is possible to create different filters,
// based on the arguments recieved.

AVSValue __cdecl Create_SimpleSample(AVSValue args, void* user_data, IScriptEnvironment* env) {
	// Calls the constructor with the arguments provided
	// Therefore, parameter list must match that of the constructor, of course
	return new SimpleSample(
		args[0].AsClip(),		// source clip
		args[1].AsInt(100),		// size of the square in pixels
		args[2].AsBool(false),	// switch for in-place operation
		env);					// env is the link to essential informations, always provide it
/*
Possible parameter types, with defaults:
		args [0].AsClip (),
		args [1].AsBool (true),
		args [2].AsInt (),
		args [3].AsFloat (0.0),
		args [4].AsString (""),
		args [5].Defined (),	// is 'void'
*/
} // end Create_SimpleSample constructor call

// new V2.6 requirement
const AVS_Linkage *AVS_linkage = 0;

// The following function is the function that actually registers the filter in AviSynth
// It is called automatically when the plugin is loaded to see which functions this filter contains.

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(IScriptEnvironment* env, const AVS_Linkage* const vectors) {
	AVS_linkage = vectors;

	// The AddFunction has the following paramters:
	// AddFunction(Filtername , Arguments, Function to call,0);
	env->AddFunction("SimpleSample", "c[size]i[inplace]b", Create_SimpleSample, 0);

	// Arguments is a string that defines the types and optional names of the arguments for you filter.
	// c - Video Clip
	// i - Integer number
	// f - Float number
	// s - String
	// b - boolean

	 // The word inside the [ ] lets you used named parameters in your script
	 // e.g last=SimpleSample(last,size=100).
	 // but last=SimpleSample(last, 100) will also work

	// A freeform name of the plugin
	return "SimpleSample plugin";
	
} // end AvisynthPluginInit3 function

