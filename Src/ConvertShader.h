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



class ConvertShader : public GenericVideoFilter {
    const char* name;
    VideoInfo viSrc;
    int procWidth;
    int procHeight;
    int floatBufferPitch;
    float* buff;
    bool isPlusMt;

    void constructToShader(int precision, bool stack16, bool planar, arch_t arch, IScriptEnvironment* env);
    void constructFromShader(int precision, bool stack16, std::string& format, arch_t arch);
    convert_shader_t mainProc;

public:
    ConvertShader(PClip _child, int _precision, bool stack16, std::string& format, bool planar, int opt, IScriptEnvironment* env);
    ~ConvertShader();
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
};


convert_shader_t get_to_shader_packed(int precision, int pix_type, bool stack16, arch_t arch);
convert_shader_t get_to_shader_planar(int precision, int pix_type, bool stack16, arch_t arch);
convert_shader_t get_from_shader_packed(int precision, int pix_type, bool stack16, arch_t arch);
convert_shader_t get_from_shader_planar(int precision, int pix_type, bool stack16, arch_t arch);


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


#if defined(USE_UINT8_TO_HALF_TABLE)
static const uint16_t half_table[256] = {
    0x0000, 0x1C04, 0x2004, 0x2206, 0x2404, 0x2505, 0x2606, 0x2707,
    0x2804, 0x2885, 0x2905, 0x2986, 0x2A06, 0x2A87, 0x2B07, 0x2B88,
    0x2C04, 0x2C44, 0x2C85, 0x2CC5, 0x2D05, 0x2D45, 0x2D86, 0x2DC6,
    0x2E06, 0x2E46, 0x2E87, 0x2EC7, 0x2F07, 0x2F47, 0x2F88, 0x2FC8,
    0x3004, 0x3024, 0x3044, 0x3064, 0x3085, 0x30A5, 0x30C5, 0x30E5,
    0x3105, 0x3125, 0x3145, 0x3165, 0x3186, 0x31A6, 0x31C6, 0x31E6,
    0x3206, 0x3226, 0x3246, 0x3266, 0x3287, 0x32A7, 0x32C7, 0x32E7,
    0x3307, 0x3327, 0x3347, 0x3367, 0x3388, 0x33A8, 0x33C8, 0x33E8,
    0x3404, 0x3414, 0x3424, 0x3434, 0x3444, 0x3454, 0x3464, 0x3474,
    0x3485, 0x3495, 0x34A5, 0x34B5, 0x34C5, 0x34D5, 0x34E5, 0x34F5,
    0x3505, 0x3515, 0x3525, 0x3535, 0x3545, 0x3555, 0x3565, 0x3575,
    0x3586, 0x3596, 0x35A6, 0x35B6, 0x35C6, 0x35D6, 0x35E6, 0x35F6,
    0x3606, 0x3616, 0x3626, 0x3636, 0x3646, 0x3656, 0x3666, 0x3676,
    0x3687, 0x3697, 0x36A7, 0x36B7, 0x36C7, 0x36D7, 0x36E7, 0x36F7,
    0x3707, 0x3717, 0x3727, 0x3737, 0x3747, 0x3757, 0x3767, 0x3777,
    0x3788, 0x3798, 0x37A8, 0x37B8, 0x37C8, 0x37D8, 0x37E8, 0x37F8,
    0x3804, 0x380C, 0x3814, 0x381C, 0x3824, 0x382C, 0x3834, 0x383C,
    0x3844, 0x384C, 0x3854, 0x385C, 0x3864, 0x386C, 0x3874, 0x387C,
    0x3885, 0x388D, 0x3895, 0x389D, 0x38A5, 0x38AD, 0x38B5, 0x38BD,
    0x38C5, 0x38CD, 0x38D5, 0x38DD, 0x38E5, 0x38ED, 0x38F5, 0x38FD,
    0x3905, 0x390D, 0x3915, 0x391D, 0x3925, 0x392D, 0x3935, 0x393D,
    0x3945, 0x394D, 0x3955, 0x395D, 0x3965, 0x396D, 0x3975, 0x397D,
    0x3986, 0x398E, 0x3996, 0x399E, 0x39A6, 0x39AE, 0x39B6, 0x39BE,
    0x39C6, 0x39CE, 0x39D6, 0x39DE, 0x39E6, 0x39EE, 0x39F6, 0x39FE,
    0x3A06, 0x3A0E, 0x3A16, 0x3A1E, 0x3A26, 0x3A2E, 0x3A36, 0x3A3E,
    0x3A46, 0x3A4E, 0x3A56, 0x3A5E, 0x3A66, 0x3A6E, 0x3A76, 0x3A7E,
    0x3A87, 0x3A8F, 0x3A97, 0x3A9F, 0x3AA7, 0x3AAF, 0x3AB7, 0x3ABF,
    0x3AC7, 0x3ACF, 0x3AD7, 0x3ADF, 0x3AE7, 0x3AEF, 0x3AF7, 0x3AFF,
    0x3B07, 0x3B0F, 0x3B17, 0x3B1F, 0x3B27, 0x3B2F, 0x3B37, 0x3B3F,
    0x3B47, 0x3B4F, 0x3B57, 0x3B5F, 0x3B67, 0x3B6F, 0x3B77, 0x3B7F,
    0x3B88, 0x3B90, 0x3B98, 0x3BA0, 0x3BA8, 0x3BB0, 0x3BB8, 0x3BC0,
    0x3BC8, 0x3BD0, 0x3BD8, 0x3BE0, 0x3BE8, 0x3BF0, 0x3BF8, 0x3C00,
};
#endif
