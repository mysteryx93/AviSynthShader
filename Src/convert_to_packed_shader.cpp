#include <map>
#include <tuple>
#include "ConvertShader.h"



static void __stdcall
yuv_to_shader_1_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* sy = srcp[0];
    const uint8_t* su = srcp[1];
    const uint8_t* sv = srcp[2];

    uint8_t* d = dstp[0];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            d[4 * x + 0] = sv[x];
            d[4 * x + 1] = su[x];
            d[4 * x + 2] = sy[x];
            d[4 * x + 3] = 0;
        }
        sy += spitch;
        su += spitch;
        sv += spitch;
        d += dpitch;
    }
}


static void __stdcall
yuv_to_shader_1_sse2(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* sy = srcp[0];
    const uint8_t* su = srcp[1];
    const uint8_t* sv = srcp[2];

    uint8_t* d = dstp[0];

    const __m128i a = _mm_setzero_si128();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 8) {
            __m128i y = loadl(sy + x);
            __m128i u = loadl(su + x);
            __m128i v = loadl(sv + x);
            __m128i vu = _mm_unpacklo_epi8(v, u);
            __m128i ya = _mm_unpacklo_epi8(y, a);
            __m128i vuya0 = _mm_unpacklo_epi16(vu, ya);
            __m128i vuya1 = _mm_unpackhi_epi16(vu, ya);
            stream(d + 4 * x + 0, vuya0);
            stream(d + 4 * x + 16, vuya1);
        }
        sy += spitch;
        su += spitch;
        sv += spitch;
        d += dpitch;
    }
}


template <bool STACK16>
static void __stdcall
yuv_to_shader_2_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* sy = srcp[0];
    const uint8_t* su = srcp[1];
    const uint8_t* sv = srcp[2];

    uint8_t* d = dstp[0];

    const uint8_t *ylsb, *ulsb, *vlsb;
    if (STACK16) {
        ylsb = sy + height * spitch;
        ulsb = su + height * spitch;
        vlsb = sv + height * spitch;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (!STACK16) {
                d[8 * x + 0] = 0;
                d[8 * x + 1] = sv[x];
                d[8 * x + 2] = 0;
                d[8 * x + 3] = su[x];
                d[8 * x + 4] = 0;
                d[8 * x + 5] = sy[x];
            } else {
                d[8 * x + 0] = vlsb[x];
                d[8 * x + 1] = sv[x];
                d[8 * x + 2] = ulsb[x];
                d[8 * x + 3] = su[x];
                d[8 * x + 4] = ylsb[x];
                d[8 * x + 5] = sy[x];
            }
            d[8 * x + 6] = d[8 * x + 7] = 0;
        }
        d += dpitch;
        sy += spitch;
        su += spitch;
        sv += spitch;
        if (STACK16) {
            ylsb += spitch;
            ulsb += spitch;
            vlsb += spitch;
        }
    }
}



template <bool STACK16>
static void __stdcall
yuv_to_shader_2_sse2(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* sy = srcp[0];
    const uint8_t* su = srcp[1];
    const uint8_t* sv = srcp[2];

    uint8_t* d = dstp[0];

    const uint8_t *ylsb, *ulsb, *vlsb;
    if (STACK16) {
        ylsb = sy + height * spitch;
        ulsb = su + height * spitch;
        vlsb = sv + height * spitch;
    }

    const __m128i zero = _mm_setzero_si128();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 8) {
            __m128i y, u, v;
            if (!STACK16) {
                y = _mm_unpacklo_epi8(zero, loadl(sy + x));
                u = _mm_unpacklo_epi8(zero, loadl(su + x));
                v = _mm_unpacklo_epi8(zero, loadl(sv + x));
            } else {
                y = _mm_unpacklo_epi8(loadl(ylsb + x), loadl(sy + x));
                u = _mm_unpacklo_epi8(loadl(ulsb + x), loadl(su + x));
                v = _mm_unpacklo_epi8(loadl(vlsb + x), loadl(sv + x));
            }
            __m128i vu = _mm_unpacklo_epi16(v, u);
            __m128i ya = _mm_unpacklo_epi16(y, zero);
            stream(d + 8 * x + 0, _mm_unpacklo_epi32(vu, ya));
            stream(d + 8 * x + 16, _mm_unpackhi_epi32(vu, ya));
            vu = _mm_unpackhi_epi16(v, u);
            ya = _mm_unpackhi_epi16(y, zero);
            stream(d + 8 * x + 32, _mm_unpacklo_epi32(vu, ya));
            stream(d + 8 * x + 48, _mm_unpackhi_epi32(vu, ya));
        }
        d += dpitch;
        sy += spitch;
        su += spitch;
        sv += spitch;
        if (STACK16) {
            ylsb += spitch;
            ulsb += spitch;
            vlsb += spitch;
        }
    }
}


template <bool STACK16>
static void __stdcall
yuv_to_shader_3_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _lut) noexcept
{
    const uint8_t* sy = srcp[0];
    const uint8_t* su = srcp[1];
    const uint8_t* sv = srcp[2];

    uint8_t* d = dstp[0];

    const uint16_t* lut = reinterpret_cast<const uint16_t*>(_lut);

    const uint8_t *ylsb, *ulsb, *vlsb;
    if (STACK16) {
        ylsb = sy + height * spitch;
        ulsb = su + height * spitch;
        vlsb = sv + height * spitch;
    }

    for (int y = 0; y < height; ++y) {
        uint16_t* d16 = reinterpret_cast<uint16_t*>(d);
        for (int x = 0; x < width; ++x) {
            if (!STACK16) {
                d16[4 * x + 0] = lut[sv[x]];
                d16[4 * x + 1] = lut[su[x]];
                d16[4 * x + 2] = lut[sy[x]];
                d16[4 * x + 3] = 0;
            } else {
                d16[4 * x + 0] = lut[(sv[x] << 8) | vlsb[x]];
                d16[4 * x + 1] = lut[(su[x] << 8) | ulsb[x]];
                d16[4 * x + 2] = lut[(sy[x] << 8) | ylsb[x]];
                d16[4 * x + 3] = 0;
            }
        }
        if (STACK16) {
            ylsb += spitch;
            ulsb += spitch;
            vlsb += spitch;
        }
        d += dpitch;
        sy += spitch;
        su += spitch;
        sv += spitch;
    }
}


#if defined(__AVX__)
template <bool STACK16>
static void __stdcall
yuv_to_shader_3_f16c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _buff) noexcept
{
    const uint8_t* sy = srcp[0];
    const uint8_t* su = srcp[1];
    const uint8_t* sv = srcp[2];

    uint8_t* d = dstp[0];
    float* buff = reinterpret_cast<float*>(_buff);

    const uint8_t *ylsb, *ulsb, *vlsb;
    if (STACK16) {
        ylsb = sy + height * spitch;
        ulsb = su + height * spitch;
        vlsb = sv + height * spitch;
    }

    const __m128i zero = _mm_setzero_si128();
    const __m128 rcp = _mm_set1_ps(1.0f / (STACK16 ? 65535 : 255));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 4) {
            __m128i y, u, v;
            if (!STACK16) {
                y = _mm_unpacklo_epi16(_mm_unpacklo_epi8(loadl(sy + x), zero), zero);
                u = _mm_unpacklo_epi16(_mm_unpacklo_epi8(loadl(su + x), zero), zero);
                v = _mm_unpacklo_epi16(_mm_unpacklo_epi8(loadl(sv + x), zero), zero);
            } else {
                y = _mm_unpacklo_epi16(_mm_unpacklo_epi8(loadl(sy + x), loadl(ylsb + x)), zero);
                u = _mm_unpacklo_epi16(_mm_unpacklo_epi8(loadl(su + x), loadl(ulsb + x)), zero);
                v = _mm_unpacklo_epi16(_mm_unpacklo_epi8(loadl(sv + x), loadl(vlsb + x)), zero);
            }
            __m128i vu = _mm_unpacklo_epi32(v, u);
            __m128i ya = _mm_unpacklo_epi32(y, zero);
            __m128 vuya0 = _mm_cvtepi32_ps(_mm_unpacklo_epi64(vu, ya));
            __m128 vuya1 = _mm_cvtepi32_ps(_mm_unpackhi_epi64(vu, ya));
            _mm_store_ps(buff + 4 * x + 0, _mm_mul_ps(vuya0, rcp));
            _mm_store_ps(buff + 4 * x + 4, _mm_mul_ps(vuya1, rcp));

            vu = _mm_unpackhi_epi32(v, u);
            ya = _mm_unpackhi_epi32(y, zero);
            vuya0 = _mm_cvtepi32_ps(_mm_unpacklo_epi64(vu, ya));
            vuya1 = _mm_cvtepi32_ps(_mm_unpackhi_epi64(vu, ya));
            _mm_store_ps(buff + 4 * x +  8, _mm_mul_ps(vuya0, rcp));
            _mm_store_ps(buff + 4 * x + 12, _mm_mul_ps(vuya1, rcp));
        }
        convert_float_to_half(d, buff, width * 4);
        d += dpitch;
        sy += spitch;
        su += spitch;
        sv += spitch;
        if (STACK16) {
            ylsb += spitch;
            ulsb += spitch;
            vlsb += spitch;
        }
    }
}
#endif


template <bool IS_RGB32>
static void __stdcall
rgb_to_shader_2_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    constexpr size_t step = IS_RGB32 ? 4 : 3;

    const uint8_t* s = srcp[0];
    uint8_t* d = dstp[0];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            d[8 * x + 0] = 0;
            d[8 * x + 1] = s[step * x + 0];
            d[8 * x + 2] = 0;
            d[8 * x + 3] = s[step * x + 1];
            d[8 * x + 4] = 0;
            d[8 * x + 5] = s[step * x + 2];
            d[8 * x + 6] = 0;
            d[8 * x + 7] = IS_RGB32 ? s[4 * x + 3] : 0;
        }
        d += dpitch;
        s += spitch;
    }
}


static void __stdcall
rgb32_to_shader_2_sse2(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* s = srcp[0];
    uint8_t* d = dstp[0];

    const __m128i zero = _mm_setzero_si128();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 2) {
            stream(d + 8 * x, _mm_unpacklo_epi8(zero, loadl(s + 4 * x)));
        }
        d += dpitch;
        s += spitch;
    }
}




template <bool IS_RGB32>
static void __stdcall
rgb_to_shader_3_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _lut) noexcept
{
    constexpr size_t step = IS_RGB32 ? 4 : 3;
    const uint8_t* s = srcp[0];
    uint8_t* d = dstp[0];
    const uint16_t* lut = reinterpret_cast<const uint16_t*>(_lut);

    for (int y = 0; y < height; ++y) {
        uint16_t* d16 = reinterpret_cast<uint16_t*>(d);
        for (int x = 0; x < width; ++x) {
            d16[4 * x + 0] = lut[s[step * x + 0]];
            d16[4 * x + 1] = lut[s[step * x + 1]];
            d16[4 * x + 2] = lut[s[step * x + 2]];
            d16[4 * x + 3] = IS_RGB32 ? lut[s[4 * x + 3]] : 0;
        }
        d += dpitch;
        s += spitch;
    }
}


#if defined(__AVX__)
static void __stdcall
rgb32_to_shader_3_f16c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _buff) noexcept
{
    const uint8_t* s = srcp[0];
    uint8_t* d = dstp[0];
    float* buff = reinterpret_cast<float*>(_buff);

    const __m128i zero = _mm_setzero_si128();
    const __m128 rcp = _mm_set1_ps(1.0f / 255);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 2) {
            __m128i sx = _mm_unpacklo_epi8(loadl(s + 4 * x), zero);

            __m128i s0 = _mm_unpacklo_epi16(sx, zero);
            __m128 f0 = _mm_mul_ps(_mm_cvtepi32_ps(s0), rcp);
            _mm_store_ps(buff + 4 * x + 0, f0);

            s0 = _mm_unpackhi_epi16(sx, zero);
            f0 = _mm_mul_ps(_mm_cvtepi32_ps(s0), rcp);
            _mm_store_ps(buff + 4 * x + 4, f0);
        }
        convert_float_to_half(d, buff, width * 4);
        d += dpitch;
        s += spitch;
    }
}
#endif


convert_shader_t get_to_shader_packed(int precision, int pix_type, bool stack16, arch_t arch)
{
    using std::make_tuple;

    constexpr int rgb24 = VideoInfo::CS_BGR24;
    constexpr int rgb32 = VideoInfo::CS_BGR32;
    constexpr int yv24 = VideoInfo::CS_YV24;

    std::map<std::tuple<int, int, bool, arch_t>, convert_shader_t> func;

    func[make_tuple(1, yv24, false, NO_SIMD)] = yuv_to_shader_1_c;
    func[make_tuple(1, yv24, false, USE_SSE2)] = yuv_to_shader_1_sse2;

    func[make_tuple(2, yv24, false, NO_SIMD)] = yuv_to_shader_2_c<false>;
    func[make_tuple(2, yv24, true, NO_SIMD)] = yuv_to_shader_2_c<true>;
    func[make_tuple(2, yv24, false, USE_SSE2)] = yuv_to_shader_2_sse2<false>;
    func[make_tuple(2, yv24, true, USE_SSE2)] = yuv_to_shader_2_sse2<true>;

    func[make_tuple(2, rgb24, false, NO_SIMD)] = rgb_to_shader_2_c<false>;
    func[make_tuple(2, rgb32, false, NO_SIMD)] = rgb_to_shader_2_c<true>;
    func[make_tuple(2, rgb32, false, USE_SSE2)] = rgb32_to_shader_2_sse2;

    func[make_tuple(3, yv24, false, NO_SIMD)] = yuv_to_shader_3_c<false>;
    func[make_tuple(3, yv24, true, NO_SIMD)] = yuv_to_shader_3_c<true>;

    func[make_tuple(3, rgb24, false, NO_SIMD)] = rgb_to_shader_3_c<false>;
    func[make_tuple(3, rgb32, false, NO_SIMD)] = rgb_to_shader_3_c<true>;

#if defined(__AVX__)
    func[make_tuple(3, yv24, false, USE_F16C)] = yuv_to_shader_3_f16c<false>;
    func[make_tuple(3, yv24, true, USE_F16C)] = yuv_to_shader_3_f16c<true>;
    func[make_tuple(3, rgb32, false, USE_F16C)] = rgb32_to_shader_3_f16c;
#endif


    return func[make_tuple(precision, pix_type, stack16, arch)];
}

