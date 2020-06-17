#include <DirectXPackedVector.h>
#include "ConvertShader.h"


extern bool has_sse2() noexcept;
extern bool has_ssse3() noexcept;
extern bool has_f16c() noexcept;


static arch_t get_arch(int opt) noexcept
{
    if (opt == 0 || !has_sse2()) {
        return NO_SIMD;
    }
    if (opt == 1 || !has_ssse3()) {
        return USE_SSE2;
    }
#if !defined(__AVX__)
    return USE_SSSE3;
#else
    if (opt == 2 || !has_f16c()) {
        return USE_SSSE3;
    }
    return USE_F16C;
#endif
}


void ConvertShader::constructToShader(int precision, bool stack16, bool planar, arch_t arch)
{
    viSrc = vi;

    vi.pixel_type = planar ? VideoInfo::CS_YV24 : VideoInfo::CS_BGR32;

    if (precision > 1) {    // Half-float frame has its width twice larger than normal
        vi.width *= 2;
    }

    if (stack16) {
        vi.height /= 2;
    }

    procWidth = viSrc.width;
    procHeight = vi.height;

    floatBufferPitch = (vi.width * 4 * 4 + 63) & ~63; // must be mod64

    mainProc = planar ? get_to_shader_planar(precision, viSrc.pixel_type, stack16, arch)
        : get_to_shader_packed(precision, viSrc.pixel_type, stack16, arch);

    if (precision == 3 && arch != USE_F16C) {
        useLut = true;
        int maximum = stack16 ? 65536 : 256;
        lut.resize(maximum);
        for (int i = 0; i < maximum; ++i) {
            lut[i] = DirectX::PackedVector::XMConvertFloatToHalf(i * 1.0f / (maximum - 1));
        }
    }


}


void ConvertShader::constructFromShader(int precision, bool stack16, std::string& format, arch_t arch)
{
    viSrc = vi;

    if (format == "RGB32") {
        vi.pixel_type = VideoInfo::CS_BGR32;
    } else if (format == "RGB24") {
        vi.pixel_type = VideoInfo::CS_BGR24;
    } else {
        vi.pixel_type = VideoInfo::CS_YV24;
    }

    if (stack16) { // Stack16 frame has twice the height
        vi.height *= 2;
    }
    if (precision > 1) { // UINT16 frame has twice the width
        vi.width /= 2;
    }

    procWidth = vi.width;
    procHeight = viSrc.height;

    floatBufferPitch = (viSrc.width * 8 + 63) & ~63; // must be mod64

    mainProc = viSrc.IsRGB() ? get_from_shader_packed(precision, vi.pixel_type, stack16, arch)
        : get_from_shader_planar(precision, vi.pixel_type, stack16, arch);

    if (precision == 3 && arch != USE_F16C) {
        useLut = true;
        int maximum = stack16 ? 65535 : 255;
        lut.resize(65536);
        for (int i = 0; i < 65536; ++i) {
            float t = DirectX::PackedVector::XMConvertHalfToFloat(static_cast<uint16_t>(i));
            lut[i] = static_cast<uint16_t>(t * maximum + 0.5f);
        }
    }
}


ConvertShader::ConvertShader(PClip _child, int precision, bool stack16, std::string& format, bool planar, int opt, IScriptEnvironment* env) :
    GenericVideoFilter(_child), isPlusMt(false), buff(nullptr), useLut(false)
{
    name = format == "" ? "ConvertToShader" : "ConvertFromShader";

    arch_t arch = get_arch(opt);

    if (name == "ConvertToShader") {
        constructToShader(precision, stack16, planar, arch);
    } else {
        constructFromShader(precision, stack16, format, arch);
    }

    if (!mainProc) {
        env->ThrowError("%s: not implemented yet.", name.c_str());
    }

    if (precision == 3 && !useLut) {
        isPlusMt = env->FunctionExists("SetFilterMTMode");
        if (!isPlusMt) { // if not avs+MT, allocate buffer at constructor.
            buff = static_cast<float*>(_aligned_malloc(floatBufferPitch, 32));
            if (!buff) {
                env->ThrowError("%s: Failed to allocate temporal buffer.", name.c_str());
            }
        }
    }
}


ConvertShader::~ConvertShader() {
    _aligned_free(buff);
}


PVideoFrame __stdcall ConvertShader::GetFrame(int n, IScriptEnvironment* env) {
    PVideoFrame src = child->GetFrame(n, env);

    PVideoFrame dst = env->NewVideoFrame(vi, 32);

    const uint8_t* srcp[] = {
        src->GetReadPtr(),
        viSrc.IsRGB() ? nullptr : src->GetReadPtr(PLANAR_U),
        viSrc.IsRGB() ? nullptr : src->GetReadPtr(PLANAR_V),
    };

    uint8_t* dstp[] = {
        dst->GetWritePtr(),
        vi.IsRGB() ? nullptr : dst->GetWritePtr(PLANAR_U),
        vi.IsRGB() ? nullptr : dst->GetWritePtr(PLANAR_V),
    };

    void* b = useLut ? reinterpret_cast<void*>(lut.data()) : buff;
    if (isPlusMt) { // if avs+MT, allocate buffer at every GetFrame() via buffer pool.
        env->Allocate(floatBufferPitch, 32, AVS_POOLED_ALLOC);
        if (!b) {
            env->ThrowError("%s Failed to allocate temporal buffer.", name.c_str());
        }
    }

    mainProc(dstp, srcp, dst->GetPitch(), src->GetPitch(), procWidth, procHeight, b);

    if (isPlusMt) {
        env->Free(b);
    }

    return dst;
}

int __stdcall ConvertShader::SetCacheHints(int cachehints, int frame_range) {
    return cachehints == CachePolicyHint::CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
}