#include <algorithm>
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
shader_to_yuv_1_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* s = srcp[0];

    uint8_t* dr = dstp[0];
    uint8_t* dg = dstp[1];
    uint8_t* db = dstp[2];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            db[x] = s[4 * x + 0];
            dg[x] = s[4 * x + 1];
            dr[x] = s[4 * x + 2];
        }
        dr += dpitch;
        dg += dpitch;
        db += dpitch;
        s += spitch;
    }
}


static void __stdcall
shader_to_yuv_1_sse2(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* s = srcp[0];

    uint8_t* dr = dstp[0];
    uint8_t* dg = dstp[1];
    uint8_t* db = dstp[2];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 8) {
            __m128i s0 = loadl(s + 4 * x + 0);      // B0,G0,R0,A0,B1,G1,R1,A1
            __m128i s1 = loadl(s + 4 * x + 8);      // B2,G2,R2,A2,B3,G3,R3,A3
            __m128i t0 = _mm_unpacklo_epi8(s0, s1); // B0,B2,G0,G2,R0,R2,A0,A2,B1,B3,G1,G3,R1,R3,A1,A3

            s0 = loadl(s + 4 * x + 16);             // B4,G4,R4,A4,B5,G5,R5,A5
            s1 = loadl(s + 4 * x + 24);             // B6,G6,R6,A6,B7,G7,R7,A7
            __m128i t1 = _mm_unpacklo_epi8(s0, s1); // B4,B6,G4,G6,R4,R6,A4,A6,B5,B7,G5,G7,R5,R7,A5,A7

            s0 = _mm_unpacklo_epi16(t0, t1);        // B0,B2,B4,B6,G0,G2,G4,G6,R0,R2,R4,R6,A0,A2,A4,A6
            s1 = _mm_unpackhi_epi16(t0, t1);        // B1,B3,B5,B7,G1,G3,G5,G7,R1,R3,R5,R7,A1,A3,A5,A7

            t0 = _mm_unpacklo_epi8(s0, s1);         // B0,B1,B2,B3,B4,B5,B6,B7,G0,G1,G2,G3,G4,G5,G6,G7
            t1 = _mm_unpackhi_epi8(s0, s1);         // R0,R1,R2,R3,R4,R5,R6,R7,AAAAAAAA

            storel(db + x, t0); 
            storel(dg + x, _mm_srli_si128(t0, 8));
            storel(dr + x, t1);
        }
        dr += dpitch;
        dg += dpitch;
        db += dpitch;
        s += spitch;
    }
}


template <bool STACK16>
static void __stdcall
shader_to_yuv_2_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* s = srcp[0];

    uint8_t* dr = dstp[0];
    uint8_t* dg = dstp[1];
    uint8_t* db = dstp[2];

    uint8_t *lr, *lg, *lb;
    if (STACK16) {
        lr = dr + height * dpitch;
        lg = dg + height * dpitch;
        lb = db + height * dpitch;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (!STACK16) {
                dr[x] = std::min((s[8 * x + 0] >> 7) + s[8 * x + 1], 255);
                dg[x] = std::min((s[8 * x + 2] >> 7) + s[8 * x + 3], 255);
                db[x] = std::min((s[8 * x + 4] >> 7) + s[8 * x + 5], 255);
            } else {
                lr[x] = s[8 * x + 0];
                dr[x] = s[8 * x + 1];
                lg[x] = s[8 * x + 2];
                dg[x] = s[8 * x + 3];
                lb[x] = s[8 * x + 4];
                db[x] = s[8 * x + 5];
            }
        }
        s += spitch;
        dr += dpitch;
        dg += dpitch;
        db += dpitch;
        if (STACK16) {
            lr += dpitch;
            lg += dpitch;
            lb += dpitch;
        }
    }
}


template <bool STACK16>
static void __stdcall
shader_to_yuv_2_sse2(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* s = srcp[0];

    uint8_t* dr = dstp[0];
    uint8_t* dg = dstp[1];
    uint8_t* db = dstp[2];

    uint8_t *lr, *lg, *lb;
    if (STACK16) {
        lr = dr + height * dpitch;
        lg = dg + height * dpitch;
        lb = db + height * dpitch;
    }

    const __m128i mask = _mm_set1_epi16(0x00FF);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 8) {
            __m128i s0 = load(s + 8 * x + 0);       // R0,G0,B0,A0,R1,G1,B1,A1
            __m128i s1 = load(s + 8 * x + 16);      // R2,G2,B2,A2,R3,G3,B3,A3
            __m128i s2 = load(s + 8 * x + 32);      // R4,G4,B4,A4,R5,G5,B5,A5
            __m128i s3 = load(s + 8 * x + 48);      // R6,G6,B6,A6,R7,G7,B7,A7

            __m128i t0 = _mm_unpacklo_epi16(s0, s2);// R0,R4,G0,G4,B0,B4,A0,A4
            __m128i t1 = _mm_unpackhi_epi16(s0, s2);// R1,R5,G1,G5,B1,B5,A1,A5
            __m128i t2 = _mm_unpacklo_epi16(s1, s3);// R2,R6,G2,G6,B2,B6,A2,A6
            __m128i t3 = _mm_unpackhi_epi16(s1, s3);// R3,R7,G3,G7,B3,B7,A3,A7

            s0 = _mm_unpacklo_epi16(t0, t2);        // R0,R2,R4,R6,G0,G2,G4,G6
            s1 = _mm_unpackhi_epi16(t0, t2);        // B0,B2,B4,B6,A0,A2,A4,A6
            s2 = _mm_unpacklo_epi16(t1, t3);        // R1,R3,R5,R6,G1,G3,G5,G7
            s3 = _mm_unpackhi_epi16(t1, t3);        // B1,B3,B5,B7,A1,A3,A5,A7

            t0 = _mm_unpacklo_epi16(s0, s2);        // R0,R1,R2,R3,R4,R5,R6,R7
            t1 = _mm_unpackhi_epi16(s0, s2);        // G0,G1,G2,G3,G4,G5,G6,G7
            t2 = _mm_unpacklo_epi16(s1, s3);        // B0,B1,B2,B3,B4,B5,B6,B7

            if (!STACK16) {
                s0 = _mm_adds_epu8(_mm_srli_epi16(t0, 8), _mm_srli_epi16(_mm_and_si128(t0, mask), 7));
                storel(dr + x, _mm_packus_epi16(s0, s0));
                s1 = _mm_adds_epu8(_mm_srli_epi16(t1, 8), _mm_srli_epi16(_mm_and_si128(t1, mask), 7));
                storel(dg + x, _mm_packus_epi16(s1, s1));
                s2 = _mm_adds_epu8(_mm_srli_epi16(t2, 8), _mm_srli_epi16(_mm_and_si128(t2, mask), 7));
                storel(db + x, _mm_packus_epi16(s2, s2));
            } else {
                storel(dr + x, _mm_packus_epi16(_mm_srli_epi16(t0, 8), t0));
                storel(lr + x, _mm_packus_epi16(_mm_and_si128(t0, mask), t0));
                storel(dg + x, _mm_packus_epi16(_mm_srli_epi16(t1, 8), t1));
                storel(lg + x, _mm_packus_epi16(_mm_and_si128(t1, mask), t1));
                storel(db + x, _mm_packus_epi16(_mm_srli_epi16(t2, 8), t2));
                storel(lb + x, _mm_packus_epi16(_mm_and_si128(t2, mask), t2));
            }
        }
        s += spitch;
        dr += dpitch;
        dg += dpitch;
        db += dpitch;
        if (STACK16) {
            lr += dpitch;
            lg += dpitch;
            lb += dpitch;
        }
    }
}


template <bool STACK16>
static void __stdcall
shader_to_yuv_3_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _lut) noexcept
{
    const uint8_t* s = srcp[0];

    uint8_t* dr = dstp[0];
    uint8_t* dg = dstp[1];
    uint8_t* db = dstp[2];

    const uint16_t* lut = reinterpret_cast<const uint16_t*>(_lut);

    uint8_t *lr, *lg, *lb;
    if (STACK16) {
        lr = dr + height * dpitch;
        lg = dg + height * dpitch;
        lb = db + height * dpitch;
    }

    for (int y = 0; y < height; ++y) {
        const uint16_t* s16 = reinterpret_cast<const uint16_t*>(s);
        for (int x = 0; x < width; ++x) {
            if (!STACK16) {
                dr[x] = static_cast<uint8_t>(lut[s16[4 * x + 0]]);
                dg[x] = static_cast<uint8_t>(lut[s16[4 * x + 1]]);
                db[x] = static_cast<uint8_t>(lut[s16[4 * x + 2]]);
            } else {
                union dst16 {
                    uint16_t v16;
                    uint8_t v8[2];
                };
                dst16 dstr, dstg, dstb;

                dstr.v16 = lut[s16[4 * x + 0]];
                dstg.v16 = lut[s16[4 * x + 1]];
                dstb.v16 = lut[s16[4 * x + 2]];
                lr[x] = dstr.v8[0];
                dr[x] = dstr.v8[1];
                lg[x] = dstg.v8[0];
                dg[x] = dstg.v8[1];
                lb[x] = dstb.v8[0];
                db[x] = dstb.v8[1];
            }
        }
        s += spitch;
        dr += dpitch;
        dg += dpitch;
        db += dpitch;
        if (STACK16) {
            lr += dpitch;
            lg += dpitch;
            lb += dpitch;
        }
    }
}




template <bool IS_RGB32>
static void __stdcall
shader_to_rgb_1_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void*) noexcept
{
    const uint8_t* s = srcp[0] + (height - 1) * spitch;
    uint8_t* d = dstp[0];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (IS_RGB32) {
                memcpy(d, s, width * 4); // same as FlipVertical()
            } else {
                d[3 * x + 0] = s[4 * x + 0];
                d[3 * x + 1] = s[4 * x + 1];
                d[3 * x + 2] = s[4 * x + 2];
            }
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
            d[step * x + 0] = std::min((s[8 * x + 4] >> 7) + s[8 * x + 5], 255);
            d[step * x + 1] = std::min((s[8 * x + 2] >> 7) + s[8 * x + 3], 255);
            d[step * x + 2] = std::min((s[8 * x + 0] >> 7) + s[8 * x + 1], 255);
            if (IS_RGB32) {
                d[4 * x + 3] = std::min((s[8 * x + 6] >> 7) + s[8 * x + 7], 255);
            }
        }
        d += dpitch;
        s -= spitch;
    }
}



//static void __stdcall
//shader_to_rgb32_2_ssse3(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
//    const int spitch, const int width, const int height, void*) noexcept
//{
//    const uint8_t* s = srcp[0] + (height - 1) * spitch;
//    uint8_t* d = dstp[0];
//
//    const __m128i mask = _mm_set1_epi16(0x00FF);
//    const __m128i order = _mm_setr_epi8(2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15);
//
//    for (int y = 0; y < height; ++y) {
//        for (int x = 0; x < width; x += 4) {
//            __m128i d0 = load(s + 8 * x);
//            __m128i d1 = load(s + 8 * x + 16);
//            d0 = _mm_adds_epu8(_mm_srli_epi16(d0, 8), _mm_srli_epi16(_mm_and_si128(d0, mask), 7));
//            d1 = _mm_adds_epu8(_mm_srli_epi16(d1, 8), _mm_srli_epi16(_mm_and_si128(d1, mask), 7));
//            d0 = _mm_shuffle_epi8(_mm_packus_epi16(d0, d1), order);
//            stream(d + 4 * x, d0);
//        }
//        d += dpitch;
//        s -= spitch;
//    }
//}


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
            d[step * x + 0] = static_cast<uint8_t>(lut[s16[4 * x + 2]]);
            d[step * x + 1] = static_cast<uint8_t>(lut[s16[4 * x + 1]]);
            d[step * x + 2] = static_cast<uint8_t>(lut[s16[4 * x + 0]]);
            if (IS_RGB32) {
                d[4 * x + 3] = static_cast<uint8_t>(lut[s16[4 * x + 3]]);
            }
        }
        d += dpitch;
        s -= spitch;
    }
}


extern void __stdcall
packed_shader_to_yuv_3_f16c_stacked(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _buff) noexcept;


extern void __stdcall
packed_shader_to_yuv_3_f16c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _buff) noexcept;


extern void __stdcall
packed_shader_to_rgb32_3_f16c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _buff) noexcept;


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

    func[make_tuple(2, yv24, false, NO_SIMD)] = shader_to_yuv_2_c<false>;
    func[make_tuple(2, yv24, true, NO_SIMD)] = shader_to_yuv_2_c<true>;
    func[make_tuple(2, yv24, false, USE_SSE2)] = shader_to_yuv_2_sse2<false>;
    func[make_tuple(2, yv24, true, USE_SSE2)] = shader_to_yuv_2_sse2<true>;

    func[make_tuple(2, rgb24, false, NO_SIMD)] = shader_to_rgb_2_c<false>;
    func[make_tuple(2, rgb32, false, NO_SIMD)] = shader_to_rgb_2_c<true>;
    //func[make_tuple(2, rgb32, false, USE_SSSE3)] = shader_to_rgb32_2_ssse3;

    func[make_tuple(3, yv24, false, NO_SIMD)] = shader_to_yuv_3_c<false>;
    func[make_tuple(3, yv24, true, NO_SIMD)] = shader_to_yuv_3_c<true>;

    func[make_tuple(3, rgb24, false, NO_SIMD)] = shader_to_rgb_3_c<false>;
    func[make_tuple(3, rgb32, false, NO_SIMD)] = shader_to_rgb_3_c<true>;

    func[make_tuple(3, yv24, false, USE_F16C)] = packed_shader_to_yuv_3_f16c;
    func[make_tuple(3, yv24, true, USE_F16C)] = packed_shader_to_yuv_3_f16c_stacked;
    func[make_tuple(3, rgb32, false, USE_F16C)] = packed_shader_to_rgb32_3_f16c;

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

