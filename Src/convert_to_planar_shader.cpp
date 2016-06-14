#include <map>
#include <tuple>

#include "ConvertShader.h"



template <bool STACK16>
static void __stdcall
yuv_to_shader_2_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    for (int p = 0; p < 3; ++p) {
        const uint8_t* s = srcp[p];
        const uint8_t* lsb = s + height * spitch;
        uint8_t* d = dstp[p];

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                d[2 * x + 0] = STACK16 ? lsb[x] : 0;
                d[2 * x + 1] = s[x];
            }
            s += spitch;
            d += dpitch;
            if (STACK16) {
                lsb += spitch;
            }
        }
    }
}


template <bool STACK16>
static void __stdcall
yuv_to_shader_2_sse2(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const __m128i zero = _mm_setzero_si128();

    for (int p = 0; p < 3; ++p) {
        const uint8_t* s = srcp[p];
        const uint8_t* lsb = s + height * spitch;
        uint8_t* d = dstp[p];

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; x += 16) {
                __m128i msbx = load(s + x);
                __m128i lsbx = STACK16 ? load(lsb + x) : zero;
                __m128i d0 = _mm_unpacklo_epi8(lsbx, msbx);
                __m128i d1 = _mm_unpackhi_epi8(lsbx, msbx);
                stream(d + 2 * x, d0);
                stream(d + 2 * x + 16, d1);
            }
            s += spitch;
            d += dpitch;
            if (STACK16) {
                lsb += spitch;
            }
        }
    }
}


template <bool STACK16>
static void __stdcall
yuv_to_shader_3_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _lut) noexcept
{
    const uint16_t* lut = reinterpret_cast<const uint16_t*>(_lut);

    for (int p = 0; p < 3; ++p) {
        const uint8_t* s = srcp[p];
        const uint8_t* lsb = s + height * spitch;
        uint8_t* d = dstp[p];

        for (int y = 0; y < height; ++y) {
            uint16_t* d16 = reinterpret_cast<uint16_t*>(d);
            for (int x = 0; x < width; ++x) {
                if (!STACK16) {
                    d16[x] = lut[s[x]];
                } else {
                    d16[x] = lut[(s[x] << 8) | lsb[x]];
                }
            }
            if (STACK16) {
                lsb += spitch;
            }
            s += spitch;
            d += dpitch;
        }
    }
}


#if defined(__AVX__)
template <bool STACK16>
static void __stdcall
yuv_to_shader_3_f16c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _buff) noexcept
{
    const __m128i zero = _mm_setzero_si128();
    const __m128 rcp = _mm_set1_ps(1.0f / (STACK16 ? 65535 : 255));

    float* buff = reinterpret_cast<float*>(_buff);

    for (int p = 0; p < 3; ++p) {
        const uint8_t* s = srcp[p];
        const uint8_t* lsb = s + height * spitch;
        uint8_t* d = dstp[p];

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; x += 16) {
                __m128i msbx = load(s + x);
                __m128i d0, lsbx;
                if (!STACK16) {
                    d0 = _mm_unpacklo_epi8(msbx, zero);
                } else {
                    lsbx = load(lsb + x);
                    d0 = _mm_unpacklo_epi8(lsbx, msbx);
                }
                __m128 f0 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(d0, zero));
                _mm_store_ps(buff + x + 0, _mm_mul_ps(rcp, f0));
                f0 = _mm_cvtepi32_ps(_mm_unpackhi_epi16(d0, zero));
                _mm_store_ps(buff + x + 4, _mm_mul_ps(rcp, f0));

                if (!STACK16) {
                    d0 = _mm_unpackhi_epi8(msbx, zero);
                } else {
                    d0 = _mm_unpackhi_epi8(lsbx, msbx);
                }
                f0 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(d0, zero));
                _mm_store_ps(buff + x + 8, _mm_mul_ps(rcp, f0));
                f0 = _mm_cvtepi32_ps(_mm_unpackhi_epi16(d0, zero));
                _mm_store_ps(buff + x + 12, _mm_mul_ps(rcp, f0));
            }
            convert_float_to_half(d, buff, width);
            s += spitch;
            d += dpitch;
            if (STACK16) {
                lsb += spitch;
            }
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
    // map r to Y, g to U, b to V
    uint8_t* dr = dstp[0];
    uint8_t* dg = dstp[1];
    uint8_t* db = dstp[2];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            db[x] = s[step * x + 0];
            dg[x] = s[step * x + 1];
            dr[x] = s[step * x + 2];
        }
        s -= spitch;
        dr += dpitch;
        dg += dpitch;
        db += dpitch;
    }
}


static void __stdcall
rgb32_to_shader_1_sse2(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* s = srcp[0] + (height - 1) * spitch;
    // map r to Y, g to U, b to V
    uint8_t* dr = dstp[0];
    uint8_t* dg = dstp[1];
    uint8_t* db = dstp[2];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 8) {
            __m128i s0 = loadl(s + 4 * x);      //b0,g0,r0,a0,b1,g1,r1,a1
            __m128i s1 = loadl(s + 4 * x + 8);  //b2,g2,r2,a2,b3,g3,r3,a3
            __m128i s2 = loadl(s + 4 * x + 16); //b4,g4,r4,a4,b5,g5,r5,a5
            __m128i s3 = loadl(s + 4 * x + 24); //b6,g6,r6,a6,b7,g7,r7,a7
            s0 = _mm_unpacklo_epi8(s0, s1);     //b0,b2,g0,g2,r0,r2,a0,a2,b1,b3,g1,g3,r1,r3,a1,a3
            s1 = _mm_unpacklo_epi8(s2, s3);     //b4,b6,g4,g6,r4,r6,a4,a6,b5,b7,g5,g7,r5,r7,a5,a7
            s2 = _mm_unpacklo_epi16(s0, s1);    //b0,b2,b4,b6,g0,g2,g4,g6,r0,r2,r4,r6,a0,a2,a4,a6
            s3 = _mm_unpackhi_epi16(s0, s1);    //b1,b3,b5,b7,g1,g3,g5,g7,r1,r3,r5,r7,a1,a3,a5,a7
            s0 = _mm_unpacklo_epi8(s2, s3);     //b0,b1,b2,b3,b4,b5,b6,b7,g0,g1,g2,g3,g4,g5,g6,g7
            s1 = _mm_unpackhi_epi8(s2, s3);     //r0,r1,r2,r3,r4,r5,r6,r7,aaaaaaaa
            storel(db + x, s0);
            storel(dg + x, _mm_srli_si128(s0, 8));
            storel(dr + x, s1);
        }
        s -= spitch;
        dr += dpitch;
        dg += dpitch;
        db += dpitch;
    }
}


template <bool IS_RGB32>
static void __stdcall
rgb_to_shader_2_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    constexpr size_t step = IS_RGB32 ? 4 : 3;

    const uint8_t* s = srcp[0] + (height - 1) * spitch;
    // map r to Y, g to U, b to V
    uint8_t* dr = dstp[0];
    uint8_t* dg = dstp[1];
    uint8_t* db = dstp[2];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            db[2 * x + 0] = 0;
            db[2 * x + 1] = s[step * x + 0];
            dg[2 * x + 0] = 0;
            dg[2 * x + 1] = s[step * x + 1];
            dr[2 * x + 0] = 0;
            dr[2 * x + 1] = s[step * x + 2];
        }
        s -= spitch;
        dr += dpitch;
        dg += dpitch;
        db += dpitch;
    }
}


static void __stdcall
rgb32_to_shader_2_sse2(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* s = srcp[0] + (height - 1) * spitch;
    // map r to Y, g to U, b to V
    uint8_t* dr = dstp[0];
    uint8_t* dg = dstp[1];
    uint8_t* db = dstp[2];

    const __m128i zero = _mm_setzero_si128();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 8) {
            __m128i s0 = loadl(s + 4 * x + 0);
            __m128i s1 = loadl(s + 4 * x + 8);
            __m128i s2 = loadl(s + 4 * x + 16);
            __m128i s3 = loadl(s + 4 * x + 24);
            s0 = _mm_unpacklo_epi8(s0, s1);
            s1 = _mm_unpacklo_epi8(s2, s3);
            s2 = _mm_unpacklo_epi16(s0, s1);
            s3 = _mm_unpackhi_epi16(s0, s1);
            s0 = _mm_unpacklo_epi8(s2, s3);
            s1 = _mm_unpackhi_epi8(s2, s3);
            stream(db + 2 * x, _mm_unpacklo_epi8(zero, s0));
            stream(dg + 2 * x, _mm_unpackhi_epi8(zero, s0));
            stream(dr + 2 * x, _mm_unpacklo_epi8(zero, s1));
        }
        s -= spitch;
        dr += dpitch;
        dg += dpitch;
        db += dpitch;
    }
}


template <bool IS_RGB32>
static void __stdcall
rgb_to_shader_3_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _lut) noexcept
{
    constexpr size_t step = IS_RGB32 ? 4 : 3;

    const uint8_t* s = srcp[0] + (height - 1) * spitch;
    // map r to Y, g to U, b to V
    uint8_t* dr = dstp[0];
    uint8_t* dg = dstp[1];
    uint8_t* db = dstp[2];

    const uint16_t* lut = reinterpret_cast<const uint16_t*>(_lut);

    for (int y = 0; y < height; ++y) {
        uint16_t* dr16 = reinterpret_cast<uint16_t*>(dr);
        uint16_t* dg16 = reinterpret_cast<uint16_t*>(dg);
        uint16_t* db16 = reinterpret_cast<uint16_t*>(db);
        for (int x = 0; x < width; ++x) {
            db16[x] = lut[s[step * x + 0]];
            dg16[x] = lut[s[step * x + 1]];
            dr16[x] = lut[s[step * x + 2]];
        }
        s -= spitch;
        dr += dpitch;
        dg += dpitch;
        db += dpitch;
    }
}


#if defined(__AVX__)
static void __stdcall
rgb32_to_shader_3_f16c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* buff) noexcept
{
    const uint8_t* s = srcp[0] + (height - 1) * spitch;
    // map r to Y, g to U, b to V
    uint8_t* dr = dstp[0];
    uint8_t* dg = dstp[1];
    uint8_t* db = dstp[2];

    float* bb = reinterpret_cast<float*>(buff);
    float* bg = bb + ((width + 7) & ~7); // must be aligned 32 bytes
    float* br = bg + ((width + 7) & ~7); // must be aligned 32 bytes

    const __m128 rcp = _mm_set1_ps(1.0f / 255);
    const __m128i zero = _mm_setzero_si128();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 8) {
            __m128i s0 = loadl(s + 4 * x + 0);
            __m128i s1 = loadl(s + 4 * x + 8);
            __m128i s2 = loadl(s + 4 * x + 16);
            __m128i s3 = loadl(s + 4 * x + 24);
            s0 = _mm_unpacklo_epi8(s0, s1);
            s1 = _mm_unpacklo_epi8(s2, s3);
            s2 = _mm_unpacklo_epi16(s0, s1);
            s3 = _mm_unpackhi_epi16(s0, s1);
            s0 = _mm_unpacklo_epi8(s2, s3);
            s1 = _mm_unpackhi_epi8(s2, s3);

            __m128i r = _mm_unpacklo_epi8(s1, zero);
            s3 = _mm_unpacklo_epi16(r, zero);
            _mm_store_ps(br + x + 0, _mm_mul_ps(rcp, _mm_cvtepi32_ps(s3)));
            s3 = _mm_unpackhi_epi16(r, zero);
            _mm_store_ps(br + x + 4, _mm_mul_ps(rcp, _mm_cvtepi32_ps(s3)));

            __m128i b = _mm_unpacklo_epi8(s0, zero);
            __m128i g = _mm_unpackhi_epi8(s0, zero);

            s3 = _mm_unpacklo_epi16(b, zero);
            _mm_store_ps(bb + x + 0, _mm_mul_ps(rcp, _mm_cvtepi32_ps(s3)));
            s3 = _mm_unpackhi_epi16(b, zero);
            _mm_store_ps(bb + x + 4, _mm_mul_ps(rcp, _mm_cvtepi32_ps(s3)));

            s3 = _mm_unpacklo_epi16(g, zero);
            _mm_store_ps(bg + x + 0, _mm_mul_ps(rcp, _mm_cvtepi32_ps(s3)));
            s3 = _mm_unpackhi_epi16(g, zero);
            _mm_store_ps(bg + x + 4, _mm_mul_ps(rcp, _mm_cvtepi32_ps(s3)));
        }
        convert_float_to_half(dr, br, width);
        convert_float_to_half(dg, bg, width);
        convert_float_to_half(db, bb, width);
        s -= spitch;
        dr += dpitch;
        dg += dpitch;
        db += dpitch;
    }
}
#endif


convert_shader_t get_to_shader_planar(int precision, int pix_type, bool stack16, arch_t arch)
{
    using std::make_tuple;
    constexpr int rgb24 = VideoInfo::CS_BGR24;
    constexpr int rgb32 = VideoInfo::CS_BGR32;
    constexpr int yv24 = VideoInfo::CS_YV24;

    std::map<std::tuple<int, int, bool, arch_t>, convert_shader_t> func;

    func[make_tuple(1, rgb24, false, NO_SIMD)] = rgb_to_shader_1_c<false>;
    func[make_tuple(1, rgb32, false, NO_SIMD)] = rgb_to_shader_1_c<true>;
    func[make_tuple(1, rgb32, false, USE_SSE2)] = rgb32_to_shader_1_sse2;

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

    if (pix_type == rgb24) {
        arch = NO_SIMD;
    }
    if (precision != 3 && arch > USE_SSE2) {
        arch = USE_SSE2;
    }
    if (precision == 3 && arch != USE_F16C) {
        arch = NO_SIMD;
    }

    return func[make_tuple(precision, pix_type, stack16, arch)];
}

