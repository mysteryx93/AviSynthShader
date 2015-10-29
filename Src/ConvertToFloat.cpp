#include "ConvertToFloat.h"

ConvertToFloat::ConvertToFloat(PClip _child, bool _convertYuv, int _precision, IScriptEnvironment* env) :
	GenericVideoFilter(_child), precision(_precision), convertYUV(_convertYuv) {
	if (!vi.IsYV24() && !vi.IsRGB32())
		env->ThrowError("ConvertToFloat: Source must be YV12, YV24 or RGB32");
	if (precision != 1 && precision != 2)
		env->ThrowError("ConvertToFloat: Precision must be 1 or 2");

	// Keep original clip information
	srcWidth = vi.width;
	srcHeight = vi.height;
	srcRgb = vi.IsRGB32();

	vi.pixel_type = VideoInfo::CS_BGR32;
	if (precision == 2) // Half-float frame has its width twice larger than normal
		vi.width <<= 1;
	
	if (precision == 1)
		precisionShift = 2;
	else
		precisionShift = 4;

	if (precision == 2) {
		floatBufferPitch = srcWidth * 4 * 4;
		floatBuffer = (unsigned char*)malloc(floatBufferPitch);
		halfFloatBufferPitch = srcWidth * 4 * 2;
		halfFloatBuffer = (unsigned char*)malloc(halfFloatBufferPitch);
	}
}

ConvertToFloat::~ConvertToFloat() {
	if (precision == 2) {
		free(floatBuffer);
		free(halfFloatBuffer);
	}
}

PVideoFrame __stdcall ConvertToFloat::GetFrame(int n, IScriptEnvironment* env) {
	PVideoFrame src = child->GetFrame(n, env);

	// Convert from YV24 to half-float RGB
	PVideoFrame dst = env->NewVideoFrame(vi);
	if (srcRgb)
		convRgbToFloat(src->GetReadPtr(), dst->GetWritePtr(), src->GetPitch(), dst->GetPitch(), srcWidth, srcHeight, env);
	else
		convYV24ToFloat(src->GetReadPtr(PLANAR_Y), src->GetReadPtr(PLANAR_U), src->GetReadPtr(PLANAR_V), dst->GetWritePtr(), src->GetPitch(PLANAR_Y), src->GetPitch(PLANAR_U), dst->GetPitch(), srcWidth, srcHeight, env);

	return dst;
}

void ConvertToFloat::convYV24ToFloat(const byte *py, const byte *pu, const byte *pv,
	unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height, IScriptEnvironment* env)
{
	unsigned char* dstLoop = precision == 1 ? dst : floatBuffer;
	int dstLoopPitch = precision == 1 ? pitch2 : floatBufferPitch;

	// Convert all data to float
	byte Y, U, V;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			Y = py[x];
			U = pu[x];
			V = pv[x];

			convFloat(Y, U, V, dstLoop + (x << precisionShift));
		}
		py += pitch1Y;
		pu += pitch1UV;
		pv += pitch1UV;
		if (precision == 1)
			dstLoop += dstLoopPitch;
		else {
			// Convert float buffer to half-float
			DirectX::PackedVector::XMConvertFloatToHalfStream((DirectX::PackedVector::HALF*)halfFloatBuffer, 2, (float*)floatBuffer, 4, width * 4);

			// Copy half-float data back into frame
			env->BitBlt(dst, pitch2, halfFloatBuffer, halfFloatBufferPitch, halfFloatBufferPitch, 1);
			dst += pitch2;
		}
	}
}

void ConvertToFloat::convRgbToFloat(const byte *src, unsigned char *dst, int srcPitch, int dstPitch, int width, int height, IScriptEnvironment* env) {
	unsigned char* dstLoop = precision == 1 ? dst : floatBuffer;
	int dstLoopPitch = precision == 1 ? dstPitch : floatBufferPitch;

	byte B, G, R;
	src += height * srcPitch;
	for (int y = 0; y < height; ++y) {
		src -= srcPitch;
		for (int x = 0; x < width; ++x) {
			B = src[x * 4];
			G = src[x * 4 + 1];
			R = src[x * 4 + 2];

			convFloat(R, G, B, dstLoop + (x << precisionShift));
		}
		if (precision == 1)
			dstLoop += dstLoopPitch;
		else {
			// Convert float buffer to half-float
			DirectX::PackedVector::XMConvertFloatToHalfStream((DirectX::PackedVector::HALF*)halfFloatBuffer, 2, (float*)floatBuffer, 4, width * 4);

			// Copy half-float data back into frame
			env->BitBlt(dst, dstPitch, halfFloatBuffer, halfFloatBufferPitch, halfFloatBufferPitch, 1);
			dst += dstPitch;
		}
	}
}

// Using Rec601 color space. Can be optimized with MMX assembly or by converting on the GPU with a shader.
void ConvertToFloat::convFloat(byte y, byte u, byte v, unsigned char* out) {
	if (convertYUV) {
		b = 1164 * (y - 16) + 2018 * (u - 128);
		g = 1164 * (y - 16) - 813 * (v - 128) - 391 * (u - 128);
		r = 1164 * (y - 16) + 1596 * (v - 128);
	}
	else {
		// Pass YUV values to be converted by a shader
		r = y * 1000;
		g = u * 1000;
		b = v * 1000;
	}

	if (precision == 1) {
		if (convertYUV) {
			if (r > 255000) r = 255000;
			if (g > 255000) g = 255000;
			if (b > 255000) b = 255000;
			if (r < 0) r = 0;
			if (g < 0) g = 0;
			if (b < 0) b = 0;
		}

		r2 = unsigned char(r / 1000);
		g2 = unsigned char(g / 1000);
		b2 = unsigned char(b / 1000);
		memcpy(out + 0, &b2, precision);
		memcpy(out + precision, &g2, precision);
		memcpy(out + precision * 2, &r2, precision);
		out[precision * 3] = 0;
	}
	else {
		// Texture shaders expect data between 0 and 1
		rf = float(r) / 255000;
		gf = float(g) / 255000;
		bf = float(b) / 255000;

		memcpy(out, &rf, 4);
		memcpy(out + 4, &gf, 4);
		memcpy(out + 8, &bf, 4);
		memcpy(out + 12, &AlphaValue, 4);
	}
}