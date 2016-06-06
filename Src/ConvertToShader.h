#pragma once
#include <cstdint>
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