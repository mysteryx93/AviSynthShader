#include <windows.h>
#include <cstdio>		//needed by OutputDebugString()
#include <math.h>
#include <limits.h>
#include <DirectXPackedVector.h>
#include "avisynth.h"
#include "d3dx9.h"

// Converts float-precision RGB data (12-byte per pixel) into YV12 format.
class ConvertFromShader : public GenericVideoFilter {
public:
	ConvertFromShader(PClip _child, const char* _format, int _precision, IScriptEnvironment* env);
	~ConvertFromShader();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
	const VideoInfo& __stdcall GetVideoInfo() { return viDst; }
private:
	const int precision;
	int precisionShift;
	const bool convertYUV = false;
	unsigned char* floatBuffer;
	int floatBufferPitch;
	unsigned char* halfFloatBuffer;
	int halfFloatBufferPitch;
	const char* format;
	void convFloatToYV24(const byte *src, unsigned char *py, unsigned char *pu, unsigned char *pv,
		int pitch1, int pitch2Y, int pitch2UV, int width, int height, IScriptEnvironment* env);
	void convFloatToRGB32(const byte *src, unsigned char *dst, int pitchSrc, int pitchDst, int width, int height, IScriptEnvironment* env);
	void convFloat(const byte* rgb, unsigned char* outY, unsigned char* outU, unsigned char* outV);
	void convInt(const byte* rgb, unsigned char* outY, unsigned char* outU, unsigned char* outV);
	uint16_t sadd16(uint16_t a, uint16_t b);
	VideoInfo viDst;
};