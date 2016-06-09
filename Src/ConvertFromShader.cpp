#include <algorithm>
#include <map>
#include <tuple>

#include "ConvertShader.h"





ConvertFromShader::ConvertFromShader(PClip _child, int precision, std::string format, bool stack16, int opt, IScriptEnvironment* env) :
    GenericVideoFilter(_child), buff(nullptr), isPlusMt(false)
{
    arch_t arch = get_arch(opt);
    if (precision != 3 && arch > USE_SSE2) {
        arch = USE_SSE2;
    }

    viSrc = vi;

    if (format == "RGB32") {
        vi.pixel_type = VideoInfo::CS_BGR32;
    }
    else if (format == "RGB24") {
        vi.pixel_type = VideoInfo::CS_BGR24;
        arch = NO_SIMD;
    }
    else {
        vi.pixel_type = VideoInfo::CS_YV24;
    }

    if (stack16) {      // Stack16 frame has twice the height
        vi.height *= 2;
    }
    if (precision > 1) {    // UINT16 frame has twice the width
        vi.width /= 2;
    }

    mainProc = viSrc.IsRGB() ? get_from_shader_packed(precision, vi.pixel_type, stack16, arch)
        : get_from_shader_planar(precision, vi.pixel_type, stack16, arch);
    if (!mainProc) {
        env->ThrowError("ConvertFromShader: not impplemented yet.");
    }

    if (precision == 3) {
        floatBufferPitch = (viSrc.width * 8 + 63) & ~63; // must be mod64
        isPlusMt = env->FunctionExists("SetFilterMTMode");
        if (!isPlusMt) { // if not avs+MT, allocate buffer at constructor.
            buff = reinterpret_cast<float*>(_aligned_malloc(floatBufferPitch, 32));
            if (!buff) {
                env->ThrowError("ConvertFromShader: failed to allocate buffer.");
            }
        }
    }
}


ConvertFromShader::~ConvertFromShader() {
    _aligned_free(buff);
}


PVideoFrame __stdcall ConvertFromShader::GetFrame(int n, IScriptEnvironment* env) {
    PVideoFrame src = child->GetFrame(n, env);

    PVideoFrame dst = env->NewVideoFrame(vi);

    float* b = buff;
    if (isPlusMt) { // if avs+MT, allocate buffer at every GetFrame() via buffer pool.
        void* t = static_cast<IScriptEnvironment2*>(env)->Allocate(floatBufferPitch, 32, AVS_POOLED_ALLOC);
        if (!t) {
            env->ThrowError("ConvertFromShader: failed to allocate buffer.");
        }
        b = reinterpret_cast<float*>(t);
    }

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

    mainProc(dstp, srcp, dst->GetPitch(), src->GetPitch(), vi.width, viSrc.height, b);

    if (isPlusMt) {
        static_cast<IScriptEnvironment2*>(env)->Free(b);
    }

    return dst;
}
