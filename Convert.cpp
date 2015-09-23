#include "Convert.h"

ConvertToShader::ConvertToShader(PClip _child, IScriptEnvironment* env) :
GenericVideoFilter(_child), precision(2), precisionShift(3), convertYUV(false) {
	if (!vi.IsYV12() && !vi.IsYV24())
		env->ThrowError("Source must be YV12 or YV24");

	// Convert from YV24 to half-float RGB
	viRGB = vi;
	viRGB.pixel_type = VideoInfo::CS_BGR32;
	viRGB.width <<= 1;
}

ConvertToShader::~ConvertToShader() {
}

PVideoFrame __stdcall ConvertToShader::GetFrame(int n, IScriptEnvironment* env) {
	PVideoFrame src = child->GetFrame(n, env);

	// Convert from YV12 to half-float RGB
	PVideoFrame dst = env->NewVideoFrame(viRGB);
	convYV24toRGB(src->GetReadPtr(PLANAR_Y), src->GetReadPtr(PLANAR_U), src->GetReadPtr(PLANAR_V), dst->GetWritePtr(), src->GetPitch(PLANAR_Y), src->GetPitch(PLANAR_U), dst->GetPitch(), vi.width, vi.height);
	return dst;
}

void ConvertToShader::convYV24toRGB(const byte *py, const byte *pu, const byte *pv,
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
void ConvertToShader::convFloat(int y, int u, int v, unsigned char* out) {
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


ConvertFromShader::ConvertFromShader(PClip _child, IScriptEnvironment* env) :
GenericVideoFilter(_child), precision(2), precisionShift(3), convertYUV(false) {
	if (!vi.IsRGB32())
		env->ThrowError("Source must be float-precision RGB");

	// Convert from float-precision RGB to YV12
	viYV = vi;
	viYV.pixel_type = VideoInfo::CS_YV24;

	// Float-precision RGB is 8-byte per pixel (4 half-float), YV12 is 2-byte per pixel
	viYV.width >>= 1;
}

ConvertFromShader::~ConvertFromShader() {
}


PVideoFrame __stdcall ConvertFromShader::GetFrame(int n, IScriptEnvironment* env) {
	PVideoFrame src = child->GetFrame(n, env);

	// Convert from float-precision RGB to YV12
	PVideoFrame dst = env->NewVideoFrame(viYV);
	convRGBtoYV24(src->GetReadPtr(),
		dst->GetWritePtr(PLANAR_Y), dst->GetWritePtr(PLANAR_U), dst->GetWritePtr(PLANAR_V),
		src->GetPitch(), dst->GetPitch(PLANAR_Y), dst->GetPitch(PLANAR_U), viYV.width, viYV.height);
	return dst;
}

void ConvertFromShader::convRGBtoYV24(const byte *src, unsigned char *py, unsigned char *pu, unsigned char *pv,
	int pitch1, int pitch2Y, int pitch2UV, int width, int height)
{
	unsigned char U, V;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			convFloat(src + (x << precisionShift), &py[x], &pu[x], &pv[x]);
		}
		src += pitch1;
		py += pitch2Y;
		pu += pitch2UV;
		pv += pitch2UV;
	}
}

// Using Rec601 color space. Can be optimized with MMX assembly or by converting on the GPU with a shader.
void ConvertFromShader::convFloat(const byte* src, byte* outY, unsigned char* outU, unsigned char* outV) {
	float r, g, b;
	D3DXFLOAT16 r2, g2, b2;
	memcpy(&r2, src + precision * 0, precision);
	memcpy(&g2, src + precision * 1, precision);
	memcpy(&b2, src + precision * 2, precision);
	b = (float)b2;
	g = (float)g2;
	r = (float)r2;

	// rgb are in the 0 to 1 range
	r = r / 1 * 255;
	b = b / 1 * 255;
	g = g / 1 * 255;

	float y, u, v;
	if (convertYUV) {
		y = (0.257f * r) + (0.504f * g) + (0.098f * b) + 16;
		v = (0.439f * r) - (0.368f * g) - (0.071f * b) + 128;
		u = -(0.148f * r) - (0.291f * g) + (0.439f * b) + 128;

		if (y > 255) y = 255;
		if (u > 255) u = 255;
		if (v > 255) v = 255;
		if (y < 0) y = 0;
		if (u < 0) u = 0;
		if (v < 0) v = 0;
	}
	else {
		y = r;
		u = g;
		v = b;
	}

	// Store YUV
	outY[0] = unsigned char(y);
	outU[0] = unsigned char(u);
	outV[0] = unsigned char(v);
}