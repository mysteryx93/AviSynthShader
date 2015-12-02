#include "ConvertToShader.h"

ConvertToShader::ConvertToShader(PClip _child, int _precision, IScriptEnvironment* env) :
	GenericVideoFilter(_child), precision(_precision) {
	if (!vi.IsYV24() && !vi.IsRGB24() && !vi.IsRGB32())
		env->ThrowError("ConvertToShader: Source must be YV12, YV24, RGB24 or RGB32");
	if (precision < 1 && precision > 3)
		env->ThrowError("ConvertToShader: Precision must be 1, 2 or 3");

	viDst = vi;
	viDst.pixel_type = VideoInfo::CS_BGR32;
	if (precision > 1) // Half-float frame has its width twice larger than normal
		viDst.width <<= 1;

	if (precision == 1)
		precisionShift = 2;
	else if (precision == 2)
		precisionShift = 3;
	else
		precisionShift = 4;

	if (precision == 3) {
		floatBufferPitch = vi.width << 4;
		floatBuffer = (unsigned char*)malloc(floatBufferPitch);
		halfFloatBufferPitch = vi.width << 3;
		halfFloatBuffer = (unsigned char*)malloc(halfFloatBufferPitch);
	}
}

ConvertToShader::~ConvertToShader() {
	if (precision == 3) {
		free(floatBuffer);
		free(halfFloatBuffer);
	}
}

PVideoFrame __stdcall ConvertToShader::GetFrame(int n, IScriptEnvironment* env) {
	PVideoFrame src = child->GetFrame(n, env);

	// Convert from YV24 to half-float RGB
	PVideoFrame dst = env->NewVideoFrame(viDst);
	if (vi.IsRGB())
		convRgbToFloat(src->GetReadPtr(), dst->GetWritePtr(), src->GetPitch(), dst->GetPitch(), vi.width, vi.height, env);
	//else if (precision == 2 && !convertYUV)
	//	bitblt_i8_to_i16_sse2((uint8_t*)src->GetReadPtr(PLANAR_Y), (uint8_t*)src->GetReadPtr(PLANAR_U), (uint8_t*)src->GetReadPtr(PLANAR_V), src->GetPitch(PLANAR_Y), (uint16_t*)dst->GetWritePtr(), dst->GetPitch(), srcHeight);
	else
		convYV24ToFloat(src->GetReadPtr(PLANAR_Y), src->GetReadPtr(PLANAR_U), src->GetReadPtr(PLANAR_V), dst->GetWritePtr(), src->GetPitch(PLANAR_Y), src->GetPitch(PLANAR_U), dst->GetPitch(), vi.width, vi.height, env);

	return dst;
}

void ConvertToShader::convYV24ToFloat(const byte *py, const byte *pu, const byte *pv,
	unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height, IScriptEnvironment* env)
{
	unsigned char* dstLoop = precision == 3 ? floatBuffer : dst;
	int dstLoopPitch = precision == 3 ? floatBufferPitch : pitch2;

	// Convert all data to float
	unsigned char Y, U, V;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			Y = py[x];
			U = pu[x];
			V = pv[x];

			if (!convertYUV && precision < 3)
				convInt(Y, U, V, dstLoop + (x << precisionShift));
			else
				convFloat(Y, U, V, dstLoop + (x << precisionShift));
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
}

void ConvertToShader::convRgbToFloat(const byte *src, unsigned char *dst, int srcPitch, int dstPitch, int width, int height, IScriptEnvironment* env) {
	unsigned char* dstLoop = precision == 3 ? floatBuffer : dst;
	int dstLoopPitch = precision == 3 ? floatBufferPitch : dstPitch;

	const byte* Val;
	unsigned char B, G, R;
	src += height * srcPitch;
	for (int y = 0; y < height; ++y) {
		src -= srcPitch;
		for (int x = 0; x < width; ++x) {
			Val = vi.IsRGB32() ? &src[x << 2] : &src[x * 3];
			B = Val[0];
			G = Val[1];
			R = Val[2];

			if (!convertYUV && precision < 3)
				convInt(R, G, B, dstLoop + (x << precisionShift));
			else
				convFloat(R, G, B, dstLoop + (x << precisionShift));
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
}

// Using Rec601 color space. Can be optimized with MMX assembly or by converting on the GPU with a shader.
void ConvertToShader::convFloat(unsigned char y, unsigned char u, unsigned char v, unsigned char* out) {
	int r, g, b;
	if (convertYUV) {
		b = 1164 * (y - 16) + 2018 * (u - 128);
		g = 1164 * (y - 16) - 813 * (v - 128) - 391 * (u - 128);
		r = 1164 * (y - 16) + 1596 * (v - 128);
	}

	if (precision == 1) {
		unsigned char r2, g2, b2;
		if (convertYUV) {
			if (r > 255000) r = 255000;
			if (g > 255000) g = 255000;
			if (b > 255000) b = 255000;
			if (r < 0) r = 0;
			if (g < 0) g = 0;
			if (b < 0) b = 0;
			r2 = unsigned char(r / 1000);
			g2 = unsigned char(g / 1000);
			b2 = unsigned char(b / 1000);
		}
		else {
			r2 = y;
			g2 = u;
			b2 = v;
		}

		memcpy(out + 0, &b2, 1);
		memcpy(out + 1, &g2, 1);
		memcpy(out + 2, &r2, 1);
		// out[precision * 3] = 0;
	}
	else if (precision == 2) {
		// UINT16, texture shaders expect data between 0 and UINT16_MAX
		unsigned short rs, gs, bs;
		if (convertYUV) {
			if (r > 255000) r = 255000;
			if (g > 255000) g = 255000;
			if (b > 255000) b = 255000;
			if (r < 0) r = 0;
			if (g < 0) g = 0;
			if (b < 0) b = 0;
			rs = unsigned short(r / (1000 << 8));
			gs = unsigned short(g / (1000 << 8));
			bs = unsigned short(b / (1000 << 8));
		}
		else {
			rs = unsigned short(y << 8);
			gs = unsigned short(u << 8);
			bs = unsigned short(v << 8);
		}

		memcpy(out, &rs, 2);
		memcpy(out + 2, &gs, 2);
		memcpy(out + 4, &bs, 2);
		//memcpy(out + 6, &AlphaShort, 2);
	}
	else {
		// Half-float, texture shaders expect data between 0 and 1
		float rf, gf, bf;
		if (convertYUV) {
			rf = float(r) / 255000;
			gf = float(g) / 255000;
			bf = float(b) / 255000;
		}
		else {
			rf = float(y) / 255;
			gf = float(u) / 255;
			bf = float(v) / 255;
		}

		memcpy(out, &rf, 4);
		memcpy(out + 4, &gf, 4);
		memcpy(out + 8, &bf, 4);
		//memcpy(out + 12, &AlphaFloat, 4);
	}
}

// Shortcut to process BYTE or UINT16 values faster when not converting colors
void ConvertToShader::convInt(unsigned char y, unsigned char u, unsigned char v, unsigned char* out) {
	if (precision == 1) {
		out[0] = v;
		out[1] = u;
		out[2] = y;
	}
	else { // Precision == 2
		unsigned short *outS = (unsigned short *)out;
		outS[0] = (unsigned short)y << 8;
		outS[1] = (unsigned short)u << 8;
		outS[2] = (unsigned short)v << 8;
	}
}

// restrictions: src_stride MUST BE a multiple of 8, dst_stride MUST BE a multiple of 64 and 8x src_stride (x4 planes, x2 pixel size)
void ConvertToShader::bitblt_i8_to_i16_sse2(const uint8_t* srcY, const uint8_t* srcU, const uint8_t* srcV, int srcPitch, uint16_t* dst, int dstPitch, int height)
{
	assert(srcPitch % 2 == 0);
	assert(dstPitch % 16 == 0);
	assert(dstPitch / srcPitch == 8);

	const __m128i  zero = _mm_setzero_si128();
	const __m128i  val_ma = _mm_set1_epi16(0);
	const __m128i  mask_lsb = _mm_set1_epi16(0x00FF);

	uint8_t        convBuffer[8];
	convBuffer[3] = 255;
	convBuffer[7] = 255;

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < srcPitch; x += 2) {
			// 1. Merge 2 pixels from 3 planes into buffer.
			convBuffer[0] = srcY[0];
			convBuffer[1] = srcU[0];
			convBuffer[2] = srcV[0];
			convBuffer[4] = srcY[1];
			convBuffer[5] = srcU[1];
			convBuffer[6] = srcV[1];

			// 2. Convert 8 bytes from UINT8 buffer into UINT16 dest
			__m128i val = load_8_16l(convBuffer, zero);
			val = _mm_slli_epi16(val, 8);
			store_8_16l(dst, val, mask_lsb);

			// 3. Move to the next stride
			srcY += 2;
			srcU += 2;
			srcV += 2;
			dst += 8; // dst is uint16_t so this moves 16 bytes
		}
	}
}

__m128i	ConvertToShader::load_8_16l(const void *lsb_ptr, __m128i zero)
{
	assert(lsb_ptr != 0);

	const __m128i  val_lsb = _mm_loadl_epi64(
		reinterpret_cast <const __m128i *> (lsb_ptr)
		);
	const __m128i  val = _mm_unpacklo_epi8(val_lsb, zero);

	return (val);
}

void ConvertToShader::store_8_16l(void *lsb_ptr, __m128i val, __m128i mask_lsb)
{
	assert(lsb_ptr != 0);

	__m128i        lsb = _mm_and_si128(mask_lsb, val);
	lsb = _mm_packus_epi16(lsb, lsb);
	_mm_storel_epi64(reinterpret_cast <__m128i *> (lsb_ptr), lsb);
}