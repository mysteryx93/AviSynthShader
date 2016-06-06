#include <DirectXPackedVector.h>
#include <d3dx9.h>
#include "ConvertToShader.h"


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

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (!STACK16) {
                constexpr float rcp8 = 1.0f / 255;
                buff[4 * x + 0] = sv[x] * rcp8;
                buff[4 * x + 1] = su[x] * rcp8;
                buff[4 * x + 2] = sy[x] * rcp8;
            } else {
                constexpr float rcp16 = 1.0f / 65535;
                buff[4 * x + 0] = (sv[x] << 8 | vlsb[x]) * rcp16;
                buff[4 * x + 1] = (su[x] << 8 | ulsb[x]) * rcp16;
                buff[4 * x + 2] = (sy[x] << 8 | ylsb[x]) * rcp16;
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

    viSrc = vi;
    vi.pixel_type = VideoInfo::CS_BGR32;
    if (precision > 1) // Half-float frame has its width twice larger than normal
        vi.width *= 2;
    if (stack16)
        vi.height /= 2;
    if (precision == 3) {
        floatBufferPitch = vi.width << 4;
        isPlusMt = env->FunctionExists("SetFilterMTMode");
        if (!isPlusMt) {
            buff = static_cast<float*>(_aligned_malloc(floatBufferPitch, 16));
            if (!buff) {
                env->ThrowError("ConvertToShader: Failed to allocate temporal buffer.");
            }
        }
    }

    switch (precision) {
    case 1:
        mainProc = yuv_to_shader_1_c;
        break;
    case 2:
        if (viSrc.IsRGB32()) {
            mainProc = rgb_to_shader_2_c<true>;
        } else if (viSrc.IsRGB24()) {
            mainProc = rgb_to_shader_2_c<false>;
        } else if (stack16) {
            mainProc = yuv_to_shader_2_c<true>;
        } else {
            mainProc = yuv_to_shader_2_c<false>;
        }
        break;
    default:
        if (viSrc.IsRGB32()) {
            mainProc = rgb_to_shader_3_c<true>;
        } else if (viSrc.IsRGB24()) {
            mainProc = rgb_to_shader_3_c<false>;
        } else if (stack16) {
            mainProc = yuv_to_shader_3_c<true>;
        } else {
            mainProc = yuv_to_shader_3_c<false>;
        }
    }


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

