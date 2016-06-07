#include <map>
#include <tuple>
#include <DirectXPackedVector.h>
#include <d3dx9.h>
#if defined(__AVX__)
#include <immintrin.h>
#else
#include <emmintrin.h>
#endif
#include "ConvertToShader.h"

static __forceinline __m128i loadl(const uint8_t* p)
{
    return _mm_loadl_epi64(reinterpret_cast<const __m128i*>(p));
}

static __forceinline void stream(uint8_t* p, const __m128i& x)
{
    _mm_stream_si128(reinterpret_cast<__m128i*>(p), x);
}

#if defined(__AVX__)
static __forceinline void
convert_float_to_half_f16c(float* srcp, uint8_t* dstp, size_t count)
{
    for (size_t x = 0; x < count; x += 8) {
        __m256 s = _mm256_load_ps(srcp + x);
        __m128i d = _mm256_cvtps_ph(s, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
        stream(dstp + 2 * x, d);
    }
}
#endif


static void __stdcall
yuv_to_shader_1_c(uint8_t* dstp, const uint8_t** srcp, const int dpitch,
                  const int spitch, const int width, const int height, float*) noexcept
{
    const uint8_t* sy = srcp[0];
    const uint8_t* su = srcp[1];
    const uint8_t* sv = srcp[2];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            dstp[4 * x + 0] = sv[x];
            dstp[4 * x + 1] = su[x];
            dstp[4 * x + 2] = sy[x];
            dstp[4 * x + 3] = 0;
        }
        sy += spitch;
        su += spitch;
        sv += spitch;
        dstp += dpitch;
    }
}


static void __stdcall
yuv_to_shader_1_sse2(uint8_t* dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float*) noexcept
{
    const uint8_t* sy = srcp[0];
    const uint8_t* su = srcp[1];
    const uint8_t* sv = srcp[2];

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
            stream(dstp + 4 * x + 0, vuya0);
            stream(dstp + 4 * x + 16, vuya1);
        }
        sy += spitch;
        su += spitch;
        sv += spitch;
        dstp += dpitch;
    }
}


template <bool STACK16>
static void __stdcall
yuv_to_shader_2_c(uint8_t* dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float*) noexcept
{
    const uint8_t* sy = srcp[0];
    const uint8_t* su = srcp[1];
    const uint8_t* sv = srcp[2];

    const uint8_t *ylsb, *ulsb, *vlsb;
    if (STACK16) {
        ylsb = sy + height * spitch;
        ulsb = su + height * spitch;
        vlsb = sv + height * spitch;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (!STACK16) {
                dstp[8 * x + 0] = 0;
                dstp[8 * x + 1] = sv[x];
                dstp[8 * x + 2] = 0;
                dstp[8 * x + 3] = su[x];
                dstp[8 * x + 4] = 0;
                dstp[8 * x + 5] = sy[x];
            } else {
                dstp[8 * x + 0] = vlsb[x];
                dstp[8 * x + 1] = sv[x];
                dstp[8 * x + 2] = ulsb[x];
                dstp[8 * x + 3] = su[x];
                dstp[8 * x + 4] = ylsb[x];
                dstp[8 * x + 5] = sy[x];
                dstp[8 * x + 6] = dstp[8 * x + 7] = 0;
            }
            dstp[8 * x + 6] = dstp[8 * x + 7] = 0;
        }
        dstp += dpitch;
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
yuv_to_shader_2_sse2(uint8_t* dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float*) noexcept
{
    const uint8_t* sy = srcp[0];
    const uint8_t* su = srcp[1];
    const uint8_t* sv = srcp[2];

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
                y = _mm_unpacklo_epi8(loadl(sy + x), zero);
                u = _mm_unpacklo_epi8(loadl(su + x), zero);
                v = _mm_unpacklo_epi8(loadl(sv + x), zero);
            } else {
                y = _mm_unpacklo_epi8(loadl(sy + x), loadl(ylsb + x));
                u = _mm_unpacklo_epi8(loadl(su + x), loadl(ulsb + x));
                v = _mm_unpacklo_epi8(loadl(sv + x), loadl(vlsb + x));
            }
            __m128i vu = _mm_unpacklo_epi16(v, u);
            __m128i ya = _mm_unpacklo_epi16(y, zero);
            stream(dstp + 8 * x + 0, _mm_unpacklo_epi32(vu, ya));
            stream(dstp + 8 * x + 16, _mm_unpackhi_epi32(vu, ya));
            vu = _mm_unpackhi_epi16(v, u);
            ya = _mm_unpackhi_epi16(y, zero);
            stream(dstp + 8 * x + 32, _mm_unpacklo_epi32(vu, ya));
            stream(dstp + 8 * x + 48, _mm_unpackhi_epi32(vu, ya));
        }
        dstp += dpitch;
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
yuv_to_shader_3_c(uint8_t* dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float* buff) noexcept
{
    using namespace DirectX::PackedVector;

    const uint8_t* sy = srcp[0];
    const uint8_t* su = srcp[1];
    const uint8_t* sv = srcp[2];

    const uint8_t *ylsb, *ulsb, *vlsb;
    if (STACK16) {
        ylsb = sy + height * spitch;
        ulsb = su + height * spitch;
        vlsb = sv + height * spitch;
    }

    constexpr float rcp = 1.0f / (STACK16 ? 65535 : 255);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (!STACK16) {
                buff[4 * x + 0] = sv[x] * rcp;
                buff[4 * x + 1] = su[x] * rcp;
                buff[4 * x + 2] = sy[x] * rcp;
            } else {
                buff[4 * x + 0] = (sv[x] << 8 | vlsb[x]) * rcp;
                buff[4 * x + 1] = (su[x] << 8 | ulsb[x]) * rcp;
                buff[4 * x + 2] = (sy[x] << 8 | ylsb[x]) * rcp;
            }
            buff[4 * x + 3] = 0.0f;
        }
        XMConvertFloatToHalfStream(reinterpret_cast<HALF*>(dstp), sizeof(HALF),
            buff, sizeof(float), width * 4);
        dstp += dpitch;
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


template <bool STACK16, int ARCH>
static void __stdcall
yuv_to_shader_3_simd(uint8_t* dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float* buff) noexcept
{
    using namespace DirectX::PackedVector;

    const uint8_t* sy = srcp[0];
    const uint8_t* su = srcp[1];
    const uint8_t* sv = srcp[2];

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
                u = _mm_unpacklo_epi16(_mm_unpacklo_epi8(loadl(sy + x), zero), zero);
                v = _mm_unpacklo_epi16(_mm_unpacklo_epi8(loadl(sy + x), zero), zero);
            } else {
                y = _mm_unpacklo_epi16(_mm_unpacklo_epi8(loadl(sy + x), loadl(ylsb + x)), zero);
                u = _mm_unpacklo_epi16(_mm_unpacklo_epi8(loadl(su + x), loadl(ulsb + x)), zero);
                v = _mm_unpacklo_epi16(_mm_unpacklo_epi8(loadl(sv + x), loadl(vlsb + x)), zero);
            }
            __m128i vu = _mm_unpacklo_epi32(v, u);
            __m128i ya = _mm_unpacklo_epi32(y, zero);
            __m128 vuya0 = _mm_cvtepi32_ps(_mm_unpacklo_epi64(vu, ya));
            __m128 vuya1 = _mm_cvtepi32_ps(_mm_unpackhi_epi64(vu, ya));
            _mm_store_ps(buff + 4 * x + 0, vuya0);
            _mm_store_ps(buff + 4 * x + 4, vuya1);

            vu = _mm_unpackhi_epi32(v, u);
            ya = _mm_unpackhi_epi32(y, zero);
            vuya0 = _mm_cvtepi32_ps(_mm_unpacklo_epi64(vu, ya));
            vuya1 = _mm_cvtepi32_ps(_mm_unpackhi_epi64(vu, ya));
            _mm_store_ps(buff + 4 * x +  8, vuya0);
            _mm_store_ps(buff + 4 * x + 12, vuya1);
        }
#if defined(__AVX__)
        if (ARCH == USE_F16C) {
            convert_float_to_half_f16c(buff, dstp, width * 4);
        } else {
            XMConvertFloatToHalfStream(reinterpret_cast<HALF*>(dstp), sizeof(HALF),
                buff, sizeof(float), width * 4);
        }
#else
        XMConvertFloatToHalfStream(reinterpret_cast<HALF*>(dstp), sizeof(HALF),
            buff, sizeof(float), width * 4);
#endif
        dstp += dpitch;
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


template <bool IS_RGB32>
static void __stdcall
rgb_to_shader_2_c(uint8_t* dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float*) noexcept
{
    constexpr size_t step = IS_RGB32 ? 4 : 3;

    const uint8_t* s = srcp[0];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            dstp[8 * x + 0] = 0;
            dstp[8 * x + 1] = s[step * x + 0];
            dstp[8 * x + 2] = 0;
            dstp[8 * x + 3] = s[step * x + 1];
            dstp[8 * x + 4] = 0;
            dstp[8 * x + 5] = s[step * x + 2];
            dstp[8 * x + 6] = 0;
            dstp[8 * x + 7] = IS_RGB32 ? s[4 * x + 3] : 0;
        }
        dstp += dpitch;
        s += spitch;
    }
}


static void __stdcall
rgb32_to_shader_2_sse2(uint8_t* dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float*) noexcept
{
    const uint8_t* s = srcp[0];
    const __m128i zero = _mm_setzero_si128();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width * 4; x += 8) {
            stream(dstp + 2 * x, _mm_unpacklo_epi8(loadl(s + x), zero));
        }
        dstp += dpitch;
        s += spitch;
    }
}


template <bool IS_RGB32>
static void __stdcall
rgb_to_shader_3_c(uint8_t* dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float* buff) noexcept
{
    using namespace DirectX::PackedVector;

    constexpr size_t step = IS_RGB32 ? 4 : 3;

    const uint8_t* s = srcp[0];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            constexpr float rcp = 1.0f / 255;
            buff[4 * x + 0] = s[step * x + 0] * rcp;
            buff[4 * x + 1] = s[step * x + 1] * rcp;
            buff[4 * x + 2] = s[step * x + 2] * rcp;
            buff[4 * x + 3] = IS_RGB32 ? s[4 * x + 3] * rcp : 0.0f;
        }
        XMConvertFloatToHalfStream(reinterpret_cast<HALF*>(dstp), sizeof(HALF),
                                   buff, sizeof(float), width * 4);
        dstp += dpitch;
        s += spitch;
    }
}


template <arch_t ARCH>
static void __stdcall
rgb32_to_shader_3_simd(uint8_t* dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float* buff) noexcept
{
    using namespace DirectX::PackedVector;

    const uint8_t* s = srcp[0];

    const __m128i zero = _mm_setzero_si128();
    const __m128 rcp = _mm_set1_ps(1.0f / 255);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width * 4; x += 4) {
            __m128i i32 = _mm_unpacklo_epi16(_mm_unpacklo_epi8(loadl(s + x), zero), zero);
            __m128 f32 = _mm_mul_ps(_mm_cvtepi32_ps(i32), rcp);
            _mm_store_ps(buff + x, f32);
        }
#if defined(__AVX__)
        if (ARCH == USE_F16C) {
            convert_float_to_half_f16c(buff, dstp, width * 4);
        } else {
            XMConvertFloatToHalfStream(reinterpret_cast<HALF*>(dstp), sizeof(HALF),
                buff, sizeof(float), width * 4);
        }
#else
        XMConvertFloatToHalfStream(reinterpret_cast<HALF*>(dstp), sizeof(HALF),
            buff, sizeof(float), width * 4);
#endif
        dstp += dpitch;
        s += spitch;
    }
}


static arch_t get_arch()
{
    if (!has_sse2()) {
        return NO_SIMD;
    }
#if !defined(__AVX__)
    return USE_SSE2;
#else
    if (!has_f16c()) {
        return USE_SSE2;
    }
    return USE_F16C;
#endif
}


using main_proc_t = decltype(yuv_to_shader_1_c)*;

main_proc_t get_main_proc(int precision, int pix_type, bool stack16, arch_t arch)
{
    using std::make_tuple;

    constexpr int rgb24 = VideoInfo::CS_BGR24;
    constexpr int rgb32 = VideoInfo::CS_BGR32;
    constexpr int yv24 = VideoInfo::CS_YV24;

    std::map<std::tuple<int, int, bool, arch_t>, main_proc_t> func;

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
    func[make_tuple(3, yv24, false, USE_SSE2)] = yuv_to_shader_3_simd<false, USE_SSE2>;
    func[make_tuple(3, yv24, true, USE_SSE2)] = yuv_to_shader_3_simd<true, USE_SSE2>;

    func[make_tuple(3, rgb24, false, NO_SIMD)] = rgb_to_shader_3_c<false>;
    func[make_tuple(3, rgb32, false, NO_SIMD)] = rgb_to_shader_3_c<true>;
    func[make_tuple(3, rgb32, false, USE_SSE2)] = rgb32_to_shader_3_simd<USE_SSE2>;

#if defined(__AVX__)
    func[make_tuple(3, yv24, false, USE_F16C)] = yuv_to_shader_3_simd<false, USE_F16C>;
    func[make_tuple(3, yv24, true, USE_F16C)] = yuv_to_shader_3_simd<true, USE_F16C>;
    func[make_tuple(3, rgb32, false, USE_F16C)] = rgb32_to_shader_3_simd<USE_F16C>;
#endif


    return func[make_tuple(precision, pix_type, stack16, arch)];
}


ConvertToShader::ConvertToShader(PClip _child, int precision, bool stack16, IScriptEnvironment* env) :
    GenericVideoFilter(_child), isPlusMt(false), buff(nullptr)
{
    if (!vi.IsYV24() && !vi.IsRGB24() && !vi.IsRGB32())
        env->ThrowError("ConvertToShader: Source must be YV12, YV24, RGB24 or RGB32");
    if (precision < 1 && precision > 3)
        env->ThrowError("ConvertToShader: Precision must be 1, 2 or 3");
    if (stack16 && vi.IsRGB())
        env->ThrowError("ConvertToShader: Conversion from Stack16 only supports YV12 and YV24");
    if (stack16 && precision == 1)
        env->ThrowError("ConvertToShader: When using lsb, don't set precision=1!");

    arch_t arch = get_arch();
    if (precision != 3 && arch > USE_SSE2) {
        arch = USE_SSE2;
    }

    if (vi.IsRGB24() && arch != NO_SIMD) {
        child = env->Invoke("ConvertToRGB32", child).AsClip();
        vi = child->GetVideoInfo();
    }

    viSrc = vi;

    vi.pixel_type = VideoInfo::CS_BGR32;

    if (precision > 1) {    // Half-float frame has its width twice larger than normal
        vi.width *= 2;
    }

    if (stack16) {
        vi.height /= 2;
    }

    if (precision == 3) {
        floatBufferPitch = (vi.width * 4 * 4 + 63) & ~63;
        isPlusMt = env->FunctionExists("SetFilterMTMode");
        if (!isPlusMt) {
            buff = static_cast<float*>(_aligned_malloc(floatBufferPitch, 16));
            if (!buff) {
                env->ThrowError("ConvertToShader: Failed to allocate temporal buffer.");
            }
        }
    }

    mainProc = get_main_proc(precision, viSrc.pixel_type, stack16, arch);
}


ConvertToShader::~ConvertToShader() {
    _aligned_free(buff);
}


PVideoFrame __stdcall ConvertToShader::GetFrame(int n, IScriptEnvironment* env) {
    PVideoFrame src = child->GetFrame(n, env);

    // Convert from YV24 to half-float RGB
    PVideoFrame dst = env->NewVideoFrame(vi);

    const uint8_t* srcp[] = {
        src->GetReadPtr(),
        viSrc.IsRGB() ? nullptr : src->GetReadPtr(PLANAR_U),
        viSrc.IsRGB() ? nullptr : src->GetReadPtr(PLANAR_V),
    };

    float* b = buff;
    if (isPlusMt) {
        void* t = static_cast<IScriptEnvironment2*>(env)->Allocate(floatBufferPitch, 16, AVS_POOLED_ALLOC);
        if (!t) {
            env->ThrowError("ConvertToShader: Failed to allocate temporal buffer.");
        }
        b = reinterpret_cast<float*>(t);
    }

    mainProc(dst->GetWritePtr(), srcp, dst->GetPitch(), src->GetPitch(), viSrc.width, vi.height, b);

    if (isPlusMt) {
        static_cast<IScriptEnvironment2*>(env)->Free(b);
    }

    return dst;
}

