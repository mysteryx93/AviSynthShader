#include <algorithm>
#include <map>
#include <tuple>
#include "ConvertShader.h"


template <bool STACK16>
static void __stdcall
shader_to_yuv_2_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float*) noexcept
{
    for (int p = 0; p < 3; ++p) {
        const uint8_t* s = srcp[p];
        uint8_t* d = dstp[p];
        uint8_t* lsb = d + height * dpitch;

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (!STACK16) {
                    d[x] = std::min((s[2 * x] >> 7) + s[2 * x + 1], 255);
                } else {
                    lsb[x] = s[2 * x];
                    d[x] = s[2 * x + 1];
                }
            }
            s += spitch;
            d += dpitch;
            if (STACK16) {
                lsb += dpitch;
            }
        }
    }
}


template <bool STACK16>
static void __stdcall
shader_to_yuv_2_sse2(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float*) noexcept
{
    const __m128i mask = _mm_set1_epi16(0x00FF);

    for (int p = 0; p < 3; ++p) {
        const uint8_t* s = srcp[p];
        uint8_t* d = dstp[p];
        uint8_t* lsb = d + height * dpitch;

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; x += 8) {
                __m128i sx = load(s + 2 * x);
                if (!STACK16) {
                    sx = _mm_adds_epu8(_mm_srli_epi16(sx, 8), _mm_srli_epi16(_mm_and_si128(sx, mask), 8));
                    storel(d + x, _mm_packus_epi16(sx, sx));
                } else {
                    storel(d + x, _mm_packus_epi16(_mm_srli_epi16(sx, 8), sx));
                    storel(lsb + x, _mm_packus_epi16(_mm_and_si128(sx, mask), sx));
                }
            }
            s += spitch;
            d += dpitch;
            if (STACK16) {
                lsb += dpitch;
            }
        }
    }
}


template <bool STACK16>
static void __stdcall
shader_to_yuv_3_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float* buff) noexcept
{
    for (int p = 0; p < 3; ++p) {
        const uint8_t* s = srcp[p];
        uint8_t* d = dstp[p];
        uint8_t* lsb = d + height * dpitch;

        for (int y = 0; y < height; ++y) {
            convert_half_to_float<NO_SIMD>(buff, s, width);
            for (int x = 0; x < width; ++x) {
                if (!STACK16) {
                    d[x] = static_cast<uint8_t>(buff[x] * 255 + 0.5f);
                } else {
                    union {
                        uint16_t v16;
                        uint8_t v8[2];
                    } dst;

                    dst.v16 = static_cast<uint16_t>(buff[x] * 65535 + 0.5f);
                    lsb[x] = dst.v8[0];
                    d[x] = dst.v8[1];
                }
            }
            s += spitch;
            d += dpitch;
            if (STACK16) {
                lsb += dpitch;
            }
        }
    }
}


static __forceinline __m128i i32_to_u16(const __m128i& x, const __m128i& y)
{
    __m128i t0 = _mm_shufflelo_epi16(x, _MM_SHUFFLE(3, 1, 2, 0));
    t0 = _mm_shufflehi_epi16(t0, _MM_SHUFFLE(3, 1, 2, 0));
    __m128i t1 = _mm_shufflelo_epi16(y, _MM_SHUFFLE(2, 0, 3, 1));
    t1 = _mm_shufflehi_epi16(t1, _MM_SHUFFLE(2, 0, 3, 1));
    return _mm_shuffle_epi32(_mm_or_si128(t0, t1), _MM_SHUFFLE(3, 1, 2, 0));
}


template <bool STACK16, arch_t ARCH>
static void __stdcall
shader_to_yuv_3_simd(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float* buff) noexcept
{
    const __m128 coef = _mm_set1_ps(STACK16 ? 65535.0f : 255.0f);
    const __m128i mask16 = _mm_set1_epi16(0x00FF);

    for (int p = 0; p < 3; ++p) {
        const uint8_t* s = srcp[p];
        uint8_t* d = dstp[p];
        uint8_t* lsb = d + height * dpitch;

        for (int y = 0; y < height; ++y) {
            convert_half_to_float<ARCH>(buff, s, width);
            for (int x = 0; x < width; x += 16) {
                __m128i s0 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + x + 0)));
                __m128i s1 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + x + 4)));
                __m128i s2 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + x + 8)));
                __m128i s3 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + x + 12)));
                s0 = i32_to_u16(s0, s1);
                s1 = i32_to_u16(s2, s3);
                if (!STACK16) {
                    s0 = _mm_packus_epi16(s0, s1);
                    stream(d + x, s0);
                } else {
                    __m128i dm = _mm_packus_epi16(_mm_srli_epi16(s0, 8), _mm_srli_epi16(s1, 8));
                    __m128i dl = _mm_packus_epi16(_mm_and_si128(s0, mask16), _mm_and_si128(s1, mask16));
                    stream(d + x, dm);
                    stream(lsb + x, dl);
                }
            }
            s += spitch;
            d += dpitch;
            if (STACK16) {
                lsb += dpitch;
            }
        }
    }
}


template <bool IS_RGB32>
static void __stdcall
shader_to_rgb_1_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float* buff) noexcept
{
    constexpr size_t step = IS_RGB32 ? 4 : 3;

    const uint8_t* sr = srcp[0];
    const uint8_t* sg = srcp[1];
    const uint8_t* sb = srcp[2];
    uint8_t* d = dstp[0] + (height - 1) * dpitch;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            d[step * x + 0] = sb[x];
            d[step * x + 1] = sg[x];
            d[step * x + 2] = sr[x];
            if (IS_RGB32) {
                d[4 * x + 3] = 0;
            }
        }
        sr += spitch;
        sg += spitch;
        sb += spitch;
        d -= dpitch;
    }
}


static void __stdcall
shader_to_rgb32_1_sse2(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float* buff) noexcept
{
    const uint8_t* sr = srcp[0];
    const uint8_t* sg = srcp[1];
    const uint8_t* sb = srcp[2];
    uint8_t* d = dstp[0] + (height - 1) * dpitch;

    const __m128i zero = _mm_setzero_si128();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 8) {
            __m128i bg = _mm_unpacklo_epi8(loadl(sb + x), loadl(sg + x));
            __m128i ra = _mm_unpacklo_epi8(loadl(sr + x), zero);
            __m128i bgra0 = _mm_unpacklo_epi16(bg, ra);
            __m128i bgra1 = _mm_unpackhi_epi16(bg, ra);
            stream(d + 4 * x + 0, bgra0);
            stream(d + 4 * x + 16, bgra1);
        }
        sr += spitch;
        sg += spitch;
        sb += spitch;
        d -= dpitch;
    }
}


template <bool IS_RGB32>
static void __stdcall
shader_to_rgb_2_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float* buff) noexcept
{
    constexpr size_t step = IS_RGB32 ? 4 : 3;

    const uint8_t* sr = srcp[0];
    const uint8_t* sg = srcp[1];
    const uint8_t* sb = srcp[2];
    uint8_t* d = dstp[0] + (height - 1) * dpitch;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            d[step * x + 0] = std::min((sb[2 * x + 0] >> 7) + sb[2 * x + 1], 255);
            d[step * x + 1] = std::min((sg[2 * x + 0] >> 7) + sb[2 * x + 1], 255);
            d[step * x + 2] = std::min((sr[2 * x + 0] >> 7) + sb[2 * x + 1], 255);
            if (IS_RGB32) {
                d[4 * x + 3] = 0;
            }
        }
        sr += spitch;
        sg += spitch;
        sb += spitch;
        d -= dpitch;
    }
}


static void __stdcall
shader_to_rgb32_2_sse2(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float* buff) noexcept
{
    const uint8_t* sr = srcp[0];
    const uint8_t* sg = srcp[1];
    const uint8_t* sb = srcp[2];
    uint8_t* d = dstp[0] + (height - 1) * dpitch;

    const __m128i zero = _mm_setzero_si128();
    const __m128i mask = _mm_set1_epi16(0x00FF);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 8) {
            __m128i b = load(sb + 2 * x);
            b = _mm_adds_epu8(_mm_srli_epi16(b, 8), _mm_srli_epi16(_mm_and_si128(b, mask), 8));
            __m128i g = load(sg + 2 * x);
            g = _mm_adds_epu8(_mm_srli_epi16(g, 8), _mm_srli_epi16(_mm_and_si128(g, mask), 8));
            __m128i r = load(sr + 2 * x);
            r = _mm_adds_epu8(_mm_srli_epi16(r, 8), _mm_srli_epi16(_mm_and_si128(r, mask), 8));
            __m128i b8r8 = _mm_packus_epi16(b, r);
            __m128i g8a8 = _mm_packus_epi16(g, zero);
            __m128i bg = _mm_unpacklo_epi8(b8r8, g8a8);
            __m128i ra = _mm_unpackhi_epi8(b8r8, g8a8);
            stream(d + 4 * x + 0, _mm_unpacklo_epi16(bg, ra));
            stream(d + 4 * x + 16, _mm_unpackhi_epi16(bg, ra));
        }
        sr += spitch;
        sg += spitch;
        sb += spitch;
        d -= dpitch;
    }
}


template <bool IS_RGB32>
static void __stdcall
shader_to_rgb_3_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float* buff) noexcept
{
    constexpr size_t step = IS_RGB32 ? 4 : 3;

    const uint8_t* sr = srcp[0];
    const uint8_t* sg = srcp[1];
    const uint8_t* sb = srcp[2];
    uint8_t* d = dstp[0] + (height - 1) * dpitch;

    float* bb = buff;
    float* bg = bb + ((width + 7) & ~7); // must be aligned 32 bytes
    float* br = bg + ((width + 7) & ~7); // must be aligned 32 bytes

    for (int y = 0; y < height; ++y) {
        convert_half_to_float<NO_SIMD>(br, sr, width);
        convert_half_to_float<NO_SIMD>(bg, sg, width);
        convert_half_to_float<NO_SIMD>(bb, sb, width);
        for (int x = 0; x < width; ++x) {
            d[step * x + 0] = static_cast<uint8_t>(bb[x] * 255 + 0.5f);
            d[step * x + 1] = static_cast<uint8_t>(bg[x] * 255 + 0.5f);
            d[step * x + 2] = static_cast<uint8_t>(br[x] * 255 + 0.5f);
            if (IS_RGB32) {
                d[4 * x + 3] = 0;
            }
        }
        sr += spitch;
        sg += spitch;
        sb += spitch;
        d -= dpitch;
    }
}


template <arch_t ARCH>
static void __stdcall
shader_to_rgb32_3_simd(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float* buff) noexcept
{
    const uint8_t* sr = srcp[0];
    const uint8_t* sg = srcp[1];
    const uint8_t* sb = srcp[2];
    uint8_t* d = dstp[0] + (height - 1) * dpitch;

    float* bb = buff;
    float* bg = bb + ((width + 7) & ~7); // must be aligned 32 bytes
    float* br = bg + ((width + 7) & ~7); // must be aligned 32 bytes

    const __m128 coef = _mm_set1_ps(255.0f);
    const __m128i zero = _mm_setzero_si128();

    for (int y = 0; y < height; ++y) {
        convert_half_to_float<ARCH>(br, sr, width);
        convert_half_to_float<ARCH>(bg, sg, width);
        convert_half_to_float<ARCH>(bb, sb, width);
        for (int x = 0; x < width; x += 4) {
            __m128i b = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(bb + x)));
            __m128i g = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(bg + x)));
            __m128i r = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(br + x)));
            __m128i bgra = _mm_or_si128(b, _mm_slli_si128(g, 1));
            bgra = _mm_or_si128(bgra, _mm_slli_si128(r, 2));
            stream(d + x * 4, bgra);
        }
        sr += spitch;
        sg += spitch;
        sb += spitch;
        d -= dpitch;
    }
}


convert_shader_t get_from_shader_planar(int precision, int pix_type, bool stack16, arch_t arch)
{
    using std::make_tuple;
    constexpr int rgb24 = VideoInfo::CS_BGR24;
    constexpr int rgb32 = VideoInfo::CS_BGR32;
    constexpr int yv24 = VideoInfo::CS_YV24;

    std::map<std::tuple<int, int, bool, arch_t>, convert_shader_t> func;

    func[make_tuple(1, rgb24, false, NO_SIMD)] = shader_to_rgb_1_c<false>;
    func[make_tuple(1, rgb32, false, NO_SIMD)] = shader_to_rgb_1_c<true>;
    func[make_tuple(1, rgb32, false, USE_SSE2)] = shader_to_rgb32_1_sse2;

    func[make_tuple(2, yv24, false, NO_SIMD)] = shader_to_yuv_2_c<false>;
    func[make_tuple(2, yv24, true, NO_SIMD)] = shader_to_yuv_2_c<true>;
    func[make_tuple(2, yv24, false, USE_SSE2)] = shader_to_yuv_2_sse2<false>;
    func[make_tuple(2, yv24, true, USE_SSE2)] = shader_to_yuv_2_sse2<true>;

    func[make_tuple(2, rgb24, false, NO_SIMD)] = shader_to_rgb_2_c<false>;
    func[make_tuple(2, rgb32, false, NO_SIMD)] = shader_to_rgb_2_c<true>;
    func[make_tuple(2, rgb32, false, USE_SSE2)] = shader_to_rgb32_2_sse2;

    func[make_tuple(3, yv24, false, NO_SIMD)] = shader_to_yuv_3_c<false>;
    func[make_tuple(3, yv24, true, NO_SIMD)] = shader_to_yuv_3_c<true>;
    func[make_tuple(3, yv24, false, USE_SSE2)] = shader_to_yuv_3_simd<false, USE_SSE2>;
    func[make_tuple(3, yv24, true, USE_SSE2)] = shader_to_yuv_3_simd<true, USE_SSE2>;

    func[make_tuple(3, rgb24, false, NO_SIMD)] = shader_to_rgb_3_c<false>;
    func[make_tuple(3, rgb32, false, NO_SIMD)] = shader_to_rgb_3_c<true>;
    func[make_tuple(3, rgb32, false, USE_SSE2)] = shader_to_rgb32_3_simd<USE_SSE2>;

#if defined(__AVX__)
    func[make_tuple(3, yv24, false, USE_F16C)] = shader_to_yuv_3_simd<false, USE_F16C>;
    func[make_tuple(3, yv24, true, USE_F16C)] = shader_to_yuv_3_simd<true, USE_F16C>;
    func[make_tuple(3, rgb32, false, USE_F16C)] = shader_to_rgb32_3_simd<USE_F16C>;
#endif

    return func[make_tuple(precision, pix_type, stack16, arch)];


}

