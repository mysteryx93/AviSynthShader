
#if !defined(__AVX__)
#error /arch:avx is not set.
#else

#include <cstdint>
#include <immintrin.h>

#pragma warning(disable:4556)


static __forceinline __m128i loadl(const uint8_t* p)
{
    return _mm_loadl_epi64(reinterpret_cast<const __m128i*>(p));
}


static __forceinline void
convert_float_to_half(uint8_t* dstp, const float* srcp, size_t count)
{
    for (size_t x = 0; x < count; x += 8) {
        __m256 s = _mm256_load_ps(srcp + x);
        __m128i d = _mm256_cvtps_ph(s, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
        _mm_stream_si128(reinterpret_cast<__m128i*>(dstp + 2 * x), d);
    }
}


static __forceinline void
convert_half_to_float(float* dstp, const uint8_t* srcp, size_t count)
{
    for (size_t x = 0; x < count; x += 8) {
        __m128i s = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + 2 * x));
        __m256 d = _mm256_cvtph_ps(s);
        _mm256_store_ps(dstp + x, d);
    }
}



template <bool STACK16>
static inline void
packed_shader_to_yuv_3(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _buff) noexcept
{
    const uint8_t* s = srcp[0];

    uint8_t* dr = dstp[0];
    uint8_t* dg = dstp[1];
    uint8_t* db = dstp[2];

    float * buff = reinterpret_cast<float*>(_buff);

    uint8_t *lr, *lg, *lb;
    const __m128 coef = _mm_set1_ps(STACK16 ? 65535.0f : 255.0f);
    const __m128i mask = _mm_set1_epi16(0x00FF);

    if (STACK16) {
        lr = dr + height * dpitch;
        lg = dg + height * dpitch;
        lb = db + height * dpitch;
    }

    for (int y = 0; y < height; ++y) {
        convert_half_to_float(buff, s, width * 4);
        for (int x = 0; x < width; x += 4) {
            __m128i s0 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + 4 * x +  0))); // R0,G0,B0,A0
            __m128i s1 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + 4 * x +  4))); // R1,G1,B1,A1
            __m128i s2 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + 4 * x +  8))); // R2,G2,B2,A2
            __m128i s3 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + 4 * x + 12))); // R3,G3,B3,A3
            s0 = _mm_or_si128(s0, _mm_slli_epi32(s1, 16)); //R0,R1,G0,G1,B0,B1,A0,A1
            s1 = _mm_or_si128(s2, _mm_slli_epi32(s3, 16)); //R2,R3,G2,G3,B2,B3,A2,A3
            s2 = _mm_unpacklo_epi32(s0, s1); // R0,R1,R2,R3,G0,G1,G2,G3
            s3 = _mm_unpackhi_epi32(s0, s1); // B0,B1,B2,B3,A0,A1,A2,A3
            if (!STACK16) {
                s0 = _mm_packus_epi16(s2, s3);
                *(reinterpret_cast<int32_t*>(dr + x)) = _mm_cvtsi128_si32(s0);
                *(reinterpret_cast<int32_t*>(dg + x)) = _mm_cvtsi128_si32(_mm_srli_si128(s0, 4));
                *(reinterpret_cast<int32_t*>(db + x)) = _mm_cvtsi128_si32(_mm_srli_si128(s0, 8));
            } else {
                __m128i rgbamsb = _mm_packus_epi16(_mm_srli_epi16(s2, 8), _mm_srli_epi16(s3, 8));
                __m128i rgbalsb = _mm_packus_epi16(_mm_and_si128(s2, mask), _mm_and_si128(s3, mask));
                *(reinterpret_cast<int32_t*>(dr + x)) = _mm_cvtsi128_si32(rgbamsb);
                *(reinterpret_cast<int32_t*>(lr + x)) = _mm_cvtsi128_si32(rgbalsb);
                *(reinterpret_cast<int32_t*>(dg + x)) = _mm_cvtsi128_si32(_mm_srli_si128(rgbamsb, 4));
                *(reinterpret_cast<int32_t*>(lg + x)) = _mm_cvtsi128_si32(_mm_srli_si128(rgbalsb, 4));
                *(reinterpret_cast<int32_t*>(db + x)) = _mm_cvtsi128_si32(_mm_srli_si128(rgbamsb, 8));
                *(reinterpret_cast<int32_t*>(lb + x)) = _mm_cvtsi128_si32(_mm_srli_si128(rgbalsb, 8));
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


void __stdcall
packed_shader_to_yuv_3_f16c_stacked(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _buff) noexcept
{
    packed_shader_to_yuv_3<true>(dstp, srcp, dpitch, spitch, width, height, _buff);
}


void __stdcall
packed_shader_to_yuv_3_f16c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _buff) noexcept
{
    packed_shader_to_yuv_3<false>(dstp, srcp, dpitch, spitch, width, height, _buff);
}


void __stdcall
packed_shader_to_rgb32_3_f16c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _buff) noexcept
{
    const uint8_t* s = srcp[0] + (height - 1) * spitch;
    uint8_t* d = dstp[0];
    float* buff = reinterpret_cast<float*>(_buff);

    const __m128 coef = _mm_set1_ps(255.0f);
    const __m128i order = _mm_setr_epi8(2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15);

    for (int y = 0; y < height; ++y) {
        convert_half_to_float(buff, s, width * 4);
        for (int x = 0; x < width; x += 4) {
            __m128i d0 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + 4 * x + 0)));
            __m128i d1 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + 4 * x + 4)));
            __m128i d2 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + 4 * x + 8)));
            __m128i d3 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + 4 * x + 12)));
            d0 = _mm_packus_epi16(_mm_packs_epi32(d0, d1), _mm_packs_epi32(d2, d3));
            _mm_stream_si128(reinterpret_cast<__m128i*>(d + 4 * x), _mm_shuffle_epi8(d0, order));
        }
        d += dpitch;
        s -= spitch;
    }
}



template <bool STACK16>
static inline void
planar_shader_to_yuv_3(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _buff) noexcept
{
    const __m128 coef = _mm_set1_ps(STACK16 ? 65535.0f : 255.0f);
    const __m128i mask16 = _mm_set1_epi16(0x00FF);
    float* buff = reinterpret_cast<float*>(_buff);

    for (int p = 0; p < 3; ++p) {
        const uint8_t* s = srcp[p];
        uint8_t* d = dstp[p];
        uint8_t* lsb = d + height * dpitch;

        for (int y = 0; y < height; ++y) {
            convert_half_to_float(buff, s, width);
            for (int x = 0; x < width; x += 16) {
                __m128i s0 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + x + 0)));
                __m128i s1 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + x + 4)));
                __m128i s2 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + x + 8)));
                __m128i s3 = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(buff + x + 12)));
                s0 = _mm_packus_epi32(s0, s1);
                s1 = _mm_packus_epi32(s2, s3);
                if (!STACK16) {
                    s0 = _mm_packus_epi16(s0, s1);
                    _mm_stream_si128(reinterpret_cast<__m128i*>(d + x), s0);
                } else {
                    __m128i dm = _mm_packus_epi16(_mm_srli_epi16(s0, 8), _mm_srli_epi16(s1, 8));
                    __m128i dl = _mm_packus_epi16(_mm_and_si128(s0, mask16), _mm_and_si128(s1, mask16));
                    _mm_stream_si128(reinterpret_cast<__m128i*>(d + x), dm);
                    _mm_stream_si128(reinterpret_cast<__m128i*>(lsb + x), dl);
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


void __stdcall
planar_shader_to_yuv_3_f16c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _buff) noexcept
{
    planar_shader_to_yuv_3<false>(dstp, srcp, dpitch, spitch, width, height, _buff);
}


void __stdcall
planar_shader_to_yuv_3_f16c_stacked(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _buff) noexcept
{
    planar_shader_to_yuv_3<true>(dstp, srcp, dpitch, spitch, width, height, _buff);
}


void __stdcall
planar_shader_to_rgb32_3_f16c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _buff) noexcept
{
    const uint8_t* sr = srcp[0];
    const uint8_t* sg = srcp[1];
    const uint8_t* sb = srcp[2];
    uint8_t* d = dstp[0] + (height - 1) * dpitch;

    float* bb = reinterpret_cast<float*>(_buff);
    float* bg = bb + ((width + 7) & ~7); // must be aligned 32 bytes
    float* br = bg + ((width + 7) & ~7); // must be aligned 32 bytes

    const __m128 coef = _mm_set1_ps(255.0f);
    const __m128i zero = _mm_setzero_si128();

    for (int y = 0; y < height; ++y) {
        convert_half_to_float(br, sr, width);
        convert_half_to_float(bg, sg, width);
        convert_half_to_float(bb, sb, width);
        for (int x = 0; x < width; x += 4) {
            __m128i b = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(bb + x)));
            __m128i g = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(bg + x)));
            __m128i r = _mm_cvtps_epi32(_mm_mul_ps(coef, _mm_load_ps(br + x)));
            __m128i bgra = _mm_or_si128(b, _mm_slli_si128(g, 1));
            bgra = _mm_or_si128(bgra, _mm_slli_si128(r, 2));
            _mm_stream_si128(reinterpret_cast<__m128i*>(d + x * 4), bgra);
        }
        sr += spitch;
        sg += spitch;
        sb += spitch;
        d -= dpitch;
    }
}



template <bool STACK16>
static inline void
yuv_to_packed_shader_3(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
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


void __stdcall
yuv_to_packed_shader_3_f16c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _buff) noexcept
{
    yuv_to_packed_shader_3<false>(dstp, srcp, dpitch, spitch, width, height, _buff);
}


void __stdcall
yuv_to_packed_shader_3_f16c_stacked(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _buff) noexcept
{
    yuv_to_packed_shader_3<true>(dstp, srcp, dpitch, spitch, width, height, _buff);
}


void __stdcall
rgb32_to_packed_shader_3_f16c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
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
            __m128i sx = _mm_load_si128(reinterpret_cast<const __m128i*>(s + 4 * x));
            sx = _mm_shuffle_epi8(sx, order);

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


template <bool STACK16>
static inline void
yuv_to_planar_shader_3(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
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
                __m128i msbx = _mm_load_si128(reinterpret_cast<const __m128i*>(s + x));
                __m128i d0, lsbx;
                if (!STACK16) {
                    d0 = _mm_unpacklo_epi8(msbx, zero);
                } else {
                    lsbx = _mm_load_si128(reinterpret_cast<const __m128i*>(lsb + x));
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


void __stdcall
yuv_to_planar_shader_3_f16c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _buff) noexcept
{
    yuv_to_planar_shader_3<false>(dstp, srcp, dpitch, spitch, width, height, _buff);
}


void __stdcall
yuv_to_planar_shader_3_f16c_stacked(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, void* _buff) noexcept
{
    yuv_to_planar_shader_3<true>(dstp, srcp, dpitch, spitch, width, height, _buff);
}


void __stdcall
rgb32_to_planar_shader_3_f16c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
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
            __m128i s0 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(s + 4 * x + 0));
            __m128i s1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(s + 4 * x + 8));
            __m128i s2 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(s + 4 * x + 16));
            __m128i s3 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(s + 4 * x + 24));
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


#endif // __AVX__
