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
            // on 8bits, color order of shader is ARGB
            //map 0 to A, y to R, u to G, v to B
            d[4 * x + 0] = 0;
            d[4 * x + 1] = sy[x];
            d[4 * x + 2] = su[x];
            d[4 * x + 3] = sv[x];

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

    const __m128i zero = _mm_setzero_si128();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 8) {
            __m128i y = loadl(sy + x);
            __m128i u = loadl(su + x);
            __m128i v = loadl(sv + x);
            // on 8bits, color order of shader is ARGB
            // map 0 to A, y to R, u to G, v to B
            __m128i ar = _mm_unpacklo_epi8(zero, y);
            __m128i gb = _mm_unpacklo_epi8(u, v);
            __m128i argb0 = _mm_unpacklo_epi16(ar, gb);
            __m128i argb1 = _mm_unpackhi_epi16(ar, gb);
            stream(d + 4 * x + 0, argb0);
            stream(d + 4 * x + 16, argb1);
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
            //on 16bits, color oreder of shader is ABGR
            // map 0 to A, v to B, u to G, y to R
            d[8 * x + 0] = d[8 * x + 1] = 0;
            if (!STACK16) {
                d[8 * x + 2] = 0;
                d[8 * x + 3] = sv[x];
                d[8 * x + 4] = 0;
                d[8 * x + 5] = su[x];
                d[8 * x + 6] = 0;
                d[8 * x + 7] = sy[x];
            } else {
                d[8 * x + 2] = vlsb[x];
                d[8 * x + 3] = sv[x];
                d[8 * x + 4] = ulsb[x];
                d[8 * x + 5] = su[x];
                d[8 * x + 6] = ylsb[x];
                d[8 * x + 7] = sy[x];
            }
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
            //on 16bits, color oreder of shader is ABGR
            // map 0 to A, v to B, u to G, y to R
            __m128i ab = _mm_unpacklo_epi16(zero, v);
            __m128i gr = _mm_unpacklo_epi16(u, y);
            stream(d + 8 * x + 0, _mm_unpacklo_epi32(ab, gr));
            stream(d + 8 * x + 16, _mm_unpackhi_epi32(ab, gr));
            ab = _mm_unpackhi_epi16(zero, v);
            gr = _mm_unpackhi_epi16(u, y);
            stream(d + 8 * x + 32, _mm_unpacklo_epi32(ab, gr));
            stream(d + 8 * x + 48, _mm_unpackhi_epi32(ab, gr));
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
            //on half precision, color oreder of shader is ABGR
            // map 0 to A, v to B, u to G, y to R
            d16[4 * x + 0] = 0;
            if (!STACK16) {
                d16[4 * x + 1] = lut[sv[x]];
                d16[4 * x + 2] = lut[su[x]];
                d16[4 * x + 3] = lut[sy[x]];
            } else {
                d16[4 * x + 1] = lut[(sv[x] << 8) | vlsb[x]];
                d16[4 * x + 2] = lut[(su[x] << 8) | ulsb[x]];
                d16[4 * x + 3] = lut[(sy[x] << 8) | ylsb[x]];
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
                y = _mm_cvtepu8_epi32(loadl(sy + x));
                u = _mm_cvtepu8_epi32(loadl(su + x));
                v = _mm_cvtepu8_epi32(loadl(sv + x));
            } else {
                y = _mm_unpacklo_epi16(_mm_unpacklo_epi8(loadl(ylsb + x), loadl(sy + x)), zero);
                u = _mm_unpacklo_epi16(_mm_unpacklo_epi8(loadl(ulsb + x), loadl(su + x)), zero);
                v = _mm_unpacklo_epi16(_mm_unpacklo_epi8(loadl(vlsb + x), loadl(sv + x)), zero);
            }
            //on half precision, color oreder of shader is ABGR
            // map 0 to A, v to B, u to G, y to R
            __m128i ab = _mm_unpacklo_epi32(zero, y);
            __m128i gr = _mm_unpacklo_epi32(u, v);
            __m128 abgr0 = _mm_cvtepi32_ps(_mm_unpacklo_epi64(ab, gr));
            __m128 abgr1 = _mm_cvtepi32_ps(_mm_unpackhi_epi64(ab, gr));
            _mm_store_ps(buff + 4 * x + 0, _mm_mul_ps(abgr0, rcp));
            _mm_store_ps(buff + 4 * x + 4, _mm_mul_ps(abgr1, rcp));

            ab = _mm_unpackhi_epi32(zero, y);
            gr = _mm_unpackhi_epi32(u, v);
            abgr0 = _mm_cvtepi32_ps(_mm_unpacklo_epi64(ab, gr));
            abgr1 = _mm_cvtepi32_ps(_mm_unpackhi_epi64(ab, gr));
            _mm_store_ps(buff + 4 * x +  8, _mm_mul_ps(abgr0, rcp));
            _mm_store_ps(buff + 4 * x + 12, _mm_mul_ps(abgr1, rcp));
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
rgb_to_shader_1_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    constexpr size_t step = IS_RGB32 ? 4 : 3;

    const uint8_t* s = srcp[0] + (height - 1) * spitch;
    uint8_t* d = dstp[0];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // on 8bits, color order of shader is ARGB.
            d[4 * x + 0] = IS_RGB32 ? s[4 * x + 3] : 0;
            d[4 * x + 1] = s[step * x + 2];
            d[4 * x + 2] = s[step * x + 1];
            d[4 * x + 3] = s[step * x + 0];
        }
        d += dpitch;
        s -= spitch;
    }
}


static void __stdcall
rgb32_to_shader_1_ssse3(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* s = srcp[0] + (height - 1) * spitch;
    uint8_t* d = dstp[0];

    // on 8bits, color order of shader is ARGB.
    const __m128i order = _mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 4) {
            stream(d + 4 * x, _mm_shuffle_epi8(load(s + 4 * x), order));
        }
        d += dpitch;
        s -= spitch;
    }
}


template <bool IS_RGB32>
static void __stdcall
rgb_to_shader_2_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    constexpr size_t step = IS_RGB32 ? 4 : 3;

    const uint8_t* s = srcp[0] + (height - 1) * spitch;
    uint8_t* d = dstp[0];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            //on 16bits, color order of shader is ABGR.
            d[8 * x + 0] = 0;
            d[8 * x + 1] = IS_RGB32 ? s[4 * x + 3] : 0;
            d[8 * x + 2] = 0;
            d[8 * x + 3] = s[step * x + 0];
            d[8 * x + 4] = 0;
            d[8 * x + 5] = s[step * x + 1];
            d[8 * x + 6] = 0;
            d[8 * x + 7] = s[step * x + 2];
        }
        d += dpitch;
        s -= spitch;
    }
}


static void __stdcall
rgb32_to_shader_2_ssse3(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* s = srcp[0] + (height - 1) * spitch;
    uint8_t* d = dstp[0];

    const __m128i zero = _mm_setzero_si128();
    //on 16bits, color order of shader is ABGR.
    const __m128i order = _mm_setr_epi8(3, 0, 1, 2, 7, 4, 5, 6, 11, 8, 9, 10, 15, 12, 13, 14);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 4) {
            __m128i t = load(s + 4 * x);
            t = _mm_shuffle_epi8(t, order);
            stream(d + 8 * x, _mm_unpacklo_epi8(zero, t));
            stream(d + 8 * x + 16, _mm_unpackhi_epi8(zero, t));
        }
        d += dpitch;
        s -= spitch;
    }
}


template <bool IS_RGB32>
static void __stdcall
rgb_to_shader_3_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _lut) noexcept
{
    constexpr size_t step = IS_RGB32 ? 4 : 3;
    const uint8_t* s = srcp[0] + (height - 1) * spitch;
    uint8_t* d = dstp[0];
    const uint16_t* lut = reinterpret_cast<const uint16_t*>(_lut);

    for (int y = 0; y < height; ++y) {
        uint16_t* d16 = reinterpret_cast<uint16_t*>(d);
        for (int x = 0; x < width; ++x) {
            //on half precision, order of shader is ABGR.
            d16[4 * x + 0] = IS_RGB32 ? lut[s[4 * x + 3]] : 0;
            d16[4 * x + 1] = lut[s[step * x + 0]];
            d16[4 * x + 2] = lut[s[step * x + 1]];
            d16[4 * x + 3] = lut[s[step * x + 2]];
        }
        d += dpitch;
        s -= spitch;
    }
}


#if defined(__AVX__)
static void __stdcall
rgb32_to_shader_3_f16c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _buff) noexcept
{
    const uint8_t* s = srcp[0] + (height - 1) * spitch;
    uint8_t* d = dstp[0];
    float* buff = reinterpret_cast<float*>(_buff);

    const __m128i zero = _mm_setzero_si128();
    const __m128 rcp = _mm_set1_ps(1.0f / 255);
    //on half precision, order of shader is ABGR.
    const __m128i order = _mm_setr_epi8(3, 0, 1, 2, 7, 4, 5, 6, 11, 8, 9, 10, 15, 12, 13, 14);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 4) {
            __m128i sx = _mm_shuffle_epi8(load(s + 4 * x), order);

            __m128i s0 = _mm_cvtepu8_epi32(sx);
            __m128 f0 = _mm_mul_ps(_mm_cvtepi32_ps(s0), rcp);
            _mm_store_ps(buff + 4 * x + 0, f0);

            s0 = _mm_cvtepu8_epi32(_mm_srli_si128(sx, 4));
            f0 = _mm_mul_ps(_mm_cvtepi32_ps(s0), rcp);
            _mm_store_ps(buff + 4 * x + 4, f0);

            s0 = _mm_cvtepu8_epi32(_mm_srli_si128(sx, 8));
            f0 = _mm_mul_ps(_mm_cvtepi32_ps(s0), rcp);
            _mm_store_ps(buff + 4 * x + 8, f0);

            s0 = _mm_cvtepu8_epi32(_mm_srli_si128(sx, 12));
            f0 = _mm_mul_ps(_mm_cvtepi32_ps(s0), rcp);
            _mm_store_ps(buff + 4 * x + 12, f0);
        }
        convert_float_to_half(d, buff, width * 4);
        d += dpitch;
        s -= spitch;
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

    func[make_tuple(1, rgb24, false, NO_SIMD)] = rgb_to_shader_1_c<false>;
    func[make_tuple(1, rgb32, false, NO_SIMD)] = rgb_to_shader_1_c<true>;
    func[make_tuple(1, rgb32, false, USE_SSSE3)] = rgb32_to_shader_1_ssse3;

    func[make_tuple(2, yv24, false, NO_SIMD)] = yuv_to_shader_2_c<false>;
    func[make_tuple(2, yv24, true, NO_SIMD)] = yuv_to_shader_2_c<true>;
    func[make_tuple(2, yv24, false, USE_SSE2)] = yuv_to_shader_2_sse2<false>;
    func[make_tuple(2, yv24, true, USE_SSE2)] = yuv_to_shader_2_sse2<true>;

    func[make_tuple(2, rgb24, false, NO_SIMD)] = rgb_to_shader_2_c<false>;
    func[make_tuple(2, rgb32, false, NO_SIMD)] = rgb_to_shader_2_c<true>;
    func[make_tuple(2, rgb32, false, USE_SSSE3)] = rgb32_to_shader_2_ssse3;

    func[make_tuple(3, yv24, false, NO_SIMD)] = yuv_to_shader_3_c<false>;
    func[make_tuple(3, yv24, true, NO_SIMD)] = yuv_to_shader_3_c<true>;

    func[make_tuple(3, rgb24, false, NO_SIMD)] = rgb_to_shader_3_c<false>;
    func[make_tuple(3, rgb32, false, NO_SIMD)] = rgb_to_shader_3_c<true>;

#if defined(__AVX__)
    func[make_tuple(3, yv24, false, USE_F16C)] = yuv_to_shader_3_f16c<false>;
    func[make_tuple(3, yv24, true, USE_F16C)] = yuv_to_shader_3_f16c<true>;
    func[make_tuple(3, rgb32, false, USE_F16C)] = rgb32_to_shader_3_f16c;
#endif

    if (precision != 3 && arch == USE_F16C) {
        arch = USE_SSSE3;
    }
    if (pix_type == rgb24) {
        arch = NO_SIMD;
    }
    if (pix_type == rgb32 && arch < USE_SSSE3) {
        arch = NO_SIMD;
    }
    if (pix_type == yv24 && arch == USE_SSSE3) {
        arch = USE_SSE2;
    }
    if (precision == 3 && arch != USE_F16C) {
        arch = NO_SIMD;
    }


    return func[make_tuple(precision, pix_type, stack16, arch)];
}

