#include "Convert.h"

// This page provides a good overview of YUV to RGB conversion.
// http://www.fourcc.org/fccyvrgb.php
// In this case, we want 0-255 RGB values into a 0-1 float, which is what HLSL texture shaders expect.
// There may be color matrix distortion (interpreting Rec709 data in Rec601), but converting back with
// the same formula will cancel most of the distortion. There might be color distortion on the effect of the shader.

ConvertToShader::ConvertToShader(PClip _child, int _precision, IScriptEnvironment* env) :
GenericVideoFilter(_child), precision(_precision) {
	if (!vi.IsYV12())
		env->ThrowError("Source must be YV12");

	// Convert from YV12 to float-precision RGB
	viRGB = vi;
	viRGB.pixel_type = VideoInfo::CS_BGR32;

	// RGBA is 4-byte per pixel, float-precision RGB is 16-byte per pixel (4 float). We need to increase width to store all the data.
	if (precision == 2)
		viRGB.width <<= 1;
	else if (precision == 4)
		viRGB.width <<= 2;
}

ConvertToShader::~ConvertToShader() {
}

PVideoFrame __stdcall ConvertToShader::GetFrame(int n, IScriptEnvironment* env) {
	PVideoFrame src = child->GetFrame(n, env);

	// Convert from YV12 to float-precision RGB
	PVideoFrame dst = env->NewVideoFrame(viRGB);
	conv420toRGB(src->GetReadPtr(PLANAR_Y), src->GetReadPtr(PLANAR_U), src->GetReadPtr(PLANAR_V), dst->GetWritePtr(), src->GetPitch(PLANAR_Y), src->GetPitch(PLANAR_U), dst->GetPitch(), vi.width, vi.height);
	return dst;
}

void ConvertToShader::conv420toRGB(const byte *py, const byte *pu, const byte *pv,
	unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height)
{
	width >>= 1;
	height >>= 1;
	int Y1, Y2, Y3, Y4, U, V;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			Y1 = py[x << 1];
			U = pu[x];
			Y2 = py[(x << 1) + 1];
			V = pv[x];
			// pixels on 2nd row
			Y3 = py[(x << 1) + pitch1Y];
			Y4 = py[(x << 1) + pitch1Y + 1];

			convFloat(Y1, U, V, &dst[(x << (1+precision))]);
			convFloat(Y2, U, V, &dst[(x << (1+precision)) + precision * 4]);
			convFloat(Y3, U, V, &dst[(x << (1+precision))] + pitch2);
			convFloat(Y4, U, V, &dst[(x << (1+precision))] + pitch2 + precision * 4);
		}
		py += pitch1Y*2;
		pu += pitch1UV;
		pv += pitch1UV;
		dst += pitch2*2;
	}
}

// Using Rec601 color space. Can be optimized with MMX assembly or by converting on the GPU with a shader.
// Precision can be 
// 0 = YUV, 1 byte per channel (to convert in a shader)
// 1 = RGB, 1 byte per channel (valid for display)
// 2 = Half-Float, 2 byte per channel (better performance)
// 4 = Float, 4 byte per channel (better quality)
void ConvertToShader::convFloat(int y, int u, int v, unsigned char* out) {
	float r, g, b;
	if (precision > 0) {
		r = y + 1.403f * u;
		g = y - 0.344f * u - 0.714f * v;
		b = y + 1.770f * u;

		if (r > 255) r = 255;
		if (g > 255) g = 255;
		if (b > 255) b = 255;
		if (r < 0) r = 0;
		if (g < 0) g = 0;
		if (b < 0) b = 0;

		// Texture shaders expect data between 0 and 1
		r = r / 255 * 1;
		g = g / 255 * 1;
		b = b / 255 * 1;
	}
	else {
		// Pass YUV values to be converted by a shader
		r = float(y);
		g = float(u);
		b = float(v);
	}

	// Convert the data at the position of RGB into specified specifion
	if (precision == 0 || precision == 1) {
		unsigned char r2 = unsigned char(r);
		memcpy(&r, &r2, precision);
	}
	else if (precision == 2) {
		// Convert to half float
	}

	// Store BGRA
	memcpy(out + 0, &b, precision);
	memcpy(out + precision, &g, precision);
	memcpy(out + precision, &r, precision);
	// Empty alpha channel
	ZeroMemory(&out[precision*3], precision);
}


ConvertFromShader::ConvertFromShader(PClip _child, int _precision, IScriptEnvironment* env) :
GenericVideoFilter(_child), precision(_precision) {
	if (!vi.IsRGB32())
		env->ThrowError("Source must be float-precision RGB");

	// Convert from float-precision RGB to YV12
	viYV = vi;
	viYV.pixel_type = VideoInfo::CS_YV12;
	viYV.width >>= 2; // Float-precision RGB is 16-byte per pixel (4 float), YV12 is 2-byte per pixel
}

ConvertFromShader::~ConvertFromShader() {
}


PVideoFrame __stdcall ConvertFromShader::GetFrame(int n, IScriptEnvironment* env) {
	PVideoFrame src = child->GetFrame(n, env);

	// Convert from float-precision RGB to YV12
	PVideoFrame dst = env->NewVideoFrame(viYV);
	convFloatRGBto420(src->GetReadPtr(), 
		dst->GetWritePtr(PLANAR_Y), dst->GetWritePtr(PLANAR_U), dst->GetWritePtr(PLANAR_V), 
		src->GetPitch(), dst->GetPitch(PLANAR_Y), dst->GetPitch(PLANAR_U), viYV.width, viYV.height);
	return dst;
}

void ConvertFromShader::convFloatRGBto420(const byte *src, unsigned char *py, unsigned char *pu, unsigned char *pv,
	int pitch1, int pitch2Y, int pitch2UV, int width, int height)
{
	width >>= 1;
	height >>= 1;
	unsigned char U[4], V[4];
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			convFloat(&src[(x << 3)], &py[x << 1], &U[0], &V[0]);
			convFloat(&src[(x << 3) + precision*4], &py[(x << 1) + 1], &U[1], &V[1]);
			convFloat(&src[(x << 3) + pitch1], &py[(x << 1) + 1], &U[2], &V[2]);
			convFloat(&src[(x << 3) + pitch1 + precision*4], &py[(x << 1) + pitch2Y + 1], &U[3], &V[3]);
			pu[x] = (U[0] + U[1] + U[2] + U[3]) / 4;
			pv[x] = (V[0] + V[1] + V[2] + V[3]) / 4;
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
	memcpy(&b, &src[4], sizeof(float));
	memcpy(&g, &src[8], sizeof(float));
	memcpy(&r, &src[12], sizeof(float));

	// rgb are in the 0 to 1 range
	r = r / 1 * 255;
	b = b / 1 * 255;
	g = g / 1 * 255;

	float y, u, v;
	y = 0.299f * r + 0.587f * g + 0.114f * b;
	u = (b - y) * 0.565f;
	v = (r - y) * 0.713f;

	if (y > 255) y = 255;
	if (u > 255) u = 255;
	if (v > 255) v = 255;
	if (y < 0) y = 0;
	if (u < 0) u = 0;
	if (v < 0) v = 0;

	// Store YUV
	outY[0] = unsigned char(y);
	outU[0] = unsigned char(u);
	outV[0] = unsigned char(v);
}