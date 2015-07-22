#include "Convert.h"

// This page provides a good overview of YUV to RGB conversion.
// http://www.fourcc.org/fccyvrgb.php
// In this case, we want 0-255 RGB values into a 0-1 float, which is what HLSL texture shaders expect.
// There may be color matrix distortion (interpreting Rec709 data in Rec601), but converting back with
// the same formula will cancel most of the distortion. There might be color distortion on the effect of the shader.

// Precision can be 
// 0 = YUV, 1 byte per channel (to convert in a shader)
// 1 = RGB, 1 byte per channel (valid for display)
// 2 = Half-Float, 2 byte per channel (better performance)
// 4 = Float, 4 byte per channel (better quality)

ConvertToShader::ConvertToShader(PClip _child, int _precision, IScriptEnvironment* env) :
GenericVideoFilter(_child) {
	if (!vi.IsYV12() && !vi.IsYV24())
		env->ThrowError("Source must be YV12 or YV24");
	if (_precision != 1 && _precision != 2 && _precision != 4)
		env->ThrowError("Precision must be 0, 1, 2 or 4");

	// Convert from YV24 to float-precision RGB
	viRGB = vi;
	viRGB.pixel_type = VideoInfo::CS_BGR32;

	// Initialize variables.
	if (_precision == 0) {
		precision = 1;
		copyYUV = true;
	}
	else {
		precision = _precision;
		copyYUV = false;
	}

	// Avoid having to convert this for every pixel.
	// RGBA is 4-byte per pixel, float-precision RGB is 16-byte per pixel (4 float). We need to increase width to store all the data.
	if (precision == 1)
		precisionShift = 2;
	else if (precision == 2) {
		viRGB.width <<= 1;
		precisionShift = 3;
	}
	else if (precision == 4) {
		viRGB.width <<= 2;
		precisionShift = 4;
	}
}

ConvertToShader::~ConvertToShader() {
}

PVideoFrame __stdcall ConvertToShader::GetFrame(int n, IScriptEnvironment* env) {
	PVideoFrame src = child->GetFrame(n, env);

	// Convert from YV12 to float-precision RGB
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

void ConvertToShader::conv420toRGB(const byte *py, const byte *pu, const byte *pv,
	unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height)
{
	width >>= 1;
	height >>= 1;
	int Y1, Y2, Y3, Y4, U, V;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			Y1 = py[x << 1];
			Y2 = py[(x << 1) + 1];
			Y3 = py[(x << 1) + pitch1Y]; // 2nd row
			Y4 = py[(x << 1) + pitch1Y + 1];
			U = pu[x];
			V = pv[x];

			convFloat(Y1, U, V, dst + (x << precisionShift));
			convFloat(Y2, U, V, dst + (x << precisionShift) + precision * 4);
			convFloat(Y3, U, V, dst + (x << precisionShift) + pitch2);
			convFloat(Y4, U, V, dst + (x << precisionShift) + pitch2 + precision * 4);
		}
		py += pitch1Y * 2;
		pu += pitch1UV;
		pv += pitch1UV;
		dst += pitch2 * 2;
	}
}

// Using Rec601 color space. Can be optimized with MMX assembly or by converting on the GPU with a shader.
void ConvertToShader::convFloat(int y, int u, int v, unsigned char* out) {
	float r, g, b;
	if (copyYUV) {
		// Pass YUV values to be converted by a shader
		r = float(y);
		g = float(u);
		b = float(v);
	}
	else {
		b = 1.164f * (y - 16) + 2.018f * (u - 128);
		g = 1.164f * (y - 16) - 0.813f * (v - 128) - 0.391f * (u - 128);
		r = 1.164f * (y - 16) + 1.596f * (v - 128);

		// Don't crop float values, this would cause loss of data.
		if (precision == 1) {
			if (r > 255) r = 255;
			if (g > 255) g = 255;
			if (b > 255) b = 255;
			if (r < 0) r = 0;
			if (g < 0) g = 0;
			if (b < 0) b = 0;
		}

		// Texture shaders expect data between 0 and 1
		if (precision > 1) {
			r = r / 255 * 1;
			g = g / 255 * 1;
			b = b / 255 * 1;
		}
	}

	// Convert the data at the position of RGB into specified specifion
	if (precision == 0 || precision == 1) {
		unsigned char r2 = unsigned char(r);
		unsigned char g2 = unsigned char(g);
		unsigned char b2 = unsigned char(b);
		memcpy(out + 0, &b2, precision);
		memcpy(out + precision, &g2, precision);
		memcpy(out + precision * 2, &r2, precision);
		out[precision * 3] = 0;
	}
	else if (precision == 2) {
		// Convert to half float
		//memcpy(out + 0, &b2, precision);
		//memcpy(out + precision, &g2, precision);
		//memcpy(out + precision * 2, &r2, precision);
	}
	else {
		ZeroMemory(out, precision); // Clear alpha channel
		// Store float
		memcpy(out + precision, &b, precision);
		memcpy(out + precision * 2, &g, precision);
		memcpy(out + precision * 3, &r, precision);
	}
}


ConvertFromShader::ConvertFromShader(PClip _child, int _precision, IScriptEnvironment* env) :
GenericVideoFilter(_child) {
	if (!vi.IsRGB32())
		env->ThrowError("Source must be float-precision RGB");
	if (_precision != 0 && _precision != 1 && _precision != 2 && _precision != 4)
		env->ThrowError("Precision must be 0, 1, 2 or 4");

	// Convert from float-precision RGB to YV12
	viYV = vi;
	viYV.pixel_type = VideoInfo::CS_YV24;

	// Initialize variables.
	if (_precision == 0) {
		precision = 1;
		copyYUV = true;
	}
	else {
		precision = _precision;
		copyYUV = false;
	}

	// Avoid having to convert this for every pixel.
	// Float-precision RGB is 16-byte per pixel (4 float), YV12 is 2-byte per pixel
	if (precision == 1)
		precisionShift = 2;
	else if (precision == 2) {
		viYV.width >>= 1;
		precisionShift = 3;
	}
	else if (precision == 4) {
		viYV.width >>= 2;
		precisionShift = 4;
	}
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

void ConvertFromShader::convRGBto420(const byte *src, unsigned char *py, unsigned char *pu, unsigned char *pv,
	int pitch1, int pitch2Y, int pitch2UV, int width, int height)
{
	width >>= 1;
	height >>= 1;
	unsigned char U[4], V[4];
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			convFloat(src + (x << precisionShift), &py[x << 1], &U[0], &V[0]);
			convFloat(src + (x << precisionShift) + precision * 4, &py[(x << 1) + 1], &U[1], &V[1]);
			convFloat(src + (x << precisionShift) + pitch1, &py[(x << 1) + pitch2Y], &U[2], &V[2]);
			convFloat(src + (x << precisionShift) + pitch1 + precision * 4, &py[(x << 1) + pitch2Y + 1], &U[3], &V[3]);

			pu[x] = (U[0] + U[1] + U[2] + U[3]) / 4;
			pv[x] = (V[0] + V[1] + V[2] + V[3]) / 4;
		}
		src += pitch1 * 2;
		py += pitch2Y * 2;
		pu += pitch2UV;
		pv += pitch2UV;
	}
}

// Using Rec601 color space. Can be optimized with MMX assembly or by converting on the GPU with a shader.
void ConvertFromShader::convFloat(const byte* src, byte* outY, unsigned char* outU, unsigned char* outV) {
	float r, g, b;
	// Convert the data at the position of RGB into specified specifion
	if (precision == 0 || precision == 1) {
		unsigned char b2, g2, r2;
		memcpy(&b2, src, precision);
		memcpy(&g2, src + precision, precision);
		memcpy(&r2, src + precision * 2, precision);
		b = float(b2);
		g = float(g2);
		r = float(r2);
	}
	else if (precision == 2) {
		// Convert to half float
	}
	else {
		// Read float
		memcpy(&b, src + precision, precision);
		memcpy(&g, src + precision * 2, precision);
		memcpy(&r, src + precision * 3, precision);
	}

	// rgb are in the 0 to 1 range
	if (precision > 1) {
		r = r / 1 * 255;
		b = b / 1 * 255;
		g = g / 1 * 255;
	}

	float y, u, v;
	if (copyYUV) {
		y = r;
		u = g;
		v = b;
	}
	else {
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

	// Store YUV
	outY[0] = unsigned char(y);
	outU[0] = unsigned char(u);
	outV[0] = unsigned char(v);
}