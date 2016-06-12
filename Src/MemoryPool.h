#pragma once
#include <windows.h>
#include <d3d9.h>
#include <cstdio>		//needed by OutputDebugString()
#include "atlbase.h"
#include "D3D9Macros.h"

/* Creates DX9 textures and surfaces on-demand and store them in a pool for re-use. */

class MemoryPool {
public:
	MemoryPool(CComPtr<IDirect3DDevice9Ex> device);
	~MemoryPool();
	HRESULT Allocate(bool gpuTexture, int width, int height, bool renderTarget, D3DFORMAT format, IDirect3DTexture9 **texture, IDirect3DSurface9 **surface);
	HRESULT Release(IDirect3DSurface9 *surface);
private:
	CComPtr<IDirect3DDevice9Ex> m_pDevice;
};