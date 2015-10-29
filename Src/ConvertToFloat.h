#include <windows.h>
#include <cstdio>		//needed by OutputDebugString()
#include <math.h>
#include <limits.h>
#include <DirectXPackedVector.h>
#include "avisynth.h"
#include "d3dx9.h"

// Converts YV12 data into RGB data with float precision, 12-byte per pixel.
class ConvertToFloat : public GenericVideoFilter {
public:
	ConvertToFloat(PClip _child, bool _convertYuv, int _precision, IScriptEnvironment* env);
	~ConvertToFloat();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
	// const VideoInfo& __stdcall GetVideoInfo() { return viRGB; }
private:
	const int precision;
	int precisionShift;
	const bool convertYUV;
	const float AlphaValue = 1;
	unsigned char* floatBuffer;
	int floatBufferPitch;
	unsigned char* halfFloatBuffer;
	int halfFloatBufferPitch;
	void convYV24ToFloat(const byte *py, const byte *pu, const byte *pv,
		unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height, IScriptEnvironment* env);
	void convRgbToFloat(const byte *src, unsigned char *dst, int srcPitch, int dstPitch, int width, int height, IScriptEnvironment* env);
	void convFloat(byte y, byte u, byte v, unsigned char *out);
	int srcWidth, srcHeight;
	bool srcRgb;

	// Declare variables in convFloat here to avoid re-assigning them for every pixel.
	int r, g, b;
	unsigned char r2, g2, b2;
	float rf, gf, bf;
};