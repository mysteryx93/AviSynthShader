#include "ConvertFromShader.h"

ConvertFromShader::ConvertFromShader(PClip _child, int _precision, const char* _format, bool _stack16, IScriptEnvironment* env) :
	GenericVideoFilter(_child), precision(_precision), format(_format), stack16(_stack16) {
	if (!vi.IsRGB32())
		env->ThrowError("ConvertFromShader: Source must be float-precision RGB");
	if (strcmp(format, "YV12") != 0 && strcmp(format, "YV24") != 0 && strcmp(format, "RGB24") != 0 && strcmp(format, "RGB32") != 0)
		env->ThrowError("ConvertFromShader: Destination format must be YV12, YV24, RGB24 or RGB32");
	if (precision < 1 || precision > 3)
		env->ThrowError("ConvertFromShader: Precision must be 1, 2 or 3");
	if (stack16 && strcmp(format, "YV12") != 0 && strcmp(format, "YV24") != 0)
		env->ThrowError("Conversion to Stack16 only supports YV12 and YV24");

	viDst = vi;
	if (strcmp(format, "RGB32") == 0)
		viDst.pixel_type = VideoInfo::CS_BGR32;
	else if (strcmp(format, "RGB24") == 0)
		viDst.pixel_type = VideoInfo::CS_BGR24;
	else
		viDst.pixel_type = VideoInfo::CS_YV24;

	if (stack16)		// Stack16 frame has twice the height
		viDst.height <<= 1;
	if (precision > 1)	// UINT16 frame has twice the width
		viDst.width >>= 1;

	if (precision == 1)
		precisionShift = 2;
	else if (precision == 2)
		precisionShift = 3;
	else
		precisionShift = 4;

	if (precision == 3) {
		floatBufferPitch = vi.width << 3;
		halfFloatBufferPitch = vi.width << 2;
	}
}

ConvertFromShader::~ConvertFromShader() {
}


PVideoFrame __stdcall ConvertFromShader::GetFrame(int n, IScriptEnvironment* env) {
	PVideoFrame src = child->GetFrame(n, env);

	// Convert from float-precision RGB to YV24
	PVideoFrame dst = env->NewVideoFrame(viDst);
	if (viDst.IsRGB())
		convFloatToRGB32(src->GetReadPtr(), dst->GetWritePtr(), src->GetPitch(), dst->GetPitch(), viDst.width, viDst.height, env);
	else
		convFloatToYV24(src->GetReadPtr(),
			dst->GetWritePtr(PLANAR_Y), dst->GetWritePtr(PLANAR_U), dst->GetWritePtr(PLANAR_V),
			src->GetPitch(), dst->GetPitch(PLANAR_Y), dst->GetPitch(PLANAR_U), viDst.width, viDst.height, env);
	return dst;
}

void ConvertFromShader::convFloatToYV24(const byte *src, unsigned char *py, unsigned char *pu, unsigned char *pv,
	int pitch1, int pitch2Y, int pitch2UV, int width, int height, IScriptEnvironment* env)
{
	unsigned char *floatBuffer;
	unsigned char *halfFloatBuffer;

	if (precision == 3) {
		floatBuffer = new unsigned char[floatBufferPitch]; // local 
		halfFloatBuffer = new unsigned char[halfFloatBufferPitch];
	}

	const byte* srcLoop = precision == 3 ? floatBuffer : src;
	int srcLoopPitch = precision == 3 ? floatBufferPitch : pitch1;

	if (stack16)
		height >>= 1;

	for (int y = 0; y < height; ++y) {
		if (precision == 3) {
			// Copy half-float data back into frame
			env->BitBlt(halfFloatBuffer, halfFloatBufferPitch, src, pitch1, width << 3, 1);

			// Convert float buffer to half-float
			DirectX::PackedVector::XMConvertHalfToFloatStream((float*)floatBuffer, 4, (DirectX::PackedVector::HALF*)halfFloatBuffer, 2, width << 2);
			src += pitch1;
		}

		if (stack16) {
			for (int x = 0; x < width; ++x) {
				convStack16(srcLoop + (x << precisionShift), &py[x], &pu[x], &pv[x], &py[x + pitch2Y + height], &pu[x + pitch2UV + height], &pv[x + pitch2UV + height]);
			}
		}
		else {
			for (int x = 0; x < width; ++x) {
				convInt(srcLoop + (x << precisionShift), &py[x], &pu[x], &pv[x]);
			}
		}

		if (precision < 3)
			srcLoop += srcLoopPitch;
		py += pitch2Y;
		pu += pitch2UV;
		pv += pitch2UV;
	}

	if (precision == 3) {
		delete[] floatBuffer;
		delete[] halfFloatBuffer;
	}
}

void ConvertFromShader::convFloatToRGB32(const byte *src, unsigned char *dst,
	int pitchSrc, int pitchDst, int width, int height, IScriptEnvironment* env) {

	unsigned char *floatBuffer;
	unsigned char *halfFloatBuffer;

	if (precision == 3) {
		floatBuffer = new unsigned char[floatBufferPitch]; // local 
		halfFloatBuffer = new unsigned char[halfFloatBufferPitch];
	}

	const byte* srcLoop = precision == 3 ? floatBuffer : src;
	int srcLoopPitch = precision == 3 ? floatBufferPitch : pitchSrc;

	if (stack16)
		height >>= 1;

	dst += height * pitchDst;
	unsigned char *Val, *Val2;

	for (int y = 0; y < height; ++y) {
		if (precision == 3) {
			// Copy frame half-float data into buffer
			env->BitBlt(halfFloatBuffer, halfFloatBufferPitch, src, pitchSrc, width << 3, 1);

			// Convert half-float buffer to float
			DirectX::PackedVector::XMConvertHalfToFloatStream((float*)floatBuffer, 4, (DirectX::PackedVector::HALF*)halfFloatBuffer, 2, width << 2);
			src += pitchSrc;
		}

		dst -= pitchDst;

		if (viDst.IsRGB32() && stack16) {
			for (int x = 0; x < width; ++x) {
				Val = &dst[x << 2];
				Val2 = Val + pitchDst * height;
				convStack16(srcLoop + (x << precisionShift), &Val[2], &Val[1], &Val[0], &Val2[2], &Val2[1], &Val2[0]);
			}
		}
		else if (viDst.IsRGB32() && !stack16) {
			for (int x = 0; x < width; ++x) {
				Val = &dst[x << 2];
				convInt(srcLoop + (x << precisionShift), &Val[2], &Val[1], &Val[0]);
			}
		}
		else if (viDst.IsRGB32() && stack16) {
			for (int x = 0; x < width; ++x) {
				Val = &dst[x * 3];
				Val2 = Val + pitchDst * height;
				convStack16(srcLoop + (x << precisionShift), &Val[2], &Val[1], &Val[0], &Val2[2], &Val2[1], &Val2[0]);
			}
		}
		else if (viDst.IsRGB32() && !stack16) {
			for (int x = 0; x < width; ++x) {
				Val = &dst[x * 3];
				convInt(srcLoop + (x << precisionShift), &Val[2], &Val[1], &Val[0]);
			}
		}

		if (precision < 3)
			srcLoop += srcLoopPitch;
	}
	if (precision == 3) {
		delete[] floatBuffer;
		delete[] halfFloatBuffer;
	}
}

#define clamp(n, lower, upper) max(lower, min(n, upper))

void ConvertFromShader::convInt(const byte* src, unsigned char* outY, unsigned char* outU, unsigned char* outV) {
	switch (precision) {
	case 1: {
		outV[0] = src[0];
		outU[0] = src[1];
		outY[0] = src[2];
		break;
	} case 2: {
		const uint16_t* pOut = (uint16_t*)src;
		// Conversion to UINT16 gave values between 0 and 255*256=65280. Add 128 to avoid darkening.
		outY[0] = sadd16(pOut[0], 128) >> 8;
		outU[0] = sadd16(pOut[1], 128) >> 8;
		outV[0] = sadd16(pOut[2], 128) >> 8;
		break;
	} case 3: {
		// colors are in the 0 to 1 range
		float* pIn = (float*)src;
		uint16_t y[3];
		for (int i = 0; i < 3; i++) {
			y[i] = sadd16((uint16_t)(clamp(pIn[i], 0.0f, 1.0f) * 65535), 128) / 256;
		}

		// Store YUV
		const uint8_t* pOut3 = (uint8_t*)y;
		outY[0] = pOut3[0];
		outU[0] = pOut3[2];
		outV[0] = pOut3[4];
		break; }
	}
}

void ConvertFromShader::convStack16(const byte* src, unsigned char* outY, unsigned char* outU, unsigned char* outV, unsigned char* outY2, unsigned char* outU2, unsigned char* outV2) {
	const uint8_t* pOut;
	float* pIn;
	switch (precision) {
	case 1: {
		outV[0] = src[0];
		outU[0] = src[1];
		outY[0] = src[2];
		outV2[0] = 0;
		outU2[0] = 0;
		outV2[0] = 0;
		return;
	} case 2: {
		pOut = (uint8_t*)src;
		break;
	} case 3: {
		// colors are in the 0 to 1 range
		pIn = (float*)src;
		uint16_t y[3];
		for (int i = 0; i < 3; i++) {
			y[i] = sadd16((uint16_t)(clamp(pIn[i], 0.0f, 1.0f) * 65535), 128);
		}

		// Store YUV
		pOut = (uint8_t*)y;
		break;
	} default:
		return;
		break;
	}
	outY[0] = pOut[1];
	outY2[0] = pOut[0];
	outU[0] = pOut[3];
	outU2[0] = pOut[2];
	outV[0] = pOut[5];
	outV2[0] = pOut[4];
}


inline uint16_t ConvertFromShader::sadd16(uint16_t a, uint16_t b)
{
	return (a > 0xFFFF - b) ? 0xFFFF : a + b;
}