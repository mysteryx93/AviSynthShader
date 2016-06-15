#pragma once
#include "TextureList.h"

// Only one texture per ClipIndex should be kept in the list.
InputTexture* __stdcall FindTexture(std::vector<InputTexture*>* textureList, int clipIndex) {
	for (auto const& item : *textureList) {
		if (item->ClipIndex == clipIndex)
			return item;
	}
	return nullptr;
}

HRESULT __stdcall ReleaseTexture(InputTexture* obj) {
	HR(obj->Pool->Release(obj->Surface));
	HR(obj->Pool->Release(obj->Memory));
	HR(obj->Pool->Release(obj->SurfaceY));
	HR(obj->Pool->Release(obj->SurfaceU));
	HR(obj->Pool->Release(obj->SurfaceV));
	delete obj;
	return S_OK;
}

HRESULT __stdcall RemoveTexture(std::vector<InputTexture*>* textureList, InputTexture* item) {
	textureList->erase(std::remove(textureList->begin(), textureList->end(), item), textureList->end());
	HR(ReleaseTexture(item));
	return S_OK;
}

HRESULT __stdcall ClearTextures(MemoryPool* pool, std::vector<InputTexture*>* textureList) {
	for (auto const& item : *textureList) {
		if (item->Pool == pool)
			HR(ReleaseTexture(item));
	}
	textureList->clear();
	return S_OK;
}

D3DFORMAT __stdcall GetD3DFormat(int precision, bool planar) {
	if (precision == 0)
		return D3DFMT_L8;
	else if (precision == 1) {
		if (planar)
			return D3DFMT_L8;
		else
			return D3DFMT_A8R8G8B8;
	}
	else if (precision == 2) {
		if (planar)
			return D3DFMT_L16;
		else
			return D3DFMT_A16B16G16R16;
	}
	else if (precision == 3)
		if (planar)
			return D3DFMT_R16F;
		else
			return D3DFMT_A16B16G16R16F;
	else
		return D3DFMT_UNKNOWN;
}

int __stdcall GetD3DFormatSize(int precision, bool planar) {
	if (precision == 0)
		return 1;
	else if (precision == 1) {
		if (planar)
			return 1;
		else
			return 4;
	}
	else if (precision == 2) {
		if (planar)
			return 2;
		else
			return 8;
	}
	else if (precision == 3)
		if (planar)
			return 2;
		else
			return 8;
	else
		return 0;
}

// Returns how to multiply the AviSynth frame format's width based on the precision.
int __stdcall AdjustPrecision(IScriptEnvironment* env, int precision) {
	if (precision == 0)
		return 1; // Same width as Y8
	if (precision == 1)
		return 1; // Same width as RGB24
	else if (precision == 2)
		return 2; // Double width as RGB32
	else if (precision == 3)
		return 2; // Double width as RGB32
	else
		env->ThrowError("ExecuteShader: Precision must be 0, 1, 2 or 3");
	return 0;
}

HRESULT __stdcall CopyAviSynthToBuffer(const byte* src, int srcPitch, int clipPrecision, int width, int height, InputTexture* dst, IScriptEnvironment* env) {
	// Copies source frame into main surface buffer, or into additional input textures
	RECT SrcRect;
	SrcRect.top = 0;
	SrcRect.left = 0;
	SrcRect.right = width;
	SrcRect.bottom = height;
	HR(D3DXLoadSurfaceFromMemory(dst->Surface, nullptr, nullptr, src, GetD3DFormat(clipPrecision, false), srcPitch, nullptr, &SrcRect, D3DX_FILTER_NONE, 0));
	return S_OK;
}

HRESULT __stdcall CopyAviSynthToPlanarBuffer(const byte* srcY, const byte* srcU, const byte* srcV, int srcPitch, int clipPrecision, int width, int height, InputTexture* dst, IScriptEnvironment* env) {
	// Copies source frame into main surface buffer, or into additional input textures
	RECT SrcRect;
	SrcRect.top = 0;
	SrcRect.left = 0;
	SrcRect.right = width;
	SrcRect.bottom = height;
	D3DFORMAT fmt = GetD3DFormat(clipPrecision, true);
	HR(D3DXLoadSurfaceFromMemory(dst->SurfaceY, nullptr, nullptr, srcY, fmt, srcPitch, nullptr, &SrcRect, D3DX_FILTER_NONE, 0));
	HR(D3DXLoadSurfaceFromMemory(dst->SurfaceU, nullptr, nullptr, srcU, fmt, srcPitch, nullptr, &SrcRect, D3DX_FILTER_NONE, 0));
	HR(D3DXLoadSurfaceFromMemory(dst->SurfaceV, nullptr, nullptr, srcV, fmt, srcPitch, nullptr, &SrcRect, D3DX_FILTER_NONE, 0));
	return S_OK;
}

HRESULT __stdcall CopyBufferToAviSynthInternal(IDirect3DSurface9* surface, byte* dst, int dstPitch, int rowSize, int height, IScriptEnvironment* env) {
	D3DLOCKED_RECT srcRect;
	HR(surface->LockRect(&srcRect, nullptr, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY));
	BYTE* srcPict = (BYTE*)srcRect.pBits;
	env->BitBlt(dst, dstPitch, srcPict, srcRect.Pitch, rowSize, height);
	HR(surface->UnlockRect());
	return S_OK;
}

HRESULT __stdcall CopyBufferToAviSynth(int commandIndex, InputTexture* src, byte* dst, int dstPitch, int outputPrecision, IScriptEnvironment* env) {
	int Width = src->Width * GetD3DFormatSize(outputPrecision, false);
	HR(CopyBufferToAviSynthInternal(src->Memory, dst, dstPitch, Width, src->Height, env));
	return S_OK;
}

HRESULT __stdcall CopyBufferToAviSynthPlanar(int commandIndex, InputTexture* src, byte* dstY, byte* dstU, byte* dstV, int dstPitch, int outputPrecision, IScriptEnvironment* env) {
	int Width = src->Width * GetD3DFormatSize(outputPrecision, true);
	HR(CopyBufferToAviSynthInternal(src->SurfaceY, dstY, dstPitch, Width, src->Height, env));
	HR(CopyBufferToAviSynthInternal(src->SurfaceU, dstU, dstPitch, Width, src->Height, env));
	HR(CopyBufferToAviSynthInternal(src->SurfaceV, dstV, dstPitch, Width, src->Height, env));
	return S_OK;
}