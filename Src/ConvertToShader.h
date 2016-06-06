#include <windows.h>
#include <cstdio>		//needed by OutputDebugString()
#include <math.h>
#include <limits.h>
#include <DirectXPackedVector.h>
#include "avisynth.h"
#include "d3dx9.h"
#include <mutex>

// Converts YV12 data into RGB data with float precision, 12-byte per pixel.
class ConvertToShader : public GenericVideoFilter {
public:
	ConvertToShader(PClip _child, int _precision, bool stack16, IScriptEnvironment* env);
	~ConvertToShader();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
	const VideoInfo& __stdcall GetVideoInfo() { return viDst; }
private:
	const int precision;
	const bool stack16;
	int precisionShift;
	int floatBufferPitch;
	int halfFloatBufferPitch;
	void convYV24ToShader(const byte *py, const byte *pu, const byte *pv,
		unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height, IScriptEnvironment* env);
	void convRgbToShader(const byte *src, unsigned char *dst, int srcPitch, int dstPitch, int width, int height, IScriptEnvironment* env);
	void convInt(uint8_t y, uint8_t u, uint8_t v, uint8_t* out);
	void convStack16(uint8_t y, uint8_t u, uint8_t v, uint8_t y2, uint8_t u2, uint8_t v2, uint8_t* out);
	void bitblt_i8_to_i16_sse2(const uint8_t* srcY, const uint8_t* srcU, const uint8_t* srcV, int srcPitch, uint16_t* dst, int dstPitch, int height);
	__m128i	load_8_16l(const void *lsb_ptr, __m128i zero);
	void store_8_16l(void *lsb_ptr, __m128i val, __m128i mask_lsb);
	VideoInfo viDst;
};