#pragma once
#include <windows.h>
#include <d3d9.h>
#include <cstdio>		//needed by OutputDebugString()
#include "atlbase.h"
#include "D3D9Macros.h"
#include "PooledTexture.h"
#include <vector>
#include <algorithm>
#include <mutex>

/* Creates DX9 textures and surfaces on-demand and store them in a pool for re-use. */

class MemoryPool {
public:
	MemoryPool();
	~MemoryPool();
	HRESULT AllocateTexture(CComPtr<IDirect3DDevice9Ex> device, int width, int height, bool renderTarget, D3DFORMAT format, CComPtr<IDirect3DTexture9> &texture, CComPtr<IDirect3DSurface9> &surface);
	HRESULT AllocatePlainSurface(CComPtr<IDirect3DDevice9Ex> device, int width, int height, D3DFORMAT format, CComPtr<IDirect3DSurface9> &surface);
	HRESULT Release(IDirect3DSurface9 *surface);
private:
	HRESULT AllocateInternal(CComPtr<IDirect3DDevice9Ex> device, bool gpuTexture, int width, int height, bool renderTarget, D3DFORMAT format, CComPtr<IDirect3DTexture9> &texture, CComPtr<IDirect3DSurface9> &surface);
	std::vector<PooledTexture*> m_Pool;
	//CComPtr<IDirect3DDevice9Ex> m_pDevice;
	std::mutex m_mutex;
};