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

HRESULT MemoryPool::Allocate(CComPtr<IDirect3DDevice9Ex> device, bool gpuTexture, int width, int height, bool renderTarget, D3DFORMAT format, CComPtr<IDirect3DTexture9> &texture, CComPtr<IDirect3DSurface9> &surface) {
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
	if (gpuTexture) {
		HR(device->CreateTexture(width, height, 1, renderTarget ? D3DUSAGE_RENDERTARGET : NULL, format, D3DPOOL_DEFAULT, &texture, NULL));
		HR(texture->GetSurfaceLevel(0, &surface));
	}
	else {
		HR(device->CreateOffscreenPlainSurface(width, height, format, D3DPOOL_SYSTEMMEM, &surface, NULL));
	}

	// Add to memory pool.
	PooledTexture* NewObj = new PooledTexture();
	NewObj->Available = false;
	NewObj->GpuTexture = gpuTexture;
	NewObj->Width = width;
	NewObj->Height = height;
	NewObj->RenderTarget = renderTarget;
	NewObj->Format = format;
	NewObj->Texture = texture != NULL ? texture : NULL;
	NewObj->Surface = surface != NULL ? surface : NULL;
	m_mutex.lock();
	m_Pool.push_back(NewObj);
	m_mutex.unlock();

	return S_OK;
}

HRESULT MemoryPool::Release(IDirect3DSurface9 *surface) {
	if (surface == NULL)
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
