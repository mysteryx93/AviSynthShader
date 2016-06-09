#include "ConvertShader.h"



ConvertToShader::ConvertToShader(PClip _child, int precision, bool stack16, bool planar, int opt, IScriptEnvironment* env) :
    GenericVideoFilter(_child), isPlusMt(false), buff(nullptr)
{
    arch_t arch = get_arch(opt);
    if (precision != 3 && arch > USE_SSE2) {
        arch = USE_SSE2;
    }

    if (vi.IsRGB24() && !planar && arch != NO_SIMD) {   // precision must be 2 or 3
        child = env->Invoke("ConvertToRGB32", child).AsClip();
        vi = child->GetVideoInfo();
    }

    viSrc = vi;

    vi.pixel_type = planar ? VideoInfo::CS_YV24 : VideoInfo::CS_BGR32;

    if (precision > 1) {    // Half-float frame has its width twice larger than normal
        vi.width *= 2;
    }

    if (stack16) {
        vi.height /= 2;
    }

    mainProc = planar ? get_to_shader_planar(precision, viSrc.pixel_type, stack16, arch)
        : get_to_shader_packed(precision, viSrc.pixel_type, stack16, arch);
    if (!mainProc) {
        env->ThrowError("ConvertToShader: not impplemented yet.");
    }

    if (precision == 3) {
        floatBufferPitch = (vi.width * 4 * 4 + 63) & ~63; // must be mod64
        isPlusMt = env->FunctionExists("SetFilterMTMode");
        if (!isPlusMt) { // if not avs+MT, allocate buffer at constructor.
            buff = static_cast<float*>(_aligned_malloc(floatBufferPitch, 32));
            if (!buff) {
                env->ThrowError("ConvertToShader: Failed to allocate temporal buffer.");
            }
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

    uint8_t* dstp[] = {
        dst->GetWritePtr(),
        vi.IsRGB() ? nullptr : dst->GetWritePtr(PLANAR_U),
        vi.IsRGB() ? nullptr : dst->GetWritePtr(PLANAR_V),
    };

    float* b = buff;
    if (isPlusMt) { // if avs+MT, allocate buffer at every GetFrame() via buffer pool.
        void* t = static_cast<IScriptEnvironment2*>(env)->Allocate(floatBufferPitch, 32, AVS_POOLED_ALLOC);
        if (!t) {
            env->ThrowError("ConvertToShader: Failed to allocate temporal buffer.");
        }
        b = reinterpret_cast<float*>(t);
    }

    mainProc(dstp, srcp, dst->GetPitch(), src->GetPitch(), viSrc.width, vi.height, b);

    if (isPlusMt) {
        static_cast<IScriptEnvironment2*>(env)->Free(b);
    }

    return dst;
}
