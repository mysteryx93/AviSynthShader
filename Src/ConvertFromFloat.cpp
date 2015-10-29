#include "ConvertFromFloat.h"

ConvertFromFloat::ConvertFromFloat(PClip _child, const char* _format, bool _convertYuv, int _precision, IScriptEnvironment* env) :
	GenericVideoFilter(_child), precision(_precision), format(_format), convertYUV(_convertYuv) {
	if (!vi.IsRGB32())
		env->ThrowError("ConvertFromFloat: Source must be float-precision RGB");
	if (strcmp(format, "YV24") != 0 && strcmp(format, "YV12") != 0 && strcmp(format, "RGB32") != 0)
		env->ThrowError("ConvertFromFloat: Destination format must be YV24, YV12 or RGB32");
	if (precision != 1 && precision != 2)
		env->ThrowError("ConvertFromFloat: Precision must be 1 or 2");

	// Convert from float-precision RGB to YV24
	viYV = vi;
	if (strcmp(format, "RGB32") == 0)
		viYV.pixel_type = VideoInfo::CS_BGR32;
	else
		viYV.pixel_type = VideoInfo::CS_YV24;

	if (precision == 2) // Half-float frame has its width twice larger than normal
		viYV.width >>= 1;

	if (precision == 1)
		precisionShift = 2;
	else
		precisionShift = 4;

	if (precision == 2) {
		floatBufferPitch = vi.width / 2 * 4 * 4;
		floatBuffer = (unsigned char*)malloc(floatBufferPitch);
		halfFloatBufferPitch = vi.width / 2 * 4 * 2;
		halfFloatBuffer = (unsigned char*)malloc(halfFloatBufferPitch);
	}
}

ConvertFromFloat::~ConvertFromFloat() {
	if (precision == 2) {
		free(floatBuffer);
		free(halfFloatBuffer);
	}
}


PVideoFrame __stdcall ConvertFromFloat::GetFrame(int n, IScriptEnvironment* env) {
	PVideoFrame src = child->GetFrame(n, env);

	// Convert from float-precision RGB to YV24
	PVideoFrame dst = env->NewVideoFrame(viYV);
	if (viYV.pixel_type == VideoInfo::CS_BGR32)
		convFloatToRGB32(src->GetReadPtr(), dst->GetWritePtr(), src->GetPitch(), dst->GetPitch(), viYV.width, viYV.height, env);
	else
		convFloatToYV24(src->GetReadPtr(),
			dst->GetWritePtr(PLANAR_Y), dst->GetWritePtr(PLANAR_U), dst->GetWritePtr(PLANAR_V),
			src->GetPitch(), dst->GetPitch(PLANAR_Y), dst->GetPitch(PLANAR_U), viYV.width, viYV.height, env);
	return dst;
}

void ConvertFromFloat::convFloatToYV24(const byte *src, unsigned char *py, unsigned char *pu, unsigned char *pv,
	int pitch1, int pitch2Y, int pitch2UV, int width, int height, IScriptEnvironment* env)
{
	const byte* srcLoop = precision == 1 ? src : floatBuffer;
	int srcLoopPitch = precision == 1 ? pitch1 : floatBufferPitch;

	//if (precision == 2) {
	//	// Copy half-float data back into frame
	//	env->BitBlt(halfFloatBuffer, halfFloatBufferPitch, src, pitch1, width * 4 * 2, height);

	//	// Convert float buffer to half-float
	//	DirectX::PackedVector::XMConvertHalfToFloatStream((float*)floatBuffer, 4, (DirectX::PackedVector::HALF*)halfFloatBuffer, 2, width * 4 * height);
	//}

	unsigned char U, V;
	for (int y = 0; y < height; ++y) {
		if (precision == 2) {
			// Copy half-float data back into frame
			env->BitBlt(halfFloatBuffer, halfFloatBufferPitch, src, pitch1, width * 4 * 2, 1);

			// Convert float buffer to half-float
			DirectX::PackedVector::XMConvertHalfToFloatStream((float*)floatBuffer, 4, (DirectX::PackedVector::HALF*)halfFloatBuffer, 2, width * 4);
			src += pitch1;
		}

		for (int x = 0; x < width; ++x) {
			convFloat(srcLoop + (x << precisionShift), &py[x], &pu[x], &pv[x]);
		}
		if (precision == 1)
			srcLoop += srcLoopPitch;
		py += pitch2Y;
		pu += pitch2UV;
		pv += pitch2UV;
	}
}

void ConvertFromFloat::convFloatToRGB32(const byte *src, unsigned char *dst,
	int pitchSrc, int pitchDst, int width, int height, IScriptEnvironment* env) {

	const byte* srcLoop = precision == 1 ? src : floatBuffer;
	int srcLoopPitch = precision == 1 ? pitchSrc : floatBufferPitch;

	//if (precision == 2) {
	//	// Copy frame half-float data into buffer
	//	env->BitBlt(halfFloatBuffer, halfFloatBufferPitch, src, pitchSrc, width * 4 * 2, height);

	//	// Convert half-float buffer to float
	//	DirectX::PackedVector::XMConvertHalfToFloatStream((float*)floatBuffer, 4, (DirectX::PackedVector::HALF*)halfFloatBuffer, 2, width * 4 * height);
	//}

	dst += height * pitchDst;
	for (int y = 0; y < height; ++y) {
		if (precision == 2) {
			// Copy frame half-float data into buffer
			env->BitBlt(halfFloatBuffer, halfFloatBufferPitch, src, pitchSrc, width * 4 * 2, 1);

			// Convert half-float buffer to float
			DirectX::PackedVector::XMConvertHalfToFloatStream((float*)floatBuffer, 4, (DirectX::PackedVector::HALF*)halfFloatBuffer, 2, width * 4);
			src += pitchSrc;
		}

		dst -= pitchDst;
		for (int x = 0; x < width; ++x) {
			convFloat(srcLoop + (x << precisionShift), &dst[x * 4 + 2], &dst[x * 4 + 1], &dst[x * 4]);
		}
		if (precision == 1)
			srcLoop += srcLoopPitch;
	}
}

// Using Rec601 color space. Can be optimized with MMX assembly or by converting on the GPU with a shader.
// For ConvertToFloat, it's faster to process in INT, but for ConvertFromFloat, processing with FLOAT is faster
void ConvertFromFloat::convFloat(const byte* src, byte* outY, unsigned char* outU, unsigned char* outV) {
	if (precision == 1) {
		memcpy(&b1, src, precision);
		memcpy(&g1, src + precision, precision);
		memcpy(&r1, src + precision * 2, precision);
		b = float(b1);
		g = float(g1);
		r = float(r1);
	}
	else {
		memcpy(&r, src, 4);
		memcpy(&g, src + 4, 4);
		memcpy(&b, src + 8, 4);
		// rgb are in the 0 to 1 range
		r = r * 255;
		g = g * 255;
		b = b * 255;
	}

	if (convertYUV) {
		y2 = (0.257f * r) + (0.504f * g) + (0.098f * b) + 16;
		v2 = (0.439f * r) - (0.368f * g) - (0.071f * b) + 128;
		u2 = -(0.148f * r) - (0.291f * g) + (0.439f * b) + 128;
	}
	else {
		y2 = r;
		u2 = g;
		v2 = b;
	}

	y = short(y2 + 0.5f);
	u = short(u2 + 0.5f);
	v = short(v2 + 0.5f);

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

	//int r, g, b;

	//if (precision == 1) {
	//	unsigned char r1, g1, b1;
	//	memcpy(&b1, src, precision);
	//	memcpy(&g1, src + precision, precision);
	//	memcpy(&r1, src + precision * 2, precision);
	//	b = b1 * 1000000;
	//	g = g1 * 1000000;
	//	r = r1 * 1000000;
	//}
	//else {
	//	float r2, g2, b2;
	//	memcpy(&r2, src, 4);
	//	memcpy(&g2, src + 4, 4);
	//	memcpy(&b2, src + 8, 4);
	//	// rgb are in the 0 to 1 range
	//	r = int(r2 * 255000);
	//	g = int(g2 * 255000);
	//	b = int(b2 * 255000);
	//}

	//int y2, u2, v2;
	//short y, u, v;
	//if (convertYUV) {
	//	y2 = (257 * r) + (504 * g) + (98 * b) + 16 * 1000000;
	//	v2 = (439 * r) - (368 * g) - (71 * b) + 128 * 1000000;
	//	u2 = -(148 * r) - (291 * g) + (439 * b) + 128 * 1000000;
	//}
	//else {
	//	y2 = r * 1000;
	//	u2 = g * 1000;
	//	v2 = b * 1000;
	//}

	//y = (y2 + 500000) / 1000000;
	//u = (u2 + 500000) / 1000000;
	//v = (v2 + 500000) / 1000000;

	//if (y > 255) y = 255;
	//if (u > 255) u = 255;
	//if (v > 255) v = 255;
	//if (y < 0) y = 0;
	//if (u < 0) u = 0;
	//if (v < 0) v = 0;

	//// Store YUV
	//outY[0] = unsigned char(y);
	//outU[0] = unsigned char(u);
	//outV[0] = unsigned char(v);
}