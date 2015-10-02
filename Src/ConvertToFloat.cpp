#include "ConvertToFloat.h"

ConvertToFloat::ConvertToFloat(PClip _child, bool _convertYuv, IScriptEnvironment* env) :
GenericVideoFilter(_child), precision(2), precisionShift(3), convertYUV(_convertYuv) {
	if (!vi.IsYV24() && !vi.IsRGB32())
		env->ThrowError("Source must be YV12, YV24 or RGB32");

	// Convert from YV24 to half-float RGB
	viRGB = vi;
	viRGB.pixel_type = VideoInfo::CS_BGR32;
	viRGB.width <<= 1;
}

ConvertToFloat::~ConvertToFloat() {
}

PVideoFrame __stdcall ConvertToFloat::GetFrame(int n, IScriptEnvironment* env) {
	PVideoFrame src = child->GetFrame(n, env);

	// Convert from YV24 to half-float RGB
	PVideoFrame dst = env->NewVideoFrame(viRGB);
	convYV24toRGB(src->GetReadPtr(PLANAR_Y), src->GetReadPtr(PLANAR_U), src->GetReadPtr(PLANAR_V), dst->GetWritePtr(), src->GetPitch(PLANAR_Y), src->GetPitch(PLANAR_U), dst->GetPitch(), vi.width, vi.height);
	return dst;
}

void ConvertToFloat::convYV24toRGB(const byte *py, const byte *pu, const byte *pv,
	unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height)
{
	width;
	height;
	int Y, U, V;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			Y = py[x];
			U = pu[x];
			V = pv[x];

			convFloat(Y, U, V, dst + (x << precisionShift));
		}
		py += pitch1Y;
		pu += pitch1UV;
		pv += pitch1UV;
		dst += pitch2;
	}
}

// Using Rec601 color space. Can be optimized with MMX assembly or by converting on the GPU with a shader.
void ConvertToFloat::convFloat(int y, int u, int v, unsigned char* out) {
	float r, g, b;
	if (convertYUV) {
		b = 1.164f * (y - 16) + 2.018f * (u - 128);
		g = 1.164f * (y - 16) - 0.813f * (v - 128) - 0.391f * (u - 128);
		r = 1.164f * (y - 16) + 1.596f * (v - 128);
	}
	else {
		// Pass YUV values to be converted by a shader
		r = float(y);
		g = float(u);
		b = float(v);
	}

	// Texture shaders expect data between 0 and 1
	r = r / 255 * 1;
	g = g / 255 * 1;
	b = b / 255 * 1;

	// Convert the data at the position of RGB with 16-bit float values.
	D3DXFLOAT16 r2 = D3DXFLOAT16(r);
	D3DXFLOAT16 g2 = D3DXFLOAT16(g);
	D3DXFLOAT16 b2 = D3DXFLOAT16(b);
	memcpy(out + precision * 0, &r2, precision);
	memcpy(out + precision * 1, &g2, precision);
	memcpy(out + precision * 2, &b2, precision);
	memcpy(out + precision * 3, &AlphaValue, precision);
}