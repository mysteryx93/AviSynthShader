#include "MemoryPool.h"

MemoryPool::MemoryPool() {
}

MemoryPool::~MemoryPool() {
	for (auto const item : m_Pool) {
		SafeRelease(item->Texture);
		SafeRelease(item->Surface);
		delete item;
	}
	m_Pool.clear();
}

HRESULT MemoryPool::AllocateInternal(CComPtr<IDirect3DDevice9Ex> device, bool gpuTexture, int width, int height, bool renderTarget, D3DFORMAT format, CComPtr<IDirect3DTexture9> &texture, CComPtr<IDirect3DSurface9> &surface) {
	// Textures must be created by the device that will use them; thus, a memory pool is required for each device.

	m_mutex.lock();
	// Search for available texture in pool.
	for (auto const item : m_Pool) {
		if (item->Available == true && item->GpuTexture == gpuTexture && item->Width == width && item->Height == height && item->RenderTarget == renderTarget && item->Format == format) {
			item->Available = false;
			texture = item->Texture;
			surface = item->Surface;
			m_mutex.unlock();
			return S_OK;
		}
	}
	m_mutex.unlock();

	// If not found, create it
	// Note: CreateOffscreenPlainSurface fails for L8 format on NVidia cards but CreateTexture works.
	HR(device->CreateTexture(width, height, 1, renderTarget ? D3DUSAGE_RENDERTARGET : NULL, format, gpuTexture ? D3DPOOL_DEFAULT : D3DPOOL_SYSTEMMEM, &texture, nullptr));
	HR(texture->GetSurfaceLevel(0, &surface));

	// Add to memory pool.
	PooledTexture* NewObj = new PooledTexture();
	NewObj->Available = false;
	NewObj->GpuTexture = gpuTexture;
	NewObj->Width = width;
	NewObj->Height = height;
	NewObj->RenderTarget = renderTarget;
	NewObj->Format = format;
	NewObj->Texture = texture ? texture : nullptr;
	NewObj->Surface = surface ? surface : nullptr;
	m_mutex.lock();
	m_Pool.push_back(NewObj);
	m_mutex.unlock();

	return S_OK;
}

HRESULT MemoryPool::AllocateTexture(CComPtr<IDirect3DDevice9Ex> device, int width, int height, bool renderTarget, D3DFORMAT format, CComPtr<IDirect3DTexture9> &texture, CComPtr<IDirect3DSurface9> &surface) {
	return AllocateInternal(device, true, width, height, renderTarget, format, texture, surface);
}

HRESULT MemoryPool::AllocatePlainSurface(CComPtr<IDirect3DDevice9Ex> device, int width, int height, D3DFORMAT format, CComPtr<IDirect3DSurface9> &surface) {
	return AllocateInternal(device, false, width, height, false, format, CComPtr<IDirect3DTexture9>(nullptr), surface);
}

HRESULT MemoryPool::Release(IDirect3DSurface9 *surface) {
	if (!surface)
		return S_OK;

	std::unique_lock<std::mutex> my_lock(m_mutex);

	for (auto const item : m_Pool) {
		if (item->Surface == surface) {
			item->Available = true;
			return S_OK;
		}
	}

	return E_FAIL;
}
