// Code originally taken from Taygeta Video Presender available here.
// The code has been mostly rewritten for our needs.
// http://www.codeproject.com/Articles/207642/Video-Shadering-with-Direct-D

#include "D3D9RenderImpl.h"


D3D9RenderImpl::D3D9RenderImpl() {
}

D3D9RenderImpl::~D3D9RenderImpl(void) {
	for (int i = 0; i < maxTextures; i++) {
		SafeRelease(m_InputTextures[0].Surface);
		SafeRelease(m_InputTextures[0].Texture);
		SafeRelease(m_InputTextures[0].Memory);
	}
	SafeRelease(m_pRenderTargetSurface);
	SafeRelease(m_pRenderTarget);
	SafeRelease(m_pVertexBuffer);
	SafeRelease(m_pPixelConstantTable);
	SafeRelease(m_pPixelShader);
	SafeRelease(m_pReadSurfaceGpu);
	SafeRelease(m_pReadSurfaceCpu);
}

HRESULT D3D9RenderImpl::Initialize(HWND hDisplayWindow, int width, int height, int precision) {
	m_precision = precision;
	if (precision == 1)
		m_format = D3DFMT_X8R8G8B8;
	else if (precision == 2)
		m_format = D3DFMT_A16B16G16R16F;
	else
		return E_FAIL;

	m_hDisplayWindow = hDisplayWindow;

	HR(Direct3DCreate9Ex(D3D_SDK_VERSION, &m_pD3D9));
	if (!m_pD3D9) {
		return E_FAIL;
	}

	HR(m_pD3D9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &m_displayMode));

	D3DCAPS9 deviceCaps;
	HR(m_pD3D9->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &deviceCaps));

	DWORD dwBehaviorFlags = D3DCREATE_DISABLE_PSGP_THREADING;

	if (deviceCaps.VertexProcessingCaps != 0)
		dwBehaviorFlags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
	else
		dwBehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;

	HR(GetPresentParams(&m_presentParams));

	HR(m_pD3D9->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hDisplayWindow, dwBehaviorFlags, &m_presentParams, NULL, &m_pDevice));

	HR(CheckFormatConversion(m_format));
	HR(CreateRenderTarget(width, height));
	return(SetupMatrices(width, height));
}

HRESULT D3D9RenderImpl::GetPresentParams(D3DPRESENT_PARAMETERS* params)
{
	// windowed mode
	RECT rect;
	::GetClientRect(m_hDisplayWindow, &rect);
	UINT height = rect.bottom - rect.top;
	UINT width = rect.right - rect.left;

	D3DPRESENT_PARAMETERS presentParams = { 0 };
	presentParams.Flags = D3DPRESENTFLAG_VIDEO;
	presentParams.Windowed = true;
	presentParams.hDeviceWindow = m_hDisplayWindow;
	presentParams.BackBufferWidth = width;
	presentParams.BackBufferHeight = height;
	presentParams.SwapEffect = D3DSWAPEFFECT_COPY;
	presentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
	presentParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
	presentParams.BackBufferFormat = D3DFMT_UNKNOWN; // m_displayMode.Format;
	presentParams.BackBufferCount = 0;
	presentParams.EnableAutoDepthStencil = FALSE;

	memcpy(params, &presentParams, sizeof(D3DPRESENT_PARAMETERS));

	return S_OK;
}

HRESULT D3D9RenderImpl::CheckFormatConversion(D3DFORMAT format)
{
	HR(m_pD3D9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_displayMode.Format, 0, D3DRTYPE_SURFACE, format));
	return m_pD3D9->CheckDeviceFormatConversion(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, format, m_displayMode.Format);
}

HRESULT D3D9RenderImpl::SetupMatrices(int width, int height)
{
	D3DXMATRIX matOrtho;
	D3DXMatrixOrthoOffCenterLH(&matOrtho, 0, (float)width, (float)height, 0, 0.0f, 1.0f);
	return(m_pDevice->SetTransform(D3DTS_PROJECTION, &matOrtho));
}

HRESULT D3D9RenderImpl::CreateRenderTarget(int width, int height)
{
	HR(m_pDevice->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, m_format, D3DPOOL_DEFAULT, &m_pRenderTarget, NULL));
	HR(m_pRenderTarget->GetSurfaceLevel(0, &m_pRenderTargetSurface));
	HR(m_pDevice->CreateVertexBuffer(sizeof(VERTEX) * 4, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_XYZRHW | D3DFVF_TEX1, D3DPOOL_DEFAULT, &m_pVertexBuffer, NULL));

	// Create vertexes for FVF (Fixed Vector Function) set to pre-processed vertex coordinates.
	VERTEX vertexArray[] =
	{
		{ 0, 0, 1, 1, 0, 0 }, // top left
		{ (float)width, 0, 1, 1, 1, 0 }, // top right
		{ (float)width, (float)height, 1, 1, 1, 1 }, // bottom right
		{ 0, (float)height, 1, 1, 0, 1 } // bottom left
	};
	// Prevent texture distortion during rasterization. https://msdn.microsoft.com/en-us/library/windows/desktop/bb219690%28v=vs.85%29.aspx?f=255&MSPPError=-2147217396
	for (int i = 0; i < 4; i++) {
		vertexArray[i].x -= .5f;
		vertexArray[i].y -= .5f;
	}

	VERTEX *vertices;
	HR(m_pVertexBuffer->Lock(0, 0, (void**)&vertices, D3DLOCK_DISCARD));

	memcpy(vertices, vertexArray, sizeof(vertexArray));

	HR(m_pVertexBuffer->Unlock());

	HR(m_pDevice->SetRenderTarget(0, m_pRenderTargetSurface));

	HR(m_pDevice->GetRenderTarget(0, &m_pReadSurfaceGpu));
	HR(m_pDevice->CreateOffscreenPlainSurface(width, height, m_format, D3DPOOL_SYSTEMMEM, &m_pReadSurfaceCpu, NULL));

	return S_OK;
}

HRESULT D3D9RenderImpl::CreateInputTexture(int index, int width, int height) {
	if (index < 0 || index >= maxTextures)
		return E_FAIL;

	HR(m_pDevice->CreateOffscreenPlainSurface(width, height, m_format, D3DPOOL_DEFAULT, &m_InputTextures[index].Memory, NULL));
	HR(m_pDevice->ColorFill(m_InputTextures[index].Memory, NULL, D3DCOLOR_ARGB(0xFF, 0, 0, 0)));

	HR(m_pDevice->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, m_format, D3DPOOL_DEFAULT, &m_InputTextures[index].Texture, NULL));
	HR(m_InputTextures[index].Texture->GetSurfaceLevel(0, &m_InputTextures[index].Surface));

	return S_OK;
}

HRESULT D3D9RenderImpl::ProcessFrame(byte* dst, int dstPitch, int width, int height, IScriptEnvironment* env)
{
	if (m_pDevice->TestCooperativeLevel() != S_OK)
		return D3DERR_DEVICENOTRESET;
	HR(CreateScene());
	HR(Present());
	return CopyFromRenderTarget(dst, dstPitch, width, height, env);
}

HRESULT D3D9RenderImpl::ResetDevice() {
	for (int i = 0; i < maxTextures; i++) {
		SafeRelease(m_InputTextures[0].Surface);
		SafeRelease(m_InputTextures[0].Texture);
		SafeRelease(m_InputTextures[0].Memory);
	}
	SafeRelease(m_pRenderTargetSurface);
	SafeRelease(m_pRenderTarget);
	SafeRelease(m_pVertexBuffer);
	SafeRelease(m_pPixelConstantTable);
	SafeRelease(m_pPixelShader);
	SafeRelease(m_pReadSurfaceGpu);
	SafeRelease(m_pReadSurfaceCpu);

	HR(m_pDevice->Reset(&m_presentParams));

	return S_OK;
}

HRESULT D3D9RenderImpl::CreateScene(void)
{
	HR(m_pDevice->Clear(D3DADAPTER_DEFAULT, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0));
	HR(m_pDevice->BeginScene());
	SCENE_HR(m_pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1), m_pDevice);
	SCENE_HR(m_pDevice->SetPixelShader(m_pPixelShader), m_pDevice);
	SCENE_HR(m_pDevice->SetStreamSource(0, m_pVertexBuffer, 0, sizeof(VERTEX)), m_pDevice);

	for (int i = 0; i < maxTextures; i++) {
		if (m_InputTextures[i].Texture != NULL)
			SCENE_HR(m_pDevice->SetTexture(i, m_InputTextures[i].Texture), m_pDevice);
	}

	SCENE_HR(m_pDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2), m_pDevice);
	return m_pDevice->EndScene();
}

HRESULT D3D9RenderImpl::Present(void)
{
	for (int i = 0; i < maxTextures; i++) {
		if (m_InputTextures[i].Texture != NULL) {
			HR(m_pDevice->ColorFill(m_InputTextures[i].Surface, NULL, D3DCOLOR_ARGB(0xFF, 0, 0, 0)));
			HR(m_pDevice->StretchRect(m_InputTextures[i].Memory, NULL, m_InputTextures[i].Surface, NULL, D3DTEXF_POINT));
		}
	}
	HR(m_pDevice->Present(NULL, NULL, NULL, NULL));
	Sleep(1);
	// The RenderTarget returns the previously generated scene for an unknown reason.
	// As a fix, we render another scene so that the previous scene becomes the one returned.
	HR(m_pDevice->BeginScene());
	HR(m_pDevice->EndScene());
	SCENE_HR(m_pDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2), m_pDevice);
	return m_pDevice->Present(NULL, NULL, NULL, NULL);
	//Sleep(1);
}

HRESULT D3D9RenderImpl::CopyToBuffer(const byte* src, int srcPitch, int index, int width, int height, IScriptEnvironment* env) {
	// Copies source frame into main surface buffer, or into additional input textures
	CComPtr<IDirect3DSurface9> destSurface = m_InputTextures[index].Memory;
	if (index < 0 || index >= maxTextures)
		return E_FAIL;

	D3DLOCKED_RECT d3drect;
	HR(destSurface->LockRect(&d3drect, NULL, 0));
	BYTE* pict = (BYTE*)d3drect.pBits;

	env->BitBlt(pict, d3drect.Pitch, src, srcPitch, width * m_precision * 4, height);

	//for (int y = 0; y < height; y++) {
	//	memcpy(pict, src, width * m_precision * 4);
	//	pict += d3drect.Pitch;
	//	src += srcPitch;
	//}

	return destSurface->UnlockRect();
}

HRESULT D3D9RenderImpl::CopyFromRenderTarget(byte* dst, int dstPitch, int width, int height, IScriptEnvironment* env)
{
	HR(m_pDevice->GetRenderTargetData(m_pReadSurfaceGpu, m_pReadSurfaceCpu));

	D3DLOCKED_RECT d3drect;
	HR(m_pReadSurfaceCpu->LockRect(&d3drect, NULL, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY));
	BYTE* pict = (BYTE*)d3drect.pBits;

	env->BitBlt(dst, dstPitch, pict, d3drect.Pitch, width * m_precision * 4, height);

	//for (int y = 0; y < height; y++) {
	//	memcpy(dst, pict, width * m_precision * 4);
	//	pict += d3drect.Pitch;
	//	dst += dstPitch;
	//}

	return m_pReadSurfaceCpu->UnlockRect();
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

HRESULT D3D9RenderImpl::SetPixelShaderMatrix(D3DXMATRIX* matrix, LPCSTR name)
{
	return m_pPixelConstantTable->SetMatrix(m_pDevice, name, matrix);
}

HRESULT D3D9RenderImpl::SetPixelShaderVector(LPCSTR name, D3DXVECTOR4* vector)
{
	return m_pPixelConstantTable->SetVector(m_pDevice, name, vector);
}