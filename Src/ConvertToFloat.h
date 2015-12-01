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
	const VideoInfo& __stdcall GetVideoInfo() { return viDst; }
private:
	const int precision;
	int precisionShift;
	const bool convertYUV;
	const float AlphaFloat = 1;
	const unsigned short AlphaShort = 0; // UINT16_MAX;
	unsigned char* floatBuffer;
	int floatBufferPitch;
	unsigned char* halfFloatBuffer;
	int halfFloatBufferPitch;
	void convYV24ToFloat(const byte *py, const byte *pu, const byte *pv,
		unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height, IScriptEnvironment* env);
	void convRgbToFloat(const byte *src, unsigned char *dst, int srcPitch, int dstPitch, int width, int height, IScriptEnvironment* env);
	void convFloat(unsigned char y, unsigned char u, unsigned char v, unsigned char *out);
	void convInt(byte y, unsigned char u, unsigned char v, unsigned char* out);
	void ConvertToFloat::bitblt_i8_to_i16_sse2(const uint8_t* srcY, const uint8_t* srcU, const uint8_t* srcV, int srcPitch, uint16_t* dst, int dstPitch, int height);
	__m128i	load_8_16l(const void *lsb_ptr, __m128i zero);
	void store_8_16l(void *lsb_ptr, __m128i val, __m128i mask_lsb);
	VideoInfo viDst;

	// Declare variables in convFloat here to avoid re-assigning them for every pixel.
};