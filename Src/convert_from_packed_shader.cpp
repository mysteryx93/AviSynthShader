#include <algorithm>
#include <map>
#include <tuple>
#include "ConvertShader.h"


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
            d[step * x + 0] = std::min((s[8 * x + 0] >> 7) + s[8 * x + 1], 255);
            d[step * x + 1] = std::min((s[8 * x + 2] >> 7) + s[8 * x + 3], 255);
            d[step * x + 2] = std::min((s[8 * x + 4] >> 7) + s[8 * x + 5], 255);
            if (IS_RGB32) {
                d[4 * x + 3] = std::min((s[8 * x + 6] >> 7) + s[8 * x + 7], 255);
            }
        }
        d += dpitch;
        s -= spitch;
    }
}


static void __stdcall
shader_to_rgb32_2_sse2(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* s = srcp[0] + (height - 1) * spitch;
    uint8_t* d = dstp[0];

    const __m128i mask = _mm_set1_epi16(0x0080);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width * 4; x += 16) {
            __m128i d0 = load(s + 2 * x);
            __m128i d1 = load(s + 2 * x + 16);
            d0 = _mm_adds_epu8(_mm_srli_epi16(d0, 8), _mm_srli_epi16(_mm_and_si128(d0, mask), 7));
            d1 = _mm_adds_epu8(_mm_srli_epi16(d1, 8), _mm_srli_epi16(_mm_and_si128(d1, mask), 7));
            d0 = _mm_packus_epi16(d0, d1);
            stream(d + x, d0);
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
            d[step * x + 0] = static_cast<uint8_t>(lut[s16[4 * x + 0]]);
            d[step * x + 1] = static_cast<uint8_t>(lut[s16[4 * x + 1]]);
            d[step * x + 2] = static_cast<uint8_t>(lut[s16[4 * x + 2]]);
            if (IS_RGB32) {
                d[4 * x + 3] = static_cast<uint8_t>(lut[s16[4 * x + 3]]);
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

    for (int y = 0; y < height; ++y) {
        convert_half_to_float(buff, s, width * 4);
        for (int x = 0; x < width * 4; x += 16) {
            __m128i d0 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + x + 0)));
            __m128i d1 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + x + 4)));
            __m128i d2 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + x + 8)));
            __m128i d3 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + x + 12)));
            d0 = _mm_packus_epi16(_mm_packs_epi32(d0, d1), _mm_packs_epi32(d2, d3));
            stream(d + x, d0);
        }
        d += dpitch;
        s -= spitch;
    }
}
#endif


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
            dv[x] = s[4 * x + 0];
            du[x] = s[4 * x + 1];
            dy[x] = s[4 * x + 2];
        }
        dy += dpitch;
        du += dpitch;
        dv += dpitch;
        s += spitch;
    }
}


static __forceinline void
unpack8_x4_x4(__m128i& a, __m128i& b, __m128i& c, __m128i& d)
{
    __m128i a0 = _mm_unpacklo_epi8(a, c);
    __m128i b0 = _mm_unpackhi_epi8(a, c);
    __m128i c0 = _mm_unpacklo_epi8(b, d);
    __m128i d0 = _mm_unpackhi_epi8(b, d);

    __m128i a1 = _mm_unpacklo_epi8(a0, c0);
    __m128i b1 = _mm_unpackhi_epi8(a0, c0);
    __m128i c1 = _mm_unpacklo_epi8(b0, d0);
    __m128i d1 = _mm_unpackhi_epi8(b0, d0);

    __m128i a2 = _mm_unpacklo_epi8(a1, c1);
    __m128i b2 = _mm_unpackhi_epi8(a1, c1);
    __m128i c2 = _mm_unpacklo_epi8(b1, d1);
    __m128i d2 = _mm_unpackhi_epi8(b1, d1);

    a = _mm_unpacklo_epi8(a2, c2);
    b = _mm_unpackhi_epi8(a2, c2);
    c = _mm_unpacklo_epi8(b2, d2);
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
        for (int x = 0; x < width; x += 16) {
            __m128i s0 = load(s + 4 * x + 0);
            __m128i s1 = load(s + 4 * x + 16);
            __m128i s2 = load(s + 4 * x + 32);
            __m128i s3 = load(s + 4 * x + 48);

            unpack8_x4_x4(s0, s1, s2, s3);

            stream(dv + x, s0);
            stream(du + x, s1);
            stream(dy + x, s2);
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
                dv[x] = std::min((s[8 * x + 0] >> 7) + s[8 * x + 1], 255);
                du[x] = std::min((s[8 * x + 2] >> 7) + s[8 * x + 3], 255);
                dy[x] = std::min((s[8 * x + 4] >> 7) + s[8 * x + 5], 255);
            } else {
                lv[x] = s[8 * x + 0];
                dv[x] = s[8 * x + 1];
                lu[x] = s[8 * x + 2];
                du[x] = s[8 * x + 3];
                ly[x] = s[8 * x + 4];
                dy[x] = s[8 * x + 5];
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


static __forceinline void
unpack16_x4_x3(__m128i& a, __m128i& b, __m128i& c, __m128i& d)
{
    __m128i a0 = _mm_unpacklo_epi16(a, c);
    __m128i b0 = _mm_unpackhi_epi16(a, c);
    __m128i c0 = _mm_unpacklo_epi16(b, d);
    __m128i d0 = _mm_unpackhi_epi16(b, d);

    __m128i a1 = _mm_unpacklo_epi16(a0, c0);
    __m128i b1 = _mm_unpackhi_epi16(a0, c0);
    __m128i c1 = _mm_unpacklo_epi16(b0, d0);
    __m128i d1 = _mm_unpackhi_epi16(b0, d0);

    a = _mm_unpacklo_epi16(a1, c1);
    b = _mm_unpackhi_epi16(a1, c1);
    c = _mm_unpacklo_epi16(b1, d1);
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
    const __m128i mask = _mm_set1_epi16(0x00FF);
    if (STACK16) {
        ly = dy + height * dpitch;
        lu = du + height * dpitch;
        lv = dv + height * dpitch;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 8) {
            __m128i s0 = load(s + 8 * x + 0);
            __m128i s1 = load(s + 8 * x + 16);
            __m128i s2 = load(s + 8 * x + 32);
            __m128i s3 = load(s + 8 * x + 48);

            unpack16_x4_x3(s0, s1, s2, s3); // s0 - v, s1 - u, s2 - y

            if (!STACK16) {
                s0 = _mm_adds_epu8(_mm_srli_epi16(s0, 8), _mm_srli_epi16(_mm_and_si128(s0, mask), 7));
                storel(dv + x, _mm_packus_epi16(s0, s0));
                s1 = _mm_adds_epu8(_mm_srli_epi16(s1, 8), _mm_srli_epi16(_mm_and_si128(s1, mask), 7));
                storel(du + x, _mm_packus_epi16(s1, s1));
                s2 = _mm_adds_epu8(_mm_srli_epi16(s2, 8), _mm_srli_epi16(_mm_and_si128(s2, mask), 7));
                storel(dy + x, _mm_packus_epi16(s2, s2));
            } else {
                storel(dv + x, _mm_packus_epi16(_mm_srli_epi16(s0, 8), s0));
                storel(lv + x, _mm_packus_epi16(_mm_and_si128(s0, mask), s0));
                storel(du + x, _mm_packus_epi16(_mm_srli_epi16(s1, 8), s1));
                storel(lu + x, _mm_packus_epi16(_mm_and_si128(s1, mask), s1));
                storel(dy + x, _mm_packus_epi16(_mm_srli_epi16(s2, 8), s2));
                storel(ly + x, _mm_packus_epi16(_mm_and_si128(s2, mask), s2));
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
            if (!STACK16) {
                dv[x] = static_cast<uint8_t>(lut[s16[4 * x + 0]]);
                du[x] = static_cast<uint8_t>(lut[s16[4 * x + 1]]);
                dy[x] = static_cast<uint8_t>(lut[s16[4 * x + 2]]);
            } else {
                union dst16 {
                    uint16_t v16;
                    uint8_t v8[2];
                };
                dst16 dsty, dstu, dstv;

                dstv.v16 = lut[s16[4 * x + 0]];
                dstu.v16 = lut[s16[4 * x + 1]];
                dsty.v16 = lut[s16[4 * x + 2]];
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
            __m128i s0 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + 4 * x +  0))); // v0,u0,y0,a0
            __m128i s1 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + 4 * x +  4))); // v1,u1,y1,a1
            __m128i s2 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + 4 * x +  8))); // v2,u2,y2,a2
            __m128i s3 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + 4 * x + 12))); // v3,u3,y3,a3
            s0 = _mm_or_si128(s0, _mm_slli_epi32(s1, 16)); //v0,v1,u0,u1,y0,y1,a0,a1
            s1 = _mm_or_si128(s2, _mm_slli_epi32(s3, 16)); //v2,v3,u2,u3,y2,y3,a2,a3
            __m128i vu = _mm_unpacklo_epi32(s0, s1); // v0,v1,v2,v3,u0,u1,u2,u3
            __m128i ya = _mm_unpackhi_epi32(s0, s1); // y0,y1,y2,y3,a0,a1,a2,a3
            if (!STACK16) {
                vu = _mm_packus_epi16(vu, vu);
                ya = _mm_packus_epi16(ya, ya);
                *(reinterpret_cast<int32_t*>(dv + x)) = _mm_cvtsi128_si32(vu);
                *(reinterpret_cast<int32_t*>(du + x)) = _mm_cvtsi128_si32(_mm_srli_si128(vu, 4));
                *(reinterpret_cast<int32_t*>(dy + x)) = _mm_cvtsi128_si32(ya);
            } else {
                __m128i yamsb = _mm_packus_epi16(_mm_srli_epi16(ya, 8), ya);
                __m128i yalsb = _mm_packus_epi16(_mm_and_si128(ya, mask16), ya);
                *(reinterpret_cast<int32_t*>(dy + x)) = _mm_cvtsi128_si32(yamsb);
                *(reinterpret_cast<int32_t*>(ly + x)) = _mm_cvtsi128_si32(yalsb);

                __m128i vumsb = _mm_packus_epi16(_mm_srli_epi16(vu, 8), vu);
                __m128i vulsb = _mm_packus_epi16(_mm_and_si128(vu, mask16), vu);
                *(reinterpret_cast<int32_t*>(dv + x)) = _mm_cvtsi128_si32(vumsb);
                *(reinterpret_cast<int32_t*>(du + x)) = _mm_cvtsi128_si32(_mm_srli_si128(vumsb, 4));
                *(reinterpret_cast<int32_t*>(lv + x)) = _mm_cvtsi128_si32(vulsb);
                *(reinterpret_cast<int32_t*>(lu + x)) = _mm_cvtsi128_si32(_mm_srli_si128(vulsb, 4));
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




convert_shader_t get_from_shader_packed(int precision, int pix_type, bool stack16, arch_t arch)
{
    using std::make_tuple;

    constexpr int rgb24 = VideoInfo::CS_BGR24;
    constexpr int rgb32 = VideoInfo::CS_BGR32;
    constexpr int yv24 = VideoInfo::CS_YV24;

    std::map<std::tuple<int, int, bool, arch_t>, convert_shader_t> func;

    func[make_tuple(1, yv24, false, NO_SIMD)] = shader_to_yuv_1_c;
    func[make_tuple(1, yv24, false, USE_SSE2)] = shader_to_yuv_1_sse2;

    func[make_tuple(2, yv24, false, NO_SIMD)] = shader_to_yuv_2_c<false>;
    func[make_tuple(2, yv24, true, NO_SIMD)] = shader_to_yuv_2_c<true>;
    func[make_tuple(2, yv24, false, USE_SSE2)] = shader_to_yuv_2_sse2<false>;
    func[make_tuple(2, yv24, true, USE_SSE2)] = shader_to_yuv_2_sse2<true>;

    func[make_tuple(2, rgb24, false, NO_SIMD)] = shader_to_rgb_2_c<false>;
    func[make_tuple(2, rgb32, false, NO_SIMD)] = shader_to_rgb_2_c<true>;
    func[make_tuple(2, rgb32, false, USE_SSE2)] = shader_to_rgb32_2_sse2;

    func[make_tuple(3, yv24, false, NO_SIMD)] = shader_to_yuv_3_c<false>;
    func[make_tuple(3, yv24, true, NO_SIMD)] = shader_to_yuv_3_c<true>;

    func[make_tuple(3, rgb24, false, NO_SIMD)] = shader_to_rgb_3_c<false>;
    func[make_tuple(3, rgb32, false, NO_SIMD)] = shader_to_rgb_3_c<true>;

#if defined(__AVX__)
    func[make_tuple(3, yv24, false, USE_F16C)] = shader_to_yuv_3_f16c<false>;
    func[make_tuple(3, yv24, true, USE_F16C)] = shader_to_yuv_3_f16c<true>;
    func[make_tuple(3, rgb32, false, USE_F16C)] = shader_to_rgb32_3_f16c;
#endif


    return func[make_tuple(precision, pix_type, stack16, arch)];
}

