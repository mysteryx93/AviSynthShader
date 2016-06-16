#include <algorithm>
#include <map>
#include <tuple>
#include "ConvertShader.h"



/*
packed shader: color order ARGB
precision1: all samples are uint8_t
precision2: all samples are uint16_t
precision3: all samples are half precision.
map Y to R, U to G, V to B
*/


static void __stdcall
shader_to_yuv_1_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* s = srcp[0];

    uint8_t* dy = dstp[0];
    uint8_t* du = dstp[1];
    uint8_t* dv = dstp[2];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            dy[x] = s[4 * x + 1];
            du[x] = s[4 * x + 2];
            dv[x] = s[4 * x + 3];
        }
        dy += dpitch;
        du += dpitch;
        dv += dpitch;
        s += spitch;
    }
}


static void __stdcall
shader_to_yuv_1_sse2(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* s = srcp[0];

    uint8_t* dy = dstp[0];
    uint8_t* du = dstp[1];
    uint8_t* dv = dstp[2];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 8) {
            __m128i s0 = loadl(s + 4 * x + 0);      // A0,R0,G0,B0,A1,R1,G1,B1
            __m128i s1 = loadl(s + 4 * x + 8);      // A2,R2,G2,B2,A3,R3,G3,B3
            __m128i t0 = _mm_unpacklo_epi8(s0, s1); // A0,A2,R0,R2,G0,G2,B0,B2,A1,A3,R1,R3,G1,G3,B1,B3

            s0 = loadl(s + 4 * x + 16);             // A4,R4,G4,B4,A5,R5,G5,B5
            s1 = loadl(s + 4 * x + 24);             // A6,R6,G6,B6,A7,R7,G7,B7
            __m128i t1 = _mm_unpacklo_epi8(s0, s1); // A4,A6,R4,R6,G4,G6,B4,B6,A5,A7,R5,R7,G5,G7,B5,B7

            s0 = _mm_unpacklo_epi16(t0, t1);        // A0,A2,A4,A6,R0,R2,R4,R6,G0,G2,G4,G6,B0,B2,B4,B6
            s1 = _mm_unpackhi_epi16(t0, t1);        // A1,A3,A5,A7,R1,R3,R5,R7,G1,G3,G5,G7,B1,B3,B5,B7

            t0 = _mm_unpacklo_epi8(s0, s1);         // A0,A1,A2,A3,A4,A5,A6,A7,R0,R1,R2,R3,R4,R5,R6,R7
            t1 = _mm_unpackhi_epi8(s0, s1);         // G0,G1,G2,G3,G4,G5,G6,G7,B0,B1,B2,B3,B4,B5,B6,B7

            storel(dy + x, _mm_srli_si128(t0, 8));
            storel(du + x, t1);
            storel(dv + x, _mm_srli_si128(t1, 8));
        }
        dy += dpitch;
        du += dpitch;
        dv += dpitch;
        s += spitch;
    }
}


template <bool STACK16>
static void __stdcall
shader_to_yuv_2_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* s = srcp[0];

    uint8_t* dy = dstp[0];
    uint8_t* du = dstp[1];
    uint8_t* dv = dstp[2];

    uint8_t *ly, *lu, *lv;
    if (STACK16) {
        ly = dy + height * dpitch;
        lu = du + height * dpitch;
        lv = dv + height * dpitch;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (!STACK16) {
                dy[x] = std::min((s[8 * x + 2] >> 7) + s[8 * x + 3], 255);
                du[x] = std::min((s[8 * x + 4] >> 7) + s[8 * x + 5], 255);
                dv[x] = std::min((s[8 * x + 6] >> 7) + s[8 * x + 7], 255);
            } else {
                ly[x] = s[8 * x + 2];
                dy[x] = s[8 * x + 3];
                lu[x] = s[8 * x + 4];
                du[x] = s[8 * x + 5];
                lv[x] = s[8 * x + 6];
                dv[x] = s[8 * x + 7];
            }
        }
        s += spitch;
        dy += dpitch;
        du += dpitch;
        dv += dpitch;
        if (STACK16) {
            ly += dpitch;
            lu += dpitch;
            lv += dpitch;
        }
    }
}


template <bool STACK16>
static void __stdcall
shader_to_yuv_2_sse2(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* s = srcp[0];

    uint8_t* dy = dstp[0];
    uint8_t* du = dstp[1];
    uint8_t* dv = dstp[2];

    uint8_t *ly, *lu, *lv;
    if (STACK16) {
        ly = dy + height * dpitch;
        lu = du + height * dpitch;
        lv = dv + height * dpitch;
    }

    const __m128i mask = _mm_set1_epi16(0x00FF);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 8) {
            // on 16bits, color order of packed shader is ABGR
            // map R to y, G to u, B to v
            __m128i s0 = load(s + 8 * x + 0);       // A0,R0,G0,B0,A1,R1,G1,B1
            __m128i s1 = load(s + 8 * x + 16);      // A2,R2,G2,B2,A3,R3,G3,B3
            __m128i s2 = load(s + 8 * x + 32);      // A4,R4,G4,B4,A5,R5,G5,B5
            __m128i s3 = load(s + 8 * x + 48);      // A6,R6,G6,B6,A7,R7,G7,B7

            __m128i t0 = _mm_unpacklo_epi16(s0, s2);// A0,A4,R0,R4,G0,G4,B0,B4
            __m128i t1 = _mm_unpackhi_epi16(s0, s2);// A1,A5,R1,R5,G1,G5,B1,B5
            __m128i t2 = _mm_unpacklo_epi16(s1, s3);// A2,A6,R2,R6,G2,G6,B2,B6
            __m128i t3 = _mm_unpackhi_epi16(s1, s3);// A3,A7,R3,R7,G3,G7,B3,B7

            s0 = _mm_unpacklo_epi16(t0, t2);        // A0,A2,A4,A6,R0,R2,R4,R6
            s1 = _mm_unpackhi_epi16(t0, t2);        // G0,G2,G4,G6,B0,B2,B4,B6
            s2 = _mm_unpacklo_epi16(t1, t3);        // A1,A3,A5,A6,R1,R3,R5,R7
            s3 = _mm_unpackhi_epi16(t1, t3);        // G1,G3,G5,G7,B1,B3,B5,B7

            t1 = _mm_unpackhi_epi16(s0, s2);        // R0,R1,R2,R3,R4,R5,R6,R7
            t2 = _mm_unpacklo_epi16(s1, s3);        // G0,G1,G2,G3,G4,G5,G6,G7
            t3 = _mm_unpackhi_epi16(s1, s3);        // B0,B1,B2,B3,B4,B5,B6,B7

            if (!STACK16) {
                s1 = _mm_adds_epu8(_mm_srli_epi16(t1, 8), _mm_srli_epi16(_mm_and_si128(t1, mask), 7));
                storel(dy + x, _mm_packus_epi16(s1, s1));
                s2 = _mm_adds_epu8(_mm_srli_epi16(t2, 8), _mm_srli_epi16(_mm_and_si128(t2, mask), 7));
                storel(du + x, _mm_packus_epi16(s2, s2));
                s3 = _mm_adds_epu8(_mm_srli_epi16(t3, 8), _mm_srli_epi16(_mm_and_si128(t3, mask), 7));
                storel(dv + x, _mm_packus_epi16(s3, s3));
            } else {
                storel(dy + x, _mm_packus_epi16(_mm_srli_epi16(t1, 8), t1));
                storel(ly + x, _mm_packus_epi16(_mm_and_si128(t1, mask), t1));
                storel(du + x, _mm_packus_epi16(_mm_srli_epi16(t2, 8), t2));
                storel(lu + x, _mm_packus_epi16(_mm_and_si128(t2, mask), t2));
                storel(dv + x, _mm_packus_epi16(_mm_srli_epi16(t3, 8), t3));
                storel(lv + x, _mm_packus_epi16(_mm_and_si128(t3, mask), t3));
            }
        }
        s += spitch;
        dy += dpitch;
        du += dpitch;
        dv += dpitch;
        if (STACK16) {
            ly += dpitch;
            lu += dpitch;
            lv += dpitch;
        }
    }
}


template <bool STACK16>
static void __stdcall
shader_to_yuv_3_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _lut) noexcept
{
    const uint8_t* s = srcp[0];

    uint8_t* dy = dstp[0];
    uint8_t* du = dstp[1];
    uint8_t* dv = dstp[2];

    const uint16_t* lut = reinterpret_cast<const uint16_t*>(_lut);

    uint8_t *ly, *lu, *lv;
    if (STACK16) {
        ly = dy + height * dpitch;
        lu = du + height * dpitch;
        lv = dv + height * dpitch;
    }

    for (int y = 0; y < height; ++y) {
        const uint16_t* s16 = reinterpret_cast<const uint16_t*>(s);
        for (int x = 0; x < width; ++x) {
            // on half precision, color order of packed shader is ABGR
            // map R to y, G to u, B to v
            if (!STACK16) {
                dy[x] = static_cast<uint8_t>(lut[s16[4 * x + 1]]);
                du[x] = static_cast<uint8_t>(lut[s16[4 * x + 2]]);
                dv[x] = static_cast<uint8_t>(lut[s16[4 * x + 3]]);
            } else {
                union dst16 {
                    uint16_t v16;
                    uint8_t v8[2];
                };
                dst16 dsty, dstu, dstv;

                dsty.v16 = lut[s16[4 * x + 1]];
                dstu.v16 = lut[s16[4 * x + 2]];
                dstv.v16 = lut[s16[4 * x + 3]];
                ly[x] = dsty.v8[0];
                dy[x] = dsty.v8[1];
                lu[x] = dstu.v8[0];
                du[x] = dstu.v8[1];
                lv[x] = dstv.v8[0];
                dv[x] = dstv.v8[1];
            }
        }
        s += spitch;
        dy += dpitch;
        du += dpitch;
        dv += dpitch;
        if (STACK16) {
            ly += dpitch;
            lu += dpitch;
            lv += dpitch;
        }
    }
}


#if defined(__AVX__)
template <bool STACK16>
static void __stdcall
shader_to_yuv_3_f16c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _buff) noexcept
{
    const uint8_t* s = srcp[0];

    uint8_t* dy = dstp[0];
    uint8_t* du = dstp[1];
    uint8_t* dv = dstp[2];

    float * buff = reinterpret_cast<float*>(_buff);

    uint8_t *ly, *lu, *lv;
    const __m128 coef = _mm_set1_ps(STACK16 ? 65535.0f : 255.0f);
    __m128i mask16;

    if (STACK16) {
        ly = dy + height * dpitch;
        lu = du + height * dpitch;
        lv = dv + height * dpitch;
        mask16 = _mm_set1_epi16(0x00FF);
    }

    for (int y = 0; y < height; ++y) {
        convert_half_to_float(buff, s, width * 4);
        for (int x = 0; x < width; x += 4) {
            // on half precision, color order of packed shader is ABGR
            // map R to y, G to u, B to v
            __m128i s0 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + 4 * x +  0))); // A0,R0,G0,B0
            __m128i s1 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + 4 * x +  4))); // A1,R1,G1,B1
            __m128i s2 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + 4 * x +  8))); // A2,R2,G2,B2
            __m128i s3 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + 4 * x + 12))); // A3,R3,G3,B3
            s0 = _mm_or_si128(s0, _mm_slli_epi32(s1, 16)); //A0,A1,R0,R1,G0,G1,B0,B1
            s1 = _mm_or_si128(s2, _mm_slli_epi32(s3, 16)); //A2,A3,R2,R3,G2,G3,B2,B3
            s2 = _mm_unpacklo_epi32(s0, s1); // A0,A1,A2,A3,R0,R1,R2,R3
            s3 = _mm_unpackhi_epi32(s0, s1); // G0,G1,G2,G3,B0,B1,B2,B3
            if (!STACK16) {
                s0 = _mm_packus_epi16(s2, s2);
                s1 = _mm_packus_epi16(s3, s3);
                *(reinterpret_cast<int32_t*>(dy + x)) = _mm_cvtsi128_si32(_mm_srli_si128(s0, 4));
                *(reinterpret_cast<int32_t*>(du + x)) = _mm_cvtsi128_si32(s1);
                *(reinterpret_cast<int32_t*>(dv + x)) = _mm_cvtsi128_si32(_mm_srli_si128(s1, 4));
            } else {
                __m128i armsb = _mm_packus_epi16(_mm_srli_epi16(s2, 8), s2);
                __m128i arlsb = _mm_packus_epi16(_mm_and_si128(s2, mask16), s2);
                *(reinterpret_cast<int32_t*>(dy + x)) = _mm_cvtsi128_si32(_mm_srli_si128(armsb, 4));
                *(reinterpret_cast<int32_t*>(ly + x)) = _mm_cvtsi128_si32(_mm_srli_si128(arlsb, 4));

                __m128i gbmsb = _mm_packus_epi16(_mm_srli_epi16(s3, 8), s3);
                __m128i gblsb = _mm_packus_epi16(_mm_and_si128(s3, mask16), s3);
                *(reinterpret_cast<int32_t*>(du + x)) = _mm_cvtsi128_si32(gbmsb);
                *(reinterpret_cast<int32_t*>(dv + x)) = _mm_cvtsi128_si32(_mm_srli_si128(gbmsb, 4));
                *(reinterpret_cast<int32_t*>(lu + x)) = _mm_cvtsi128_si32(gblsb);
                *(reinterpret_cast<int32_t*>(lv + x)) = _mm_cvtsi128_si32(_mm_srli_si128(gblsb, 4));
            }
        }
        s += spitch;
        dy += dpitch;
        du += dpitch;
        dv += dpitch;
        if (STACK16) {
            ly += dpitch;
            lu += dpitch;
            lv += dpitch;
        }
    }
}
#endif


template <bool IS_RGB32>
static void __stdcall
shader_to_rgb_1_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    constexpr size_t step = IS_RGB32 ? 4 : 3;

    const uint8_t* s = srcp[0] + (height - 1) * spitch;
    uint8_t* d = dstp[0];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            d[step * x + 0] = s[4 * x + 3];
            d[step * x + 1] = s[4 * x + 2];
            d[step * x + 2] = s[4 * x + 1];
            if (IS_RGB32) {
                d[4 * x + 3] = s[4 * x + 0];
            }
        }
        d += dpitch;
        s -= spitch;
    }
}


static void __stdcall
shader_to_rgb32_1_ssse3(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* s = srcp[0] + (height - 1) * spitch;
    uint8_t* d = dstp[0];
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
shader_to_rgb_2_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    constexpr size_t step = IS_RGB32 ? 4 : 3;

    const uint8_t* s = srcp[0] + (height - 1) * spitch;
    uint8_t* d = dstp[0];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            d[step * x + 0] = std::min((s[8 * x + 6] >> 7) + s[8 * x + 7], 255);
            d[step * x + 1] = std::min((s[8 * x + 4] >> 7) + s[8 * x + 5], 255);
            d[step * x + 2] = std::min((s[8 * x + 2] >> 7) + s[8 * x + 3], 255);
            if (IS_RGB32) {
                d[4 * x + 3] = std::min((s[8 * x + 0] >> 7) + s[8 * x + 1], 255);
            }
        }
        d += dpitch;
        s -= spitch;
    }
}


static void __stdcall
shader_to_rgb32_2_ssse3(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* s = srcp[0] + (height - 1) * spitch;
    uint8_t* d = dstp[0];

    const __m128i mask = _mm_set1_epi16(0x00FF);
    const __m128i order = _mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 4) {
            __m128i d0 = load(s + 8 * x);
            __m128i d1 = load(s + 8 * x + 16);
            d0 = _mm_adds_epu8(_mm_srli_epi16(d0, 8), _mm_srli_epi16(_mm_and_si128(d0, mask), 7));
            d1 = _mm_adds_epu8(_mm_srli_epi16(d1, 8), _mm_srli_epi16(_mm_and_si128(d1, mask), 7));
            d0 = _mm_shuffle_epi8(_mm_packus_epi16(d0, d1), order);
            stream(d + 4 * x, d0);
        }
        d += dpitch;
        s -= spitch;
    }
}



template <bool IS_RGB32>
static void __stdcall
shader_to_rgb_3_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _lut) noexcept
{
    constexpr size_t step = IS_RGB32 ? 4 : 3;

    const uint8_t* s = srcp[0] + (height - 1) * spitch;
    uint8_t* d = dstp[0];

    const uint16_t* lut = reinterpret_cast<const uint16_t*>(_lut);

    for (int y = 0; y < height; ++y) {
        const uint16_t* s16 = reinterpret_cast<const uint16_t*>(s);
        for (int x = 0; x < width; ++x) {
            d[step * x + 0] = static_cast<uint8_t>(lut[s16[4 * x + 3]]);
            d[step * x + 1] = static_cast<uint8_t>(lut[s16[4 * x + 2]]);
            d[step * x + 2] = static_cast<uint8_t>(lut[s16[4 * x + 1]]);
            if (IS_RGB32) {
                d[4 * x + 3] = static_cast<uint8_t>(lut[s16[4 * x + 0]]);
            }
        }
        d += dpitch;
        s -= spitch;
    }
}


#if defined(__AVX__)
static void __stdcall
shader_to_rgb32_3_f16c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _buff) noexcept
{
    const uint8_t* s = srcp[0] + (height - 1) * spitch;
    uint8_t* d = dstp[0];
    float* buff = reinterpret_cast<float*>(_buff);

    const __m128 coef = _mm_set1_ps(255.0f);
    const __m128i order = _mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);

    for (int y = 0; y < height; ++y) {
        convert_half_to_float(buff, s, width * 4);
        for (int x = 0; x < width; x += 4) {
            __m128i d0 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + 4 * x + 0)));
            __m128i d1 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + 4 * x + 4)));
            __m128i d2 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + 4 * x + 8)));
            __m128i d3 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + 4 * x + 12)));
            d0 = _mm_packus_epi16(_mm_packs_epi32(d0, d1), _mm_packs_epi32(d2, d3));
            stream(d + 4 * x, _mm_shuffle_epi8(d0, order));
        }
        d += dpitch;
        s -= spitch;
    }
}
#endif





convert_shader_t get_from_shader_packed(int precision, int pix_type, bool stack16, arch_t& arch)
{
    using std::make_tuple;

    constexpr int rgb24 = VideoInfo::CS_BGR24;
    constexpr int rgb32 = VideoInfo::CS_BGR32;
    constexpr int yv24 = VideoInfo::CS_YV24;

    std::map<std::tuple<int, int, bool, arch_t>, convert_shader_t> func;

    func[make_tuple(1, yv24, false, NO_SIMD)] = shader_to_yuv_1_c;
    func[make_tuple(1, yv24, false, USE_SSE2)] = shader_to_yuv_1_sse2;

    func[make_tuple(1, rgb24, false, NO_SIMD)] = shader_to_rgb_1_c<false>;
    func[make_tuple(1, rgb32, false, NO_SIMD)] = shader_to_rgb_1_c<true>;
    func[make_tuple(1, rgb32, false, USE_SSSE3)] = shader_to_rgb32_1_ssse3;

    func[make_tuple(2, yv24, false, NO_SIMD)] = shader_to_yuv_2_c<false>;
    func[make_tuple(2, yv24, true, NO_SIMD)] = shader_to_yuv_2_c<true>;
    func[make_tuple(2, yv24, false, USE_SSE2)] = shader_to_yuv_2_sse2<false>;
    func[make_tuple(2, yv24, true, USE_SSE2)] = shader_to_yuv_2_sse2<true>;

    func[make_tuple(2, rgb24, false, NO_SIMD)] = shader_to_rgb_2_c<false>;
    func[make_tuple(2, rgb32, false, NO_SIMD)] = shader_to_rgb_2_c<true>;
    func[make_tuple(2, rgb32, false, USE_SSSE3)] = shader_to_rgb32_2_ssse3;

    func[make_tuple(3, yv24, false, NO_SIMD)] = shader_to_yuv_3_c<false>;
    func[make_tuple(3, yv24, true, NO_SIMD)] = shader_to_yuv_3_c<true>;

    func[make_tuple(3, rgb24, false, NO_SIMD)] = shader_to_rgb_3_c<false>;
    func[make_tuple(3, rgb32, false, NO_SIMD)] = shader_to_rgb_3_c<true>;

#if defined(__AVX__)
    func[make_tuple(3, yv24, false, USE_F16C)] = shader_to_yuv_3_f16c<false>;
    func[make_tuple(3, yv24, true, USE_F16C)] = shader_to_yuv_3_f16c<true>;
    func[make_tuple(3, rgb32, false, USE_F16C)] = shader_to_rgb32_3_f16c;
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

