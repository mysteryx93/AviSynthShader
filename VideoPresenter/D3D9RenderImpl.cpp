// Code taken from Taygeta Video Presender available here under LGPL3 license.The code has been slightly edited for our needs.
// http://www.codeproject.com/Articles/207642/Video-Shadering-with-Direct-D

#include "D3D9RenderImpl.h"


D3D9RenderImpl::D3D9RenderImpl()
	: m_pVertexShader(0), m_pPixelShader(0), m_pD3D9(0), m_format(D3DFMT_UNKNOWN), m_pVertexConstantTable(0), m_pPixelConstantTable(0)
{
}

HRESULT D3D9RenderImpl::Initialize(HWND hDisplayWindow, int width, int height, int precision)
{
	m_hDisplayWindow = hDisplayWindow;
	m_videoWidth = width;
	m_videoHeight = height;
	m_precision = precision;
	if (precision == 4)
		m_format = D3DFMT_A32B32G32R32F;
	else if (precision == 1)
		m_format = D3DFMT_X8R8G8B8;
	else
		return E_FAIL;

	m_pD3D9 = Direct3DCreate9(D3D_SDK_VERSION);
	if (!m_pD3D9) {
		return E_FAIL;
	}

	HR(m_pD3D9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &m_displayMode));

	D3DCAPS9 deviceCaps;
	HR(m_pD3D9->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &deviceCaps));

	DWORD dwBehaviorFlags = D3DCREATE_MULTITHREADED;

	if (deviceCaps.VertexProcessingCaps != 0)
		dwBehaviorFlags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
	else
		dwBehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;

	HR(GetPresentParams(&m_presentParams, TRUE));

	HR(m_pD3D9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hDisplayWindow, dwBehaviorFlags, &m_presentParams, &m_pDevice));

	HR(CreateResources());
	return S_OK;
}

D3D9RenderImpl::~D3D9RenderImpl(void)
{
	DiscardResources();
}


HRESULT D3D9RenderImpl::ProcessFrame(byte* dst, int dstPitch)
{
	HR(CheckDevice());
	HR(CreateScene());
	HR(Present());
	return CopyFromRenderTarget(dst, dstPitch);
}


HRESULT D3D9RenderImpl::CheckFormatConversion(D3DFORMAT format)
{
	HR(m_pD3D9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_displayMode.Format, 0, D3DRTYPE_SURFACE, format));

	return m_pD3D9->CheckDeviceFormatConversion(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, format, m_displayMode.Format);
}


HRESULT D3D9RenderImpl::CreateRenderTarget()
{
	HR(m_pDevice->CreateTexture(m_videoWidth, m_videoHeight, 1, D3DUSAGE_RENDERTARGET, m_format, D3DPOOL_DEFAULT, &m_pRenderTarget, NULL));
	HR(m_pRenderTarget->GetSurfaceLevel(0, &m_pRenderTargetSurface));
	HR(m_pDevice->CreateVertexBuffer(sizeof(VERTEX) * 4, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &m_pVertexBuffer, NULL));

	VERTEX vertexArray[] =
	{
		{ D3DXVECTOR3(0, 0, 0), D3DCOLOR_ARGB(255, 255, 255, 255), D3DXVECTOR2(0, 0) },  // top left
		{ D3DXVECTOR3((float)m_videoWidth, 0, 0), D3DCOLOR_ARGB(255, 255, 255, 255), D3DXVECTOR2(1, 0) },  // top right
		{ D3DXVECTOR3((float)m_videoWidth, (float)m_videoHeight, 0), D3DCOLOR_ARGB(255, 255, 255, 255), D3DXVECTOR2(1, 1) },  // bottom right
		{ D3DXVECTOR3(0, (float)m_videoHeight, 0), D3DCOLOR_ARGB(255, 255, 255, 255), D3DXVECTOR2(0, 1) },  // bottom left
	};

	VERTEX *vertices;
	HR(m_pVertexBuffer->Lock(0, 0, (void**)&vertices, D3DLOCK_DISCARD));

	memcpy(vertices, vertexArray, sizeof(vertexArray));

	HR(m_pVertexBuffer->Unlock());

	return m_pDevice->SetRenderTarget(0, m_pRenderTargetSurface);
	return S_OK;
}

HRESULT D3D9RenderImpl::CreateInputTexture(int index) {
	if (index < 0 || index >= maxTextures)
		return E_FAIL;

	HR(m_pDevice->CreateOffscreenPlainSurface(m_videoWidth, m_videoHeight, m_format, D3DPOOL_DEFAULT, &m_InputTextures[index].Memory, NULL));
	HR(m_pDevice->ColorFill(m_InputTextures[index].Memory, NULL, D3DCOLOR_ARGB(0xFF, 0, 0, 0)));

	HR(m_pDevice->CreateTexture(m_videoWidth, m_videoHeight, 1, 0, m_format, D3DPOOL_DEFAULT, &m_InputTextures[index].Texture, NULL));
	HR(m_InputTextures[index].Texture->GetSurfaceLevel(0, &m_InputTextures[index].Surface));

	return S_OK;
}


HRESULT D3D9RenderImpl::SetupMatrices(int width, int height)
{
	D3DXMATRIX matOrtho;
	D3DXMATRIX matIdentity;

	D3DXMatrixOrthoOffCenterLH(&matOrtho, 0, (float)width, (float)height, 0, 0.0f, 1.0f);
	D3DXMatrixIdentity(&matIdentity);

	HR(m_pDevice->SetTransform(D3DTS_PROJECTION, &matOrtho));
	HR(m_pDevice->SetTransform(D3DTS_WORLD, &matIdentity));
	return m_pDevice->SetTransform(D3DTS_VIEW, &matIdentity);
}

HRESULT D3D9RenderImpl::CreateScene(void)
{
	HRESULT hr = m_pDevice->Clear(D3DADAPTER_DEFAULT, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
	HR(m_pDevice->BeginScene());
	SCENE_HR(m_pDevice->SetFVF(D3DFVF_CUSTOMVERTEX), m_pDevice);
	SCENE_HR(m_pDevice->SetVertexShader(m_pVertexShader), m_pDevice);
	SCENE_HR(m_pDevice->SetPixelShader(m_pPixelShader), m_pDevice);
	SCENE_HR(m_pDevice->SetStreamSource(0, m_pVertexBuffer, 0, sizeof(VERTEX)), m_pDevice);

	for (int i = 0; i < maxTextures; i++) {
		if (m_InputTextures[i].Texture != NULL)
			SCENE_HR(m_pDevice->SetTexture(i, m_InputTextures[i].Texture), m_pDevice);
	}

	SCENE_HR(m_pDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2), m_pDevice);
	return m_pDevice->EndScene();
}


HRESULT D3D9RenderImpl::CheckDevice(void)
{
	return m_pDevice->TestCooperativeLevel();
}

HRESULT D3D9RenderImpl::DiscardResources()
{
	for (int i = 0; i < maxTextures; i++) {
		SafeRelease(m_InputTextures[0].Surface);
		SafeRelease(m_InputTextures[0].Texture);
		SafeRelease(m_InputTextures[0].Memory);
	}

	SafeRelease(m_pRenderTargetSurface);
	SafeRelease(m_pRenderTarget);
	SafeRelease(m_pVertexBuffer);
	SafeRelease(m_pVertexShader);
	SafeRelease(m_pVertexConstantTable);
	SafeRelease(m_pPixelConstantTable);
	SafeRelease(m_pPixelShader);

	return S_OK;
}

HRESULT D3D9RenderImpl::CreateResources()
{
	m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	m_pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
	m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
	m_pDevice->SetRenderState(D3DRS_DITHERENABLE, TRUE);

	m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_SPECULAR);

	m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	HR(CreateRenderTarget());

	return(SetupMatrices(m_presentParams.BackBufferWidth, m_presentParams.BackBufferHeight));
}

HRESULT D3D9RenderImpl::CopyToBuffer(const byte* src, int srcPitch, int index) {
	// Copies source frame into main surface buffer, or into additional input textures
	CComPtr<IDirect3DSurface9> destSurface = m_InputTextures[index].Memory;
	if (index < 0 || index >= maxTextures)
		return E_FAIL;

	D3DLOCKED_RECT d3drect;
	HR(destSurface->LockRect(&d3drect, NULL, 0));
	BYTE* pict = (BYTE*)d3drect.pBits;

	for (int y = 0; y < m_videoHeight; y++) {
		memcpy(pict, src, m_videoWidth * m_precision * 4);
		pict += d3drect.Pitch;
		src += srcPitch;
	}

	return  destSurface->UnlockRect();
}


HRESULT D3D9RenderImpl::Present(void)
{
	HR(m_pDevice->ColorFill(m_pRenderTargetSurface, NULL, D3DCOLOR_ARGB(0xFF, 0, 0, 0)));
	HR(m_pDevice->StretchRect(m_InputTextures[0].Memory, NULL, m_pRenderTargetSurface, NULL, D3DTEXF_POINT));
	return m_pDevice->Present(NULL, NULL, NULL, NULL);
}


HRESULT D3D9RenderImpl::GetPresentParams(D3DPRESENT_PARAMETERS* params, BOOL bWindowed)
{
	UINT height, width;

	if (bWindowed) // windowed mode
	{
		RECT rect;
		::GetClientRect(m_hDisplayWindow, &rect);
		height = rect.bottom - rect.top;
		width = rect.right - rect.left;
	}
	else   // fullscreen mode
	{
		width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
		height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	}

	D3DPRESENT_PARAMETERS presentParams = { 0 };
	presentParams.Flags = D3DPRESENTFLAG_VIDEO;
	presentParams.Windowed = bWindowed;
	presentParams.hDeviceWindow = m_hDisplayWindow;
	presentParams.BackBufferWidth = width;
	presentParams.BackBufferHeight = height;
	presentParams.SwapEffect = D3DSWAPEFFECT_COPY;
	presentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
	presentParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
	presentParams.BackBufferFormat = m_displayMode.Format;
	presentParams.BackBufferCount = 1;
	presentParams.EnableAutoDepthStencil = FALSE;

	memcpy(params, &presentParams, sizeof(D3DPRESENT_PARAMETERS));

	return S_OK;
}

HRESULT D3D9RenderImpl::SetVertexShader(LPCSTR pVertexShaderName, LPCSTR entryPoint, LPCSTR shaderModel, LPSTR* ppError)
{
	CComPtr<ID3DXBuffer> code;
	CComPtr<ID3DXBuffer> errMsg;

	HRESULT hr = D3DXCompileShaderFromFile(pVertexShaderName, NULL, NULL, entryPoint, shaderModel, 0, &code, &errMsg, &m_pVertexConstantTable);
	if (FAILED(hr)) {
		if (errMsg != NULL) {
			size_t len = errMsg->GetBufferSize() + 1;
			*ppError = new CHAR[len];
			memcpy(*ppError, errMsg->GetBufferPointer(), len);
		}
		return hr;
	}

	return m_pDevice->CreateVertexShader((DWORD*)code->GetBufferPointer(), &m_pVertexShader);
}

HRESULT D3D9RenderImpl::ApplyWorldViewProj(LPCSTR matrixName)
{
	D3DXMATRIX matOrtho;
	HR(m_pDevice->GetTransform(D3DTS_PROJECTION, &matOrtho));

	return m_pVertexConstantTable->SetMatrix(m_pDevice, matrixName, &matOrtho);
}

HRESULT D3D9RenderImpl::SetVertexShader(DWORD* buffer)
{
	HR(D3DXGetShaderConstantTable(buffer, &m_pVertexConstantTable));

	return m_pDevice->CreateVertexShader(buffer, &m_pVertexShader);
}

HRESULT D3D9RenderImpl::SetVertexShaderConstant(LPCSTR name, LPVOID value, UINT size)
{
	return m_pVertexConstantTable->SetValue(m_pDevice, name, value, size);
}

HRESULT D3D9RenderImpl::SetPixelShader(LPCSTR pPixelShaderName, LPCSTR entryPoint, LPCSTR shaderModel, LPSTR* ppError)
{
	CComPtr<ID3DXBuffer> code;
	CComPtr<ID3DXBuffer> errMsg;

	HRESULT hr = D3DXCompileShaderFromFile(pPixelShaderName, NULL, NULL, entryPoint, shaderModel, 0, &code, &errMsg, &m_pPixelConstantTable);
	if (FAILED(hr)) {
		if (errMsg != NULL) {
			size_t len = errMsg->GetBufferSize() + 1;
			*ppError = new CHAR[len];
			memcpy(*ppError, errMsg->GetBufferPointer(), len);
		}
		return hr;
	}

	return m_pDevice->CreatePixelShader((DWORD*)code->GetBufferPointer(), &m_pPixelShader);
}

HRESULT D3D9RenderImpl::SetPixelShader(DWORD* buffer)
{
	HR(D3DXGetShaderConstantTable(buffer, &m_pPixelConstantTable));
	return m_pDevice->CreatePixelShader(buffer, &m_pPixelShader);
}

HRESULT D3D9RenderImpl::SetPixelShaderIntConstant(LPCSTR name, int value)
{
	return m_pPixelConstantTable->SetInt(m_pDevice, name, value);
}

HRESULT D3D9RenderImpl::SetPixelShaderFloatConstant(LPCSTR name, float value)
{
	return m_pPixelConstantTable->SetFloat(m_pDevice, name, value);
}

HRESULT D3D9RenderImpl::SetPixelShaderBoolConstant(LPCSTR name, bool value)
{
	return m_pPixelConstantTable->SetBool(m_pDevice, name, value);
}

HRESULT D3D9RenderImpl::SetPixelShaderConstant(LPCSTR name, LPVOID value, UINT size)
{
	return m_pPixelConstantTable->SetValue(m_pDevice, name, value, size);
}

HRESULT D3D9RenderImpl::SetVertexShaderMatrix(D3DXMATRIX* matrix, LPCSTR name)
{
	return m_pVertexConstantTable->SetMatrix(m_pDevice, name, matrix);
}

HRESULT D3D9RenderImpl::SetPixelShaderMatrix(D3DXMATRIX* matrix, LPCSTR name)
{
	return m_pPixelConstantTable->SetMatrix(m_pDevice, name, matrix);
}

HRESULT D3D9RenderImpl::SetVertexShaderVector(D3DXVECTOR4* vector, LPCSTR name)
{
	return m_pVertexConstantTable->SetVector(m_pDevice, name, vector);
}

HRESULT D3D9RenderImpl::SetPixelShaderVector(LPCSTR name, D3DXVECTOR4* vector)
{
	return m_pPixelConstantTable->SetVector(m_pDevice, name, vector);
}

HRESULT D3D9RenderImpl::CopyFromRenderTarget(byte* dst, int dstPitch)
{
	CComPtr<IDirect3DSurface9> pTargetSurface;
	CComPtr<IDirect3DSurface9> pTempSurface;

	HR(m_pDevice->GetRenderTarget(0, &pTargetSurface));
	HR(m_pDevice->CreateOffscreenPlainSurface(m_videoWidth, m_videoHeight, m_format, D3DPOOL_SYSTEMMEM, &pTempSurface, NULL));
	HR(m_pDevice->GetRenderTargetData(pTargetSurface, pTempSurface));

	D3DLOCKED_RECT d3drect;
	HR(pTempSurface->LockRect(&d3drect, NULL, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY));
	BYTE* pict = (BYTE*)d3drect.pBits;

	for (int y = 0; y < m_videoHeight; y++) {
		memcpy(dst, pict, m_videoWidth * m_precision * 4);
		pict += d3drect.Pitch;
		dst += dstPitch;
	}

	return pTempSurface->UnlockRect();
}

HRESULT D3D9RenderImpl::ClearPixelShader()
{
	SafeRelease(m_pPixelConstantTable);
	SafeRelease(m_pPixelShader);

	return S_OK;
}

HRESULT D3D9RenderImpl::ClearVertexShader()
{
	SafeRelease(m_pVertexConstantTable);
	SafeRelease(m_pVertexShader);

	return S_OK;
}