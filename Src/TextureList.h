#pragma once
#include "avisynth.h"
#include "D3D9Macros.h"
#include <vector>
#include <algorithm>
#include "InputTexture.h"
#include "MemoryPool.h"

class D3D9RenderImpl;

InputTexture* __stdcall FindTexture(std::vector<InputTexture*>* textureList, int clipIndex);
HRESULT __stdcall ReleaseTexture(InputTexture* obj);
HRESULT __stdcall RemoveTexture(std::vector<InputTexture*>* textureList, InputTexture* item);
HRESULT __stdcall ClearTextures(MemoryPool* pool, std::vector<InputTexture*>* textureList);
D3DFORMAT __stdcall GetD3DFormat(int precision, bool planar);
int __stdcall GetD3DFormatSize(int precision, bool planar);
int __stdcall AdjustPrecision(IScriptEnvironment* env, int precision);
HRESULT __stdcall CopyAviSynthToBuffer(const byte* src, int srcPitch, int clipPrecision, int width, int height, InputTexture* dst, IScriptEnvironment* env);
HRESULT __stdcall CopyAviSynthToPlanarBuffer(const byte* srcY, const byte* srcU, const byte* srcV, int srcPitch, int clipPrecision, int width, int height, InputTexture* dst, IScriptEnvironment* env);
HRESULT __stdcall CopyBufferToAviSynthInternal(IDirect3DSurface9* surface, byte* dst, int dstPitch, int rowSize, int height, IScriptEnvironment* env);
HRESULT __stdcall CopyBufferToAviSynth(int commandIndex, InputTexture* src, byte* dst, int dstPitch, int outputPrecision, IScriptEnvironment* env);
HRESULT __stdcall CopyBufferToAviSynthPlanar(int commandIndex, InputTexture* src, byte* dstY, byte* dstU, byte* dstV, int dstPitch, int outputPrecision, IScriptEnvironment* env);
