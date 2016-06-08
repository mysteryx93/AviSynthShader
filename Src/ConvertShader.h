#pragma once
#include <cstdint>
#include <string>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#include <windows.h>
#endif
#include "avisynth.h"


// Converts YV12 data into RGB data with float precision, 12-byte per pixel.
class ConvertToShader : public GenericVideoFilter {
    VideoInfo viSrc;
    int floatBufferPitch;
    float* buff;
    bool isPlusMt;

    void(__stdcall* mainProc)(uint8_t* dstp, const uint8_t** srcp, const int dpitch, const int spitch, const int width, const int height, float* buff);

public:
    ConvertToShader(PClip _child, int _precision, bool stack16, IScriptEnvironment* env);
    ~ConvertToShader();
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);

};


// Converts float-precision RGB data (12-byte per pixel) into YV12 format.
class ConvertFromShader : public GenericVideoFilter {
    VideoInfo viSrc;
    bool isPlusMt;
    int floatBufferPitch;
    float* buff;

    void(__stdcall *mainProc)(
        uint8_t** dstp, const uint8_t* srcp, const int dpitch, const int spitch, const int width, const int height, float* buff);

public:
    ConvertFromShader(PClip _child, int _precision, std::string format, bool stack16, IScriptEnvironment* env);
    ~ConvertFromShader();
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);

};


extern bool has_sse2() noexcept;
extern bool has_f16c() noexcept;


enum arch_t {
    NO_SIMD,
    USE_SSE2,
    USE_F16C,
};

<<<<<<< a4d4e92ab5395c9a766964e34c59c3bb380d3c75:Src/ConvertToShader.h
=======

static inline arch_t get_arch()
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

>>>>>>> ConvertFromShader: optimized.:Src/ConvertShader.h
