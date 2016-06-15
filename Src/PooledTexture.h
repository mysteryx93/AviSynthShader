#pragma once
#include "d3d9.h"
#include "atlbase.h"
#include "D3D9Macros.h"
#include "avisynth.h"
#include <windows.h>

// Contains each element in the memory pool. The struct members match the MemoryPool::Allocate parameters.
// It will search for a PooledTexture with Available=true, and if it cannot be found, it will create a new one.
struct PooledTexture {
	bool Available;
	bool GpuTexture;
	int Width;
	int Height;
	bool RenderTarget;
	D3DFORMAT Format;
	CComPtr<IDirect3DTexture9> Texture;
	CComPtr<IDirect3DSurface9> Surface;
};