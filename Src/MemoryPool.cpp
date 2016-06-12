#include "MemoryPool.h"

MemoryPool::MemoryPool(CComPtr<IDirect3DDevice9Ex> device) {
	m_pDevice = device;
}

MemoryPool::~MemoryPool() {
}

HRESULT MemoryPool::Allocate(bool gpuTexture, int width, int height, bool renderTarget, D3DFORMAT format, IDirect3DTexture9 **texture, IDirect3DSurface9 **surface) {
	if (gpuTexture) {
		HR(m_pDevice->CreateTexture(width, height, 1, renderTarget ? D3DUSAGE_RENDERTARGET : NULL, format, D3DPOOL_DEFAULT, texture, NULL));
		HR((*texture)->GetSurfaceLevel(0, surface));
	}
	else {
		HR(m_pDevice->CreateOffscreenPlainSurface(width, height, format, D3DPOOL_SYSTEMMEM, surface, NULL));
	}

	return S_OK;
}

HRESULT MemoryPool::Release(IDirect3DSurface9 *surface) {
	// Release managed by CComPtr
	// SafeRelease(surface);
	return S_OK;
}
