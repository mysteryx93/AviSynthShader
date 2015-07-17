#pragma once

#define D3D_DEBUG_INFO
#include "d3d9.h"
#include "Vertex.h"
#include "atlbase.h"
#include "Macros.h"
#include <windows.h>
#include "Overlay.h"

class D3D9RenderImpl // : public IRenderable
{
public:
	D3D9RenderImpl();
	~D3D9RenderImpl(void);

	HRESULT Initialize(HWND hDisplayWindow);
	HRESULT CheckFormatConversion(D3DFORMAT format);
	HRESULT CreateVideoSurface(int width, int height, int precision);
	HRESULT ProcessFrame(const byte* src, int srcPitch, byte* dst, int dstPitch);
	HRESULT Display(BYTE* pYplane, BYTE* pVplane, BYTE* pUplane);
	void SetDisplayMode(FillMode mode);
    FillMode GetDisplayMode();

	HRESULT SetPixelShader(LPCSTR pPixelShaderName, LPCSTR entryPoint, LPCSTR shaderModel, LPSTR* ppError);
	HRESULT SetPixelShader(DWORD* buffer);
	HRESULT SetPixelShaderConstant(LPCSTR name, LPVOID value, UINT size);

	HRESULT SetVertexShader(LPCSTR pVertexShaderName, LPCSTR entryPoint, LPCSTR shaderModel, LPSTR* ppError);
	HRESULT SetVertexShader(DWORD* buffer);
	HRESULT SetVertexShaderConstant(LPCSTR name, LPVOID value, UINT size);
	HRESULT ApplyWorldViewProj(LPCSTR matrixName);

	HRESULT SetVertexShaderMatrix(D3DXMATRIX* matrix, LPCSTR name);
	HRESULT SetPixelShaderMatrix(D3DXMATRIX* matrix, LPCSTR name);
	HRESULT SetVertexShaderVector(D3DXVECTOR4* vector, LPCSTR name);
	HRESULT SetPixelShaderVector(D3DXVECTOR4* vector, LPCSTR name);

	HRESULT ClearPixelShader();
	HRESULT ClearVertexShader();

	HRESULT CaptureDisplayFrame(BYTE* pBuffer, INT* width, INT* height, INT* stride);
	HRESULT CaptureVideoFrame(BYTE* pBuffer, INT* width, INT* height, INT* stride);

private:
	CComPtr<IDirect3D9>             m_pD3D9;
	CComPtr<IDirect3DDevice9>       m_pDevice;
	CComPtr<IDirect3DSurface9>      m_pOffsceenSurface;
	CComPtr<IDirect3DSurface9>      m_pTextureSurface;
	CComPtr<IDirect3DTexture9>      m_pTexture;
	CComPtr<IDirect3DVertexBuffer9> m_pVertexBuffer;
	CComPtr<IDirect3DVertexShader9> m_pVertexShader; 
	CComPtr<ID3DXConstantTable>     m_pVertexConstantTable; 
	CComPtr<ID3DXConstantTable>     m_pPixelConstantTable; 
	CComPtr<IDirect3DPixelShader9>  m_pPixelShader;

	D3DDISPLAYMODE m_displayMode;
	HWND m_hDisplayWindow;
	int m_videoWidth;
	int m_videoHeight;
	int m_precision;
	D3DFORMAT m_format;
	OverlayStore m_overlays;
	FillMode m_fillMode;
	D3DPRESENT_PARAMETERS m_presentParams;

private:
	HRESULT SetupMatrices(int width, int height);
	HRESULT CreateScene(void);
	HRESULT CheckDevice(void);
	HRESULT CopyToBuffer(const byte* src, int srcPitch);
	HRESULT CopyFromBuffer(byte* dst, int dstPitch);
	HRESULT FillBuffer(const BYTE* pY, BYTE* pV, BYTE* pU);
	HRESULT CreateRenderTarget(int width, int height);
	HRESULT Present(void);	
	HRESULT GetPresentParams(D3DPRESENT_PARAMETERS* params, BOOL bWindowed);

	HRESULT DiscardResources();
	HRESULT CreateResources();

public:
	HRESULT DrawLine(     SHORT key, POINT p1, POINT p2, FLOAT width = 2, D3DCOLOR color = D3DCOLOR_XRGB(255, 255, 255), BYTE opacity = 255);
	HRESULT DrawRectangle(SHORT key, RECT rectangle, FLOAT width, D3DCOLOR color = D3DCOLOR_XRGB(255, 255, 255), BYTE opacity = 255);
	HRESULT DrawPolygon  (SHORT key, POINT* points, INT pointsLen, FLOAT width, D3DCOLOR color = D3DCOLOR_XRGB(255, 255, 255), BYTE opacity = 255);
	HRESULT DrawText(     SHORT key, LPCSTR text, RECT pos, INT size = 20, D3DCOLOR color = D3DCOLOR_XRGB(255, 255, 255), LPCSTR font = "Ariel", BYTE opacity = 255);
	HRESULT DrawBitmap(   SHORT key, POINT position, INT width, INT height, BYTE* pPixelData, BYTE opacity = 255);
	HRESULT RemoveOverlay(SHORT key);
	HRESULT RemoveAllOverlays();
};

