#pragma once
#include "d3dx9.h"

template <typename T>
inline void SafeRelease(T& p) {
	if (p) {
		p.Release();
		p = nullptr;
	}
}

#define HR(x) if(FAILED(x)) { return x; }
#define SCENE_HR(hr, m_pDevice) if(FAILED(hr)) { m_pDevice->EndScene(); return hr; }

struct VERTEX
{
	float x, y, z, rhw; // the transformed(screen space) position for the vertex
	float tu, tv; // texture coordinates
};