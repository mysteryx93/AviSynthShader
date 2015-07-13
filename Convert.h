#include <windows.h>
#include <cstdio>		//needed by OutputDebugString()
#include "avisynth.h"	//version 5, AviSynth 2.6 +
#include "internal.h"

// Converts YV12 data into RGB data with float precision, 12-byte per pixel.
class ConvertToShader : public GenericVideoFilter {
public:
	ConvertToShader(PClip _child, int _precision, IScriptEnvironment* env);
	~ConvertToShader();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
	const VideoInfo& __stdcall GetVideoInfo() { return viRGB; }
private:
	void ConvertToShader::conv420toRGB(const byte *py, const byte *pu, const byte *pv, unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height);
	void ConvertToShader::convFloat(int y, int u, int v, unsigned char *out);
	VideoInfo viRGB;
	int precision;
	int precisionShift;
	bool copyYUV;
};


// Converts float-precision RGB data (12-byte per pixel) into YV12 format.
class ConvertFromShader : public GenericVideoFilter {
public:
	ConvertFromShader(PClip _child, int _precision, IScriptEnvironment* env);
	~ConvertFromShader();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
	const VideoInfo& __stdcall GetVideoInfo() { return viYV; }
private:
	void ConvertFromShader::convFloatRGBto420(const byte *src, unsigned char *py, unsigned char *pu, unsigned char *pv, int pitch1, int pitch2Y, int pitch2UV, int width, int height);
	void ConvertFromShader::convFloat(const byte* rgb, unsigned char* outY, unsigned char* outU, unsigned char* outV);
	VideoInfo viYV;
	int precision;
	int precisionShift;
	bool copyYUV;
};