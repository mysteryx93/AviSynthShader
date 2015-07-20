#pragma once

#include "d3d9.h"
#include <windows.h>
#include <map>
#include "Macros.h"

using namespace std;

class Overlay
{
public:
	Overlay(IDirect3DDevice9* device, D3DCOLOR color, BYTE opacity)
	{
		m_device = device;
		m_opacity = opacity;
		m_color = (color & 0x00FFFFFF) | (opacity << 24);
	}

	virtual ~Overlay(void)
	{

	}

	virtual HRESULT Draw(void) = 0;

protected:
	IDirect3DDevice9* m_device;
	D3DCOLOR m_color;
	BYTE m_opacity;
};

class LineOverlay : public Overlay
{
public:
	LineOverlay(IDirect3DDevice9* device, POINT p1, POINT p2, INT width, D3DCOLOR color, BYTE opacity)
		: Overlay(device, color, opacity)
	{
		HRESULT hr = D3DXCreateLine(m_device, &m_line);
		m_vectors[0].x = p1.x;
		m_vectors[0].y = p1.y;
		m_vectors[1].x = p2.x;
		m_vectors[1].y = p2.y;
		m_line->SetWidth(width);
	}

	virtual ~LineOverlay()
	{
		SafeRelease(m_line);
	}

	virtual HRESULT Draw(void)
	{
		HR(m_line->Begin());
		HR(m_line->Draw(m_vectors, 2, m_color));
		return m_line->End();
	}

private:
	CComPtr<ID3DXLine> m_line;
	D3DXVECTOR2 m_vectors[2];
};

class RectangleOverlay : public Overlay
{
public:
	RectangleOverlay(IDirect3DDevice9* device, RECT rectangle, INT width, D3DCOLOR color, BYTE opacity)
		: Overlay(device, color, opacity)
	{
		D3DXCreateLine(m_device, &m_line);
		m_line->SetWidth(width);

		m_vectors[0].x = rectangle.left;
		m_vectors[0].y = rectangle.top;
		m_vectors[1].x = rectangle.right;
		m_vectors[1].y = rectangle.top;
		m_vectors[2].x = rectangle.right;
		m_vectors[2].y = rectangle.bottom;
		m_vectors[3].x = rectangle.left;
		m_vectors[3].y = rectangle.bottom;
		m_vectors[4].x = rectangle.left;
		m_vectors[4].y = rectangle.top;
	}

	virtual ~RectangleOverlay()
	{
		SafeRelease(m_line);
	}

	virtual HRESULT Draw(void)
	{
		HR(m_line->Begin());
		HR(m_line->Draw(m_vectors, 5, m_color));
		return m_line->End();
	}

private:
	CComPtr<ID3DXLine> m_line;
	D3DXVECTOR2 m_vectors[5];
};

class PolygonOverlay : public Overlay
{
public:
	PolygonOverlay(IDirect3DDevice9* device, POINT* points, INT pointsLen, INT width, D3DCOLOR color, BYTE opacity)
		: Overlay(device, color, opacity) 
	{
		HRESULT hr = D3DXCreateLine(m_device, &m_line);
		m_vectors = new D3DXVECTOR2[pointsLen + 1];
		for(int i = 0 ; i < pointsLen; i++)
		{
			m_vectors[i].x = (float)points[i].x;
			m_vectors[i].y = (float)points[i].y;
		}

		m_vectors[pointsLen].x = (float)points[0].x;
		m_vectors[pointsLen].y = (float)points[0].y;

		m_numOfVectors = pointsLen + 1;
		m_line->SetWidth((float)width);
	}

	virtual ~PolygonOverlay()
	{
		delete m_vectors;
		SafeRelease(m_line);
	}

	virtual HRESULT Draw(void)
	{
		HR(m_line->Begin());
		HR(m_line->Draw(m_vectors, m_numOfVectors, m_color));
		return m_line->End();
	}

private:
	CComPtr<ID3DXLine> m_line;
	D3DXVECTOR2* m_vectors;
	INT m_numOfVectors;
};

class TextOverlay : public Overlay
{
public:
	TextOverlay(IDirect3DDevice9* device, LPCSTR text, RECT pos, INT size, D3DCOLOR color, LPCSTR font, BYTE opacity)
		: Overlay(device, color, opacity)
	{
		HRESULT hr = D3DXCreateFont( m_device, size, 0, FW_BOLD, 0, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, font, &m_font );
		m_pos = pos;
		m_text = text;
	}

	virtual HRESULT Draw(void)
	{
		return m_font->DrawTextA(NULL, m_text, -1, &m_pos, 0, m_color );
	}

private:
	CComPtr<ID3DXFont> m_font;
	LPCSTR m_text;
	RECT m_pos;
};

class BitmapOverlay : public Overlay
{
public:
	BitmapOverlay(IDirect3DDevice9* device, POINT position, INT width, INT height, BYTE* pPixelData, BYTE opacity)
		: Overlay(device, D3DCOLOR_ARGB(0xff, 0, 0, 0), opacity)
	{
		VERTEX vertexArray[] =
		{
			{ D3DXVECTOR3((float)position.x, (float)position.y, 0),                  D3DCOLOR_ARGB(opacity, 255, 255, 255), D3DXVECTOR2(0, 0) },  // top left
			{ D3DXVECTOR3((float)position.x + width, (float)position.y, 0),          D3DCOLOR_ARGB(opacity, 255, 255, 255), D3DXVECTOR2(1, 0) },  // top right
			{ D3DXVECTOR3((float)position.x + width, (float)position.y + height, 0), D3DCOLOR_ARGB(opacity, 255, 255, 255), D3DXVECTOR2(1, 1) },  // bottom right
			{ D3DXVECTOR3((float)position.x, (float)position.y + height, 0),         D3DCOLOR_ARGB(opacity, 255, 255, 255), D3DXVECTOR2(0, 1) },  // bottom left
		};

		HRESULT hr = m_device->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_pTexture, NULL);
		hr = m_device->CreateVertexBuffer(sizeof(VERTEX) * 4, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &m_pVertexBuffer, NULL);
	
		D3DLOCKED_RECT lock;
        hr = m_pTexture->LockRect(0, &lock, NULL, 0);
		BYTE* target = (BYTE*)lock.pBits;
		
		for(int y = 0; y < height; y++)
		{
			memcpy(target, pPixelData, width * 4);
			target += lock.Pitch;
			pPixelData += width * 4;
		}

		hr = m_pTexture->UnlockRect(0);

		VERTEX *vertices;
		hr = m_pVertexBuffer->Lock(0, 0, (void**)&vertices, D3DLOCK_DISCARD);
		memcpy(vertices, vertexArray, sizeof(vertexArray));
		hr = m_pVertexBuffer->Unlock();	
	}

	virtual ~BitmapOverlay()
	{
		SafeRelease(m_pVertexBuffer);
		SafeRelease(m_pTexture);
	}

	virtual HRESULT Draw(void)
	{
		HRESULT hr = S_OK;

		hr = m_device->SetPixelShader(NULL);
		hr = m_device->SetVertexShader(NULL);
		hr = m_device->SetTexture(0, m_pTexture);
		hr = m_device->SetStreamSource(0, m_pVertexBuffer, 0, sizeof(VERTEX));
		hr = m_device->SetFVF(D3DFVF_CUSTOMVERTEX);
		hr = m_device->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);

		return hr;
	}

private:
	CComPtr<IDirect3DTexture9> m_pTexture;
	CComPtr<IDirect3DVertexBuffer9> m_pVertexBuffer;
};

class OverlayStore
{
	typedef map<SHORT, Overlay*> OverlayMap;

public:
	OverlayStore()
	{
		InitializeCriticalSection(&m_lock);
	}

	virtual ~OverlayStore()
	{
		RemoveAll();
		DeleteCriticalSection(&m_lock);
	}

	void AddOverlay(Overlay* pOverlay, SHORT id)
	{
		EnterCriticalSection(&m_lock); 
		m_overlays[id] = pOverlay;
		LeaveCriticalSection(&m_lock);
	}

	void RemoveOverlay(SHORT id)
	{
		EnterCriticalSection(&m_lock); 
		m_overlays.erase(id);
		LeaveCriticalSection(&m_lock);
	}

	void Draw()
	{
		if(IsEmpty())
		{
			return;
		}

		EnterCriticalSection(&m_lock); 
		for each(pair<SHORT, Overlay*> pair in m_overlays )
		{
			pair.second->Draw();
		}
		LeaveCriticalSection(&m_lock);
	}

	bool IsEmpty()
	{
		bool isEmpty = false;
		EnterCriticalSection(&m_lock); 
		isEmpty = m_overlays.empty();
		LeaveCriticalSection(&m_lock);

		return isEmpty;
	}

	void RemoveAll()
	{
		EnterCriticalSection(&m_lock); 
		m_overlays.clear();
		LeaveCriticalSection(&m_lock);
	}

private:
	OverlayMap m_overlays;
	CRITICAL_SECTION m_lock; 
};