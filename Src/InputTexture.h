#include "atlbase.h"
#include "d3d9.h"
#include "MemoryPool.h"
#pragma once

struct InputTexture {
	int ClipIndex;
	int Width, Height;
	MemoryPool* Pool; // The pool containing this texture
	CComPtr<IDirect3DSurface9> Memory;
	CComPtr<IDirect3DTexture9> Texture;
	CComPtr<IDirect3DSurface9> Surface;
	CComPtr<IDirect3DTexture9> TextureY;
	CComPtr<IDirect3DSurface9> SurfaceY;
	CComPtr<IDirect3DTexture9> TextureU;
	CComPtr<IDirect3DSurface9> SurfaceU;
	CComPtr<IDirect3DTexture9> TextureV;
	CComPtr<IDirect3DSurface9> SurfaceV;
};