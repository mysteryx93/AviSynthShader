#include <windows.h>
#include <cstdio>		//needed by OutputDebugString()
#include <math.h>
#include <limits.h>
#include "avisynth.h"
#include "d3dx9.h"

// Converts float-precision RGB data (12-byte per pixel) into YV12 format.
class ConvertFromFloat : public GenericVideoFilter {
public:
	ConvertFromFloat(PClip _child, const char* _format, bool _convertYuv, IScriptEnvironment* env);
	~ConvertFromFloat();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
	const VideoInfo& __stdcall GetVideoInfo() { return viYV; }
private:
	const int precision;
	const int precisionShift;
	const bool convertYUV;
	const char* format;
	void convFloatToYV24(const byte *src, unsigned char *py, unsigned char *pu, unsigned char *pv,
		int pitch1, int pitch2Y, int pitch2UV, int width, int height);
	void convFloatToRGB32(const byte *src, unsigned char *dst, int pitchSrc, int pitchDst, int width, int height);
	void convFloat(const byte* rgb, unsigned char* outY, unsigned char* outU, unsigned char* outV);
	VideoInfo viYV;
};