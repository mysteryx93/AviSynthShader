#include "ConvertToShader.h"

ConvertToShader::ConvertToShader(PClip _child, int _precision, bool _stack16, IScriptEnvironment* env) :
	GenericVideoFilter(_child), precision(_precision), stack16(_stack16) {
	if (!vi.IsYV24() && !vi.IsRGB24() && !vi.IsRGB32())
		env->ThrowError("ConvertToShader: Source must be YV12, YV24, RGB24 or RGB32");
	if (precision < 1 && precision > 3)
		env->ThrowError("ConvertToShader: Precision must be 1, 2 or 3");
	if (stack16 && vi.IsRGB())
		env->ThrowError("ConvertToShader: Conversion from Stack16 only supports YV12 and YV24");
	if (stack16 && precision == 1)
		env->ThrowError("ConvertToShader: When using lsb, don't set precision=1!");

	viDst = vi;
	viDst.pixel_type = VideoInfo::CS_BGR32;

	if (precision > 1) // Half-float frame has its width twice larger than normal
		viDst.width <<= 1;
	if (stack16)
		viDst.height >>= 1;

	if (precision == 1)
		precisionShift = 2;
	else if (precision == 2)
		precisionShift = 3;
	else
		precisionShift = 4;

	if (precision == 3) {
		floatBufferPitch = vi.width << 4;
		halfFloatBufferPitch = vi.width << 3;
	}
}

ConvertToShader::~ConvertToShader() {
}

PVideoFrame __stdcall ConvertToShader::GetFrame(int n, IScriptEnvironment* env) {
	PVideoFrame src = child->GetFrame(n, env);

	// Convert from YV24 to half-float RGB
	PVideoFrame dst = env->NewVideoFrame(viDst);
	if (vi.IsRGB())
		convRgbToShader(src->GetReadPtr(), dst->GetWritePtr(), src->GetPitch(), dst->GetPitch(), vi.width, vi.height, env);
	//else if (precision == 2 && !convertYUV)
	//	bitblt_i8_to_i16_sse2((uint8_t*)src->GetReadPtr(PLANAR_Y), (uint8_t*)src->GetReadPtr(PLANAR_U), (uint8_t*)src->GetReadPtr(PLANAR_V), src->GetPitch(PLANAR_Y), (uint16_t*)dst->GetWritePtr(), dst->GetPitch(), srcHeight);
	else
		convYV24ToShader(src->GetReadPtr(PLANAR_Y), src->GetReadPtr(PLANAR_U), src->GetReadPtr(PLANAR_V), dst->GetWritePtr(), src->GetPitch(PLANAR_Y), src->GetPitch(PLANAR_U), dst->GetPitch(), vi.width, vi.height, env);

	return dst;
}

void ConvertToShader::convYV24ToShader(const byte *py, const byte *pu, const byte *pv,
	unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height, IScriptEnvironment* env)
{
	unsigned char *floatBuffer;
	unsigned char *halfFloatBuffer;

	if (precision == 3) {
		floatBuffer = new unsigned char[floatBufferPitch]; // C++ 
		halfFloatBuffer = new unsigned char[halfFloatBufferPitch];
	}

	unsigned char* dstLoop = precision == 3 ? floatBuffer : dst;
	int dstLoopPitch = precision == 3 ? floatBufferPitch : pitch2;

	if (stack16)
		height >>= 1;

	// Convert all data to float
	unsigned char Y, U, V, Y2, U2, V2;

	for (int y = 0; y < height; ++y) {
		//if (precision == 1) {
		//	for (int x = 0; x < width; ++x) {
		//		Y = py[x];
		//		U = pu[x];
		//		V = pv[x];
		//		convInt(Y, U, V, dstLoop + (x * 3));
		//	}
		//}
		if (!stack16) {
			for (int x = 0; x < width; ++x) {
				Y = py[x];
				U = pu[x];
				V = pv[x];
				convInt(Y, U, V, dstLoop + (x << precisionShift));
			}
		}
		else { // Stack16
			for (int x = 0; x < width; ++x) {
				Y = py[x];
				U = pu[x];
				V = pv[x];

				Y2 = Y + pitch1Y * height;
				U2 = U + pitch1UV * height;
				V2 = V + pitch1UV * height;
				convStack16(Y, U, V, Y2, U2, V2, dstLoop + (x << precisionShift));
			}
		}

		py += pitch1Y;
		pu += pitch1UV;
		pv += pitch1UV;
		if (precision == 3) {
			// Convert float buffer to half-float
			DirectX::PackedVector::XMConvertFloatToHalfStream((DirectX::PackedVector::HALF*)halfFloatBuffer, 2, (float*)floatBuffer, 4, width << 2);

			// Copy half-float data back into frame
			env->BitBlt(dst, pitch2, halfFloatBuffer, halfFloatBufferPitch, halfFloatBufferPitch, 1);
			dst += pitch2;
		}
		else
			dstLoop += dstLoopPitch;
	}
	if (precision == 3) {
		delete[] floatBuffer;
		delete[] halfFloatBuffer;
	}
}

void ConvertToShader::convRgbToShader(const byte *src, unsigned char *dst, int srcPitch, int dstPitch, int width, int height, IScriptEnvironment* env) {
	unsigned char *floatBuffer;
	unsigned char *halfFloatBuffer;
	if (precision == 3) {
		floatBuffer = new unsigned char[floatBufferPitch];
		halfFloatBuffer = new unsigned char[halfFloatBufferPitch];
	}

	unsigned char* dstLoop = precision == 3 ? floatBuffer : dst;
	int dstLoopPitch = precision == 3 ? floatBufferPitch : dstPitch;

	if (stack16)
		height >>= 1;

	const byte *Val, *Val2;
	unsigned char B, G, R, B2, G2, R2;
	bool IsSrcRgb32 = vi.IsRGB32();
	src += height * srcPitch;
	for (int y = 0; y < height; ++y) {
		src -= srcPitch;

		//if (precision == 1) {
		//	if (IsSrcRgb32) {
		//		for (int x = 0; x < width; ++x) {
		//			Val = &src[x << 2];
		//			B = Val[0];
		//			G = Val[1];
		//			R = Val[2];
		//			convInt(R, G, B, dstLoop + (x * 3));
		//		}
		//	}
		//	else {
		//		for (int x = 0; x < width; ++x) {
		//			Val = &src[x * 3];
		//			B = Val[0];
		//			G = Val[1];
		//			R = Val[2];
		//			convInt(R, G, B, dstLoop + (x * 3));
		//		}
		//	}
		//}
		if (!stack16) {
			if (IsSrcRgb32) {
				for (int x = 0; x < width; ++x) {
					Val = &src[x << 2];
					B = Val[0];
					G = Val[1];
					R = Val[2];
					convInt(R, G, B, dstLoop + (x << precisionShift));
				}
			}
			else {
				for (int x = 0; x < width; ++x) {
					Val = &src[x * 3];
					B = Val[0];
					G = Val[1];
					R = Val[2];
					convInt(R, G, B, dstLoop + (x << precisionShift));
				}
			}
		}
		else { // stack16
			if (IsSrcRgb32) {
				for (int x = 0; x < width; ++x) {
					Val = &src[x << 2];
					B = Val[0];
					G = Val[1];
					R = Val[2];
					Val2 = Val + srcPitch * height;
					B2 = Val2[0];
					G2 = Val2[1];
					R2 = Val2[2];
					convStack16(R, G, B, R2, G2, B2, dstLoop + (x << precisionShift));
				}
			}
			else {
				for (int x = 0; x < width; ++x) {
					Val = &src[x * 3];
					B = Val[0];
					G = Val[1];
					R = Val[2];
					Val2 = Val + srcPitch * height;
					B2 = Val2[0];
					G2 = Val2[1];
					R2 = Val2[2];
					convStack16(R, G, B, R2, G2, B2, dstLoop + (x << precisionShift));
				}
			}
		}

		if (precision == 3) {
			// Convert float buffer to half-float
			DirectX::PackedVector::XMConvertFloatToHalfStream((DirectX::PackedVector::HALF*)halfFloatBuffer, 2, (float*)floatBuffer, 4, width << 2);

			// Copy half-float data back into frame
			env->BitBlt(dst, dstPitch, halfFloatBuffer, halfFloatBufferPitch, halfFloatBufferPitch, 1);
			dst += dstPitch;
		}
		else
			dstLoop += dstLoopPitch;
	}
	if (precision == 3) {
		delete[] floatBuffer;
		delete[] halfFloatBuffer;
	}
}

void ConvertToShader::convInt(uint8_t y, uint8_t u, uint8_t v, uint8_t* out) {
	if (precision == 1) {
		out[0] = v;
		out[1] = u;
		out[2] = y;
	}
	else if (precision == 2) {
		uint16_t *outS = (unsigned short *)out;
		outS[0] = (uint16_t)y << 8;
		outS[1] = (uint16_t)u << 8;
		outS[2] = (uint16_t)v << 8;
	}
	else { // precision == 3
		// Half-float, texture shaders expect data between 0 and 1
		float rf, gf, bf;
		rf = float(y) / 255;
		gf = float(u) / 255;
		bf = float(v) / 255;

		float *outF = (float*)out;
		outF[0] = rf;
		outF[1] = gf;
		outF[2] = bf;
	}
}

void ConvertToShader::convStack16(uint8_t y, uint8_t u, uint8_t v, uint8_t y2, uint8_t u2, uint8_t v2, uint8_t* out) {
	if (precision == 1) {
		out[2] = y == 255 || y2 < 128 ? y : y + 1;
		out[1] = u == 255 || u2 < 128 ? u : u + 1;
		out[0] = v == 255 || v2 < 128 ? v : v + 1;
	}
	else {
		// Restore 16-bit values
		uint8_t Buffer[6];
		Buffer[0] = y2;
		Buffer[1] = y;
		Buffer[2] = u2;
		Buffer[3] = u;
		Buffer[4] = v2;
		Buffer[5] = v;
		uint16_t* pBuffer = (uint16_t*)Buffer;

		if (precision == 2) {
			uint16_t *pOut = (uint16_t*)out;
			pOut[0] = pBuffer[0];
			pOut[1] = pBuffer[1];
			pOut[2] = pBuffer[2];
		}
		else { // precision == 3
			// Half-float, texture shaders expect data between 0 and 1
			float rf, gf, bf;
			rf = float(pBuffer[0]) / 65535;
			gf = float(pBuffer[1]) / 65535;
			bf = float(pBuffer[2]) / 65535;

			float *outF = (float*)out;
			outF[0] = rf;
			outF[1] = gf;
			outF[2] = bf;
		}
	}
}

// restrictions: src_stride MUST BE a multiple of 8, dst_stride MUST BE a multiple of 64 and 8x src_stride (x4 planes, x2 pixel size)
//void ConvertToShader::bitblt_i8_to_i16_sse2(const uint8_t* srcY, const uint8_t* srcU, const uint8_t* srcV, int srcPitch, uint16_t* dst, int dstPitch, int height)
//{
//	assert(srcPitch % 2 == 0);
//	assert(dstPitch % 16 == 0);
//	assert(dstPitch / srcPitch == 8);
//
//	const __m128i  zero = _mm_setzero_si128();
//	const __m128i  val_ma = _mm_set1_epi16(0);
//	const __m128i  mask_lsb = _mm_set1_epi16(0x00FF);
//
//	uint8_t        convBuffer[8];
//	convBuffer[3] = 255;
//	convBuffer[7] = 255;
//
//	for (int y = 0; y < height; ++y) {
//		for (int x = 0; x < srcPitch; x += 2) {
//			// 1. Merge 2 pixels from 3 planes into buffer.
//			convBuffer[0] = srcY[0];
//			convBuffer[1] = srcU[0];
//			convBuffer[2] = srcV[0];
//			convBuffer[4] = srcY[1];
//			convBuffer[5] = srcU[1];
//			convBuffer[6] = srcV[1];
//
//			// 2. Convert 8 bytes from UINT8 buffer into UINT16 dest
//			__m128i val = load_8_16l(convBuffer, zero);
//			val = _mm_slli_epi16(val, 8);
//			store_8_16l(dst, val, mask_lsb);
//
//			// 3. Move to the next stride
//			srcY += 2;
//			srcU += 2;
//			srcV += 2;
//			dst += 8; // dst is uint16_t so this moves 16 bytes
//		}
//	}
//}
//
//__m128i	ConvertToShader::load_8_16l(const void *lsb_ptr, __m128i zero)
//{
//	assert(lsb_ptr != 0);
//
//	const __m128i  val_lsb = _mm_loadl_epi64(
//		reinterpret_cast <const __m128i *> (lsb_ptr)
//	);
//	const __m128i  val = _mm_unpacklo_epi8(val_lsb, zero);
//
//	return (val);
//}
//
//void ConvertToShader::store_8_16l(void *lsb_ptr, __m128i val, __m128i mask_lsb)
//{
//	assert(lsb_ptr != 0);
//
//	__m128i        lsb = _mm_and_si128(mask_lsb, val);
//	lsb = _mm_packus_epi16(lsb, lsb);
//	_mm_storel_epi64(reinterpret_cast <__m128i *> (lsb_ptr), lsb);
//}