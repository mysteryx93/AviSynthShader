#include <windows.h>
#include <cstdio>		//needed by OutputDebugString()
#include <math.h>
#include <limits.h>
#include "avisynth.h"
#include "d3dx9.h"

// Converts YV12 data into RGB data with float precision, 12-byte per pixel.
class ConvertToFloat : public GenericVideoFilter {
public:
	ConvertToFloat(PClip _child, bool _convertYuv, IScriptEnvironment* env);
	~ConvertToFloat();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
	const VideoInfo& __stdcall GetVideoInfo() { return viRGB; }
private:
	const int precision;
	const int precisionShift;
	const bool convertYUV;
	const D3DXFLOAT16 AlphaValue = D3DXFLOAT16(1);
	void ConvertToFloat::convYV24toRGB(const byte *py, const byte *pu, const byte *pv,
		unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height);
	void ConvertToFloat::convFloat(int y, int u, int v, unsigned char *out);
	VideoInfo viRGB;
};