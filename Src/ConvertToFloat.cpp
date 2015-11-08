#include "ConvertToFloat.h"

ConvertToFloat::ConvertToFloat(PClip _child, bool _convertYuv, int _precision, IScriptEnvironment* env) :
	GenericVideoFilter(_child), precision(_precision), convertYUV(_convertYuv) {
	if (!vi.IsYV24() && !vi.IsRGB32())
		env->ThrowError("ConvertToFloat: Source must be YV12, YV24 or RGB32");
	if (precision < 1 && precision > 3)
		env->ThrowError("ConvertToFloat: Precision must be 1, 2 or 3");

	// Keep original clip information
	srcWidth = vi.width;
	srcHeight = vi.height;
	srcRgb = vi.IsRGB32();

	vi.pixel_type = VideoInfo::CS_BGR32;
	if (precision > 1) // Half-float frame has its width twice larger than normal
		vi.width <<= 1;

	if (precision == 1)
		precisionShift = 2;
	else if (precision == 2)
		precisionShift = 3;
	else
		precisionShift = 4;

	if (precision == 3) {
		floatBufferPitch = srcWidth << 4;
		floatBuffer = (unsigned char*)malloc(floatBufferPitch);
		halfFloatBufferPitch = srcWidth << 3;
		halfFloatBuffer = (unsigned char*)malloc(halfFloatBufferPitch);
	}
}

ConvertToFloat::~ConvertToFloat() {
	if (precision == 3) {
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

void ConvertToFloat::convRgbToFloat(const byte *src, unsigned char *dst, int srcPitch, int dstPitch, int width, int height, IScriptEnvironment* env) {
	unsigned char* dstLoop = precision == 3 ? floatBuffer : dst;
	int dstLoopPitch = precision == 3 ? floatBufferPitch : dstPitch;

	unsigned char B, G, R;
	src += height * srcPitch;
	for (int y = 0; y < height; ++y) {
		src -= srcPitch;
		for (int x = 0; x < width; ++x) {
			B = src[x << 2];
			G = src[(x << 2) + 1];
			R = src[(x << 2) + 2];

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
void ConvertToFloat::convFloat(unsigned char y, unsigned char u, unsigned char v, unsigned char* out) {
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
void ConvertToFloat::convInt(unsigned char y, unsigned char u, unsigned char v, unsigned char* out) {
	if (precision == 1) {
		out[0] = y;
		out[1] = u;
		out[2] = v;
	}
	else { // Precision == 2
		unsigned short outY = y << 8;
		unsigned short outU = u << 8;
		unsigned short outV = v << 8;
		memcpy(out, &outY, 2);
		memcpy(out + 2, &outU, 2);
		memcpy(out + 4, &outV, 2);
	}
}