#include <map>
#include <tuple>
#include "ConvertShader.h"


/*
packed shader: color order / sample type

precision1: BGRA / all samples are uint8_t
precision2: RGBA / all samples are uint16_t(little endian)
precision3: RGBA / all samples are half precision.

map Y to R, U to G, V to B
*/


static void __stdcall
yuv_to_shader_1_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* sr = srcp[0];
    const uint8_t* sg = srcp[1];
    const uint8_t* sb = srcp[2];

    uint8_t* d = dstp[0];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            d[4 * x + 0] = sb[x];
            d[4 * x + 1] = sg[x];
            d[4 * x + 2] = sr[x];
            d[4 * x + 3] = 0;

        }
        sr += spitch;
        sg += spitch;
        sb += spitch;
        d += dpitch;
    }
}


static void __stdcall
yuv_to_shader_1_sse2(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* sr = srcp[0];
    const uint8_t* sg = srcp[1];
    const uint8_t* sb = srcp[2];

    uint8_t* d = dstp[0];

    const __m128i zero = _mm_setzero_si128();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 8) {
            __m128i r = loadl(sr + x);
            __m128i g = loadl(sg + x);
            __m128i b = loadl(sb + x);

            __m128i bg = _mm_unpacklo_epi8(b, g);
            __m128i ra = _mm_unpacklo_epi8(r, zero);

            __m128i bgra0 = _mm_unpacklo_epi16(bg, ra);
            __m128i bgra1 = _mm_unpackhi_epi16(bg, ra);

            stream(d + 4 * x + 0, bgra0);
            stream(d + 4 * x + 16, bgra1);
        }
        sr += spitch;
        sg += spitch;
        sb += spitch;
        d += dpitch;
    }
}


template <bool STACK16>
static void __stdcall
yuv_to_shader_2_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* sr = srcp[0];
    const uint8_t* sg = srcp[1];
    const uint8_t* sb = srcp[2];

    uint8_t* d = dstp[0];

    const uint8_t *rlsb, *glsb, *blsb;
    if (STACK16) {
        rlsb = sr + height * spitch;
        glsb = sg + height * spitch;
        blsb = sb + height * spitch;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (!STACK16) {
                d[8 * x + 0] = 0;
                d[8 * x + 1] = sr[x];
                d[8 * x + 2] = 0;
                d[8 * x + 3] = sg[x];
                d[8 * x + 4] = 0;
                d[8 * x + 5] = sb[x];
            } else {
                d[8 * x + 0] = rlsb[x];
                d[8 * x + 1] = sr[x];
                d[8 * x + 2] = glsb[x];
                d[8 * x + 3] = sg[x];
                d[8 * x + 4] = blsb[x];
                d[8 * x + 5] = sb[x];
            }
            d[8 * x + 6] = d[8 * x + 7] = 0;
        }
        d += dpitch;
        sr += spitch;
        sg += spitch;
        sb += spitch;
        if (STACK16) {
            rlsb += spitch;
            glsb += spitch;
            blsb += spitch;
        }
    }
}



template <bool STACK16>
static void __stdcall
yuv_to_shader_2_sse2(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* sr = srcp[0];
    const uint8_t* sg = srcp[1];
    const uint8_t* sb = srcp[2];

    uint8_t* d = dstp[0];

    const uint8_t *rlsb, *glsb, *blsb;
    if (STACK16) {
        rlsb = sr + height * spitch;
        glsb = sg + height * spitch;
        blsb = sb + height * spitch;
    }

    const __m128i zero = _mm_setzero_si128();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 8) {
            __m128i r, g, b;
            if (!STACK16) {
                r = _mm_unpacklo_epi8(zero, loadl(sr + x));
                g = _mm_unpacklo_epi8(zero, loadl(sg + x));
                b = _mm_unpacklo_epi8(zero, loadl(sb + x));
            } else {
                r = _mm_unpacklo_epi8(loadl(rlsb + x), loadl(sr + x));
                g = _mm_unpacklo_epi8(loadl(glsb + x), loadl(sg + x));
                b = _mm_unpacklo_epi8(loadl(blsb + x), loadl(sb + x));
            }
            __m128i rg = _mm_unpacklo_epi16(r, g);
            __m128i ba = _mm_unpacklo_epi16(b, zero);
            stream(d + 8 * x + 0, _mm_unpacklo_epi32(rg, ba));
            stream(d + 8 * x + 16, _mm_unpackhi_epi32(rg, ba));
            rg = _mm_unpackhi_epi16(r, g);
            ba = _mm_unpackhi_epi16(b, zero);
            stream(d + 8 * x + 32, _mm_unpacklo_epi32(rg, ba));
            stream(d + 8 * x + 48, _mm_unpackhi_epi32(rg, ba));
        }
        d += dpitch;
        sr += spitch;
        sg += spitch;
        sb += spitch;
        if (STACK16) {
            rlsb += spitch;
            glsb += spitch;
            blsb += spitch;
        }
    }
}


template <bool STACK16>
static void __stdcall
yuv_to_shader_3_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _lut) noexcept
{
    const uint8_t* sr = srcp[0];
    const uint8_t* sg = srcp[1];
    const uint8_t* sb = srcp[2];

    uint8_t* d = dstp[0];

    const uint16_t* lut = reinterpret_cast<const uint16_t*>(_lut);

    const uint8_t *rlsb, *glsb, *blsb;
    if (STACK16) {
        rlsb = sr + height * spitch;
        glsb = sg + height * spitch;
        blsb = sb + height * spitch;
    }

    for (int y = 0; y < height; ++y) {
        uint16_t* d16 = reinterpret_cast<uint16_t*>(d);
        for (int x = 0; x < width; ++x) {
            if (!STACK16) {
                d16[4 * x + 0] = lut[sr[x]];
                d16[4 * x + 1] = lut[sg[x]];
                d16[4 * x + 2] = lut[sb[x]];
            } else {
                d16[4 * x + 0] = lut[(sr[x] << 8) | rlsb[x]];
                d16[4 * x + 1] = lut[(sg[x] << 8) | glsb[x]];
                d16[4 * x + 2] = lut[(sb[x] << 8) | blsb[x]];
            }
            d16[4 * x + 3] = 0;
        }
        if (STACK16) {
            rlsb += spitch;
            glsb += spitch;
            blsb += spitch;
        }
        d += dpitch;
        sr += spitch;
        sg += spitch;
        sb += spitch;
    }
}


#if defined(__AVX__)
template <bool STACK16>
static void __stdcall
yuv_to_shader_3_f16c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _buff) noexcept
{
    const uint8_t* sr = srcp[0];
    const uint8_t* sg = srcp[1];
    const uint8_t* sb = srcp[2];

    uint8_t* d = dstp[0];
    float* buff = reinterpret_cast<float*>(_buff);

    const uint8_t *rlsb, *glsb, *blsb;
    if (STACK16) {
        rlsb = sr + height * spitch;
        glsb = sg + height * spitch;
        blsb = sb + height * spitch;
    }

    const __m128i zero = _mm_setzero_si128();
    const __m128 rcp = _mm_set1_ps(1.0f / (STACK16 ? 65535 : 255));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 4) {
            __m128i r, g, b;
            if (!STACK16) {
                r = _mm_cvtepu8_epi32(loadl(sr + x));
                g = _mm_cvtepu8_epi32(loadl(sg + x));
                b = _mm_cvtepu8_epi32(loadl(sb + x));
            } else {
                r = _mm_unpacklo_epi16(_mm_unpacklo_epi8(loadl(rlsb + x), loadl(sr + x)), zero);
                g = _mm_unpacklo_epi16(_mm_unpacklo_epi8(loadl(glsb + x), loadl(sg + x)), zero);
                b = _mm_unpacklo_epi16(_mm_unpacklo_epi8(loadl(blsb + x), loadl(sb + x)), zero);
            }
            __m128i rg = _mm_unpacklo_epi32(r, g);
            __m128i ba = _mm_unpacklo_epi32(b, zero);
            __m128 rgba0 = _mm_cvtepi32_ps(_mm_unpacklo_epi64(rg, ba));
            __m128 rgba1 = _mm_cvtepi32_ps(_mm_unpackhi_epi64(rg, ba));
            _mm_store_ps(buff + 4 * x + 0, _mm_mul_ps(rgba0, rcp));
            _mm_store_ps(buff + 4 * x + 4, _mm_mul_ps(rgba1, rcp));

            rg = _mm_unpackhi_epi32(r, g);
            ba = _mm_unpackhi_epi32(b, zero);
            rgba0 = _mm_cvtepi32_ps(_mm_unpacklo_epi64(rg, ba));
            rgba1 = _mm_cvtepi32_ps(_mm_unpackhi_epi64(rg, ba));
            _mm_store_ps(buff + 4 * x +  8, _mm_mul_ps(rgba0, rcp));
            _mm_store_ps(buff + 4 * x + 12, _mm_mul_ps(rgba1, rcp));
        }
        convert_float_to_half(d, buff, width * 4);
        d += dpitch;
        sr += spitch;
        sg += spitch;
        sb += spitch;
        if (STACK16) {
            rlsb += spitch;
            glsb += spitch;
            blsb += spitch;
        }
    }
}
#endif


template <bool IS_RGB32>
static void __stdcall
rgb_to_shader_1_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* s = srcp[0] + (height - 1) * spitch;
    uint8_t* d = dstp[0];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (IS_RGB32) {
                memcpy(d, s, width * 4); // same as FlipVertical()
            } else {
                d[4 * x + 0] = s[3 * x + 0];
                d[4 * x + 1] = s[3 * x + 1];
                d[4 * x + 2] = s[3 * x + 2];
                d[4 * x + 3] = 0;
            }
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
            d[8 * x + 0] = 0;
            d[8 * x + 1] = s[step * x + 2];
            d[8 * x + 2] = 0;
            d[8 * x + 3] = s[step * x + 1];
            d[8 * x + 4] = 0;
            d[8 * x + 5] = s[step * x + 0];
            d[8 * x + 6] = 0;
            d[8 * x + 7] = IS_RGB32 ? s[4 * x + 3] : 0;
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
    const __m128i order = _mm_setr_epi8(2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 4) {
            __m128i t = _mm_shuffle_epi8(load(s + 4 * x), order);
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
            d16[4 * x + 0] = lut[s[step * x + 2]];
            d16[4 * x + 1] = lut[s[step * x + 1]];
            d16[4 * x + 2] = lut[s[step * x + 0]];
            d16[4 * x + 3] = IS_RGB32 ? lut[s[4 * x + 3]] : 0;
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
    const __m128i order = _mm_setr_epi8(2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15);

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


convert_shader_t get_to_shader_packed(int precision, int pix_type, bool stack16, arch_t& arch)
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
    if (pix_type == rgb32 && precision == 1) {
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

