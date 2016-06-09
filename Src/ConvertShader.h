#pragma once
#include <cstdint>
#include <string>
#if defined(__AVX__)
#include <immintrin.h>
#else
#include <emmintrin.h>
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#include <windows.h>
#endif
#include <DirectXPackedVector.h>
#include "avisynth.h"

#pragma warning(disable: 4556)

enum arch_t {
    NO_SIMD,
    USE_SSE2,
    USE_F16C,
};


using convert_shader_t = void(__stdcall*)(
    uint8_t** dstp, const uint8_t** srcp, const int dpitch, const int spitch, const int width, const int height, float* buff);


// Converts YV12 data into RGB data with float precision, 12-byte per pixel.
class ConvertToShader : public GenericVideoFilter {
    VideoInfo viSrc;
    int floatBufferPitch;
    float* buff;
    bool isPlusMt;

    convert_shader_t mainProc;

public:
    ConvertToShader(PClip _child, int _precision, bool stack16, bool planar, int opt, IScriptEnvironment* env);
    ~ConvertToShader();
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);

};


// Converts float-precision RGB data (12-byte per pixel) into YV12 format.
class ConvertFromShader : public GenericVideoFilter {
    VideoInfo viSrc;
    bool isPlusMt;
    int floatBufferPitch;
    float* buff;

    convert_shader_t mainProc;

public:
    ConvertFromShader(PClip _child, int _precision, std::string format, bool stack16, int opt, IScriptEnvironment* env);
    ~ConvertFromShader();
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);

};


convert_shader_t get_to_shader_packed(int precision, int pix_type, bool stack16, arch_t arch);
convert_shader_t get_to_shader_planar(int precision, int pix_type, bool stack16, arch_t arch);
convert_shader_t get_from_shader_packed(int precision, int pix_type, bool stack16, arch_t arch);
convert_shader_t get_from_shader_planar(int precision, int pix_type, bool stack16, arch_t arch);



extern bool has_sse2() noexcept;
extern bool has_f16c() noexcept;


static inline arch_t get_arch(int opt)
{
    if (opt == 0 || !has_sse2()) {
        return NO_SIMD;
    }
#if !defined(__AVX__)
    return USE_SSE2;
#else
    if (opt == 1 || !has_f16c()) {
        return USE_SSE2;
    }
    return USE_F16C;
#endif
}




static __forceinline __m128i loadl(const uint8_t* p)
{
    return _mm_loadl_epi64(reinterpret_cast<const __m128i*>(p));
}


static __forceinline __m128i load(const uint8_t* p)
{
    return _mm_load_si128(reinterpret_cast<const __m128i*>(p));
}


static __forceinline void storel(uint8_t* p, const __m128i& x)
{
    _mm_storel_epi64(reinterpret_cast<__m128i*>(p), x);
}


static __forceinline void stream(uint8_t* p, const __m128i& x)
{
    _mm_stream_si128(reinterpret_cast<__m128i*>(p), x);
}


template <arch_t ARCH>
static __forceinline void
convert_float_to_half(uint8_t* dstp, const float* srcp, size_t count)
{
    using namespace DirectX::PackedVector;
#if defined(__AVX__)
    if (ARCH == USE_F16C) {
        for (size_t x = 0; x < count; x += 8) {
            __m256 s = _mm256_load_ps(srcp + x);
            __m128i d = _mm256_cvtps_ph(s, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
            stream(dstp + 2 * x, d);
        }
    } else {
        XMConvertFloatToHalfStream(reinterpret_cast<HALF*>(dstp), sizeof(HALF), srcp, sizeof(float), count);
    }
#else
    XMConvertFloatToHalfStream(reinterpret_cast<HALF*>(dstp), sizeof(HALF), srcp, sizeof(float), count);
#endif
}


template <arch_t ARCH>
static __forceinline void
convert_half_to_float(float* dstp, const uint8_t* srcp, size_t count)
{
    using namespace DirectX::PackedVector;
#if defined(__AVX__)
    if (ARCH == USE_F16C) {
        for (size_t x = 0; x < count; x += 8) {
            __m128i s = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + 2 * x));
            __m256 d = _mm256_cvtph_ps(s);
            _mm256_store_ps(dstp + x, d);
        }
    } else {
        XMConvertHalfToFloatStream(dstp, 4, reinterpret_cast<const HALF*>(srcp), 2, count);
    }
#else
    XMConvertHalfToFloatStream(dstp, 4, reinterpret_cast<const HALF*>(srcp), 2, count);
#endif
}

