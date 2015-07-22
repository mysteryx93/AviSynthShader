#pragma once

template <typename T>
inline void SafeRelease(T& p)
{
    if (NULL != p)
    {
        p.Release();
        p = NULL;
    }
}

#define HR(x) if(FAILED(x)) { return x; }

#define SCENE_HR(hr, m_pDevice) if(FAILED(hr)) { m_pDevice->EndScene(); return hr; }

#define D3DFMT_YV12 (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2')
#define D3DFMT_NV12 (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2')

enum FillMode
{
	KeepAspectRatio = 0,
	Fill = 1
};