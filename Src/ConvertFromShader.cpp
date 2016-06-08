#include <algorithm>
#include <DirectXPackedVector.h>
#include "ConvertShader.h"


static __forceinline void
convert_half_to_float(float* dstp, const uint8_t* srcp, size_t count)
{
    using namespace DirectX::PackedVector;

    XMConvertHalfToFloatStream(dstp, 4, reinterpret_cast<const HALF*>(srcp), 2, count);
}



template <bool IS_RGB32>
static void __stdcall
shader_to_rgb_2_c(uint8_t** dstp, const uint8_t* srcp, const int dpitch,
    const int spitch, const int width, const int height, float*) noexcept
{
    constexpr size_t step = IS_RGB32 ? 4 : 3;
    uint8_t* d = dstp[0];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            d[step * x + 0] = std::min(srcp[8 * x + 1] + (srcp[8 * x + 0] >> 7), 255);
            d[step * x + 1] = std::min(srcp[8 * x + 3] + (srcp[8 * x + 2] >> 7), 255);
            d[step * x + 2] = std::min(srcp[8 * x + 5] + (srcp[8 * x + 4] >> 7), 255);
            if (IS_RGB32) {
                d[4 * x + 3] = std::min(srcp[8 * x + 7] + (srcp[8 * x + 6] >> 7), 255);
            }
        }
        d += dpitch;
        srcp += spitch;
    }
}



template <bool IS_RGB32>
static void __stdcall
shader_to_rgb_3_c(uint8_t** dstp, const uint8_t* srcp, const int dpitch,
    const int spitch, const int width, const int height, float* buff) noexcept
{
    constexpr size_t step = IS_RGB32 ? 4 : 3;
    uint8_t* d = dstp[0];

    for (int y = 0; y < height; ++y) {
        convert_half_to_float(buff, srcp, width * 4);
        for (int x = 0; x < width; ++x) {
            d[step * x + 0] = static_cast<uint8_t>(buff[4 * x + 0] * 255 + 0.5f);
            d[step * x + 1] = static_cast<uint8_t>(buff[4 * x + 1] * 255 + 0.5f);
            d[step * x + 2] = static_cast<uint8_t>(buff[4 * x + 2] * 255 + 0.5f);
            if (IS_RGB32) {
                d[4 * x + 3] = static_cast<uint8_t>(buff[4 * x + 3] * 255 + 0.5f);
            }
        }
        d += dpitch;
        srcp += spitch;
    }
}

static void __stdcall
shader_to_yuv_1_c(uint8_t** dstp, const uint8_t* srcp, const int dpitch,
    const int spitch, const int width, const int height, float*) noexcept
{
    uint8_t* dy = dstp[0];
    uint8_t* du = dstp[1];
    uint8_t* dv = dstp[2];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            dv[x] = srcp[4 * x + 0];
            du[x] = srcp[4 * x + 1];
            dy[x] = srcp[4 * x + 2];
        }
        dy += dpitch;
        du += dpitch;
        dv += dpitch;
        srcp += spitch;
    }
}


template <bool STACK16>
static void __stdcall
shader_to_yuv_2_c(uint8_t** dstp, const uint8_t* srcp, const int dpitch,
    const int spitch, const int width, const int height, float*) noexcept
{
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
                dv[x] = std::min((srcp[8 * x + 0] >> 7) + srcp[8 * x + 1], 255);
                du[x] = std::min((srcp[8 * x + 2] >> 7) + srcp[8 * x + 3], 255);
                dy[x] = std::min((srcp[8 * x + 4] >> 7) + srcp[8 * x + 5], 255);
            } else {
                lv[x] = srcp[8 * x + 0];
                dv[x] = srcp[8 * x + 1];
                lu[x] = srcp[8 * x + 2];
                du[x] = srcp[8 * x + 3];
                ly[x] = srcp[8 * x + 4];
                dy[x] = srcp[8 * x + 5];
            }
        }
        srcp += spitch;
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
shader_to_yuv_3_c(uint8_t** dstp, const uint8_t* srcp, const int dpitch,
    const int spitch, const int width, const int height, float* buff) noexcept
{
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
        convert_half_to_float(buff, srcp, width * 4);
        for (int x = 0; x < width; ++x) {
            if (!STACK16) {
                dv[x] = static_cast<uint8_t>(buff[4 * x + 0] * 255 + 0.5f);
                du[x] = static_cast<uint8_t>(buff[4 * x + 1] * 255 + 0.5f);
                dy[x] = static_cast<uint8_t>(buff[4 * x + 2] * 255 + 0.5f);
            } else {
                union dst16 {
                    uint16_t v16;
                    uint8_t v8[2];
                };
                dst16 dsty, dstu, dstv;

                dstv.v16 = static_cast<uint16_t>(buff[4 * x + 0] * 65535 + 0.5f);
                dstu.v16 = static_cast<uint16_t>(buff[4 * x + 1] * 65535 + 0.5f);
                dsty.v16 = static_cast<uint16_t>(buff[4 * x + 2] * 65535 + 0.5f);
                ly[x] = dsty.v8[0];
                dy[x] = dsty.v8[1];
                lu[x] = dstu.v8[0];
                du[x] = dstu.v8[1];
                lv[x] = dstv.v8[0];
                dv[x] = dstv.v8[1];
            }
        }
        srcp += spitch;
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


ConvertFromShader::ConvertFromShader(PClip _child, int precision, std::string format, bool stack16, IScriptEnvironment* env) :
    GenericVideoFilter(_child), buff(nullptr), isPlusMt(false)
{
    if (!vi.IsRGB32())
        env->ThrowError("ConvertFromShader: Source must be float-precision RGB");
    if (format == "YV12" && format == "YV24" && format == "RGB24" && format == "RGB32")
        env->ThrowError("ConvertFromShader: Destination format must be YV12, YV24, RGB24 or RGB32");
    if (precision < 1 || precision > 3)
        env->ThrowError("ConvertFromShader: Precision must be 1, 2 or 3");
    if (stack16 && (format != "YV12" && format != "YV24"))
        env->ThrowError("ConvertFromShader: Conversion to Stack16 only supports YV12 and YV24");
    if (stack16 && precision == 1)
        env->ThrowError("ConvertFromShader: When using lsb, don't set precision=1!");

    viSrc = vi;
    if (format == "RGB32") {
        vi.pixel_type = VideoInfo::CS_BGR32;
    } else if (format == "RGB24") {
        vi.pixel_type = VideoInfo::CS_BGR24;
    } else {
        vi.pixel_type = VideoInfo::CS_YV24;
    }

    if (stack16) {      // Stack16 frame has twice the height
        vi.height *= 2;
    }
    if (precision > 1) {    // UINT16 frame has twice the width
        vi.width /= 2;
    }

    if (precision == 3) {
        floatBufferPitch = (viSrc.width * 8 + 63) & ~63;
        isPlusMt = env->FunctionExists("SetFilterMTMode");
        if (!isPlusMt) {
            buff = reinterpret_cast<float*>(_aligned_malloc(floatBufferPitch, 32));
            if (!buff) {
                env->ThrowError("ConvertFromShader: failed to allocate buffer.");
            }
        }
    }

    switch (precision) {
    case 1:
        mainProc = shader_to_yuv_1_c;
        break;
    case 2:
        if (vi.IsRGB()) {
            mainProc = vi.IsRGB32() ? shader_to_rgb_2_c<true> : shader_to_rgb_2_c<false>;
        } else {
            mainProc = stack16 ? shader_to_yuv_2_c<true> : shader_to_yuv_2_c<false>;
        }
        break;
    default:
        if (vi.IsRGB()) {
            mainProc = vi.IsRGB32() ? shader_to_rgb_3_c<true> : shader_to_rgb_3_c<false>;
        } else {
            mainProc = stack16 ? shader_to_yuv_3_c<true> : shader_to_yuv_3_c<false>;
        }
    }
}


ConvertFromShader::~ConvertFromShader() {
    _aligned_free(buff);
}


PVideoFrame __stdcall ConvertFromShader::GetFrame(int n, IScriptEnvironment* env) {
    PVideoFrame src = child->GetFrame(n, env);

    // Convert from float-precision RGB to YV24
    PVideoFrame dst = env->NewVideoFrame(vi);

    float* b = buff;
    if (isPlusMt) {
        void* t = static_cast<IScriptEnvironment2*>(env)->Allocate(floatBufferPitch, 32, AVS_POOLED_ALLOC);
        if (!t) {
            env->ThrowError("ConvertFromShader: failed to allocate buffer.");
        }
        b = reinterpret_cast<float*>(t);
    }

    uint8_t* dstp[] = {
        dst->GetWritePtr(),
        vi.IsRGB() ? nullptr : dst->GetWritePtr(PLANAR_U),
        vi.IsRGB() ? nullptr : dst->GetWritePtr(PLANAR_V),
    };

    mainProc(dstp, src->GetReadPtr(), dst->GetPitch(), src->GetPitch(), vi.width, viSrc.height, b);

    if (isPlusMt) {
        static_cast<IScriptEnvironment2*>(env)->Free(b);
    }

    return dst;
}


