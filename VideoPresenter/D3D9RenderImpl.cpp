// Code taken from Taygeta Video Presender available here under LGPL3 license.The code has been slightly edited for our needs.
// http://www.codeproject.com/Articles/207642/Video-Shadering-with-Direct-D

#include "D3D9RenderImpl.h"
#include "dxerr.h"


D3D9RenderImpl::D3D9RenderImpl()
    : m_pVertexShader(0), m_pPixelShader(0), m_pD3D9(0), m_format(D3DFMT_UNKNOWN), m_pVertexConstantTable(0), m_pPixelConstantTable(0), 
      m_fillMode(KeepAspectRatio)
{
    
}

HRESULT D3D9RenderImpl::Initialize(HWND hDisplayWindow)
{
    m_hDisplayWindow = hDisplayWindow;

    m_pD3D9 = Direct3DCreate9( D3D_SDK_VERSION ); 
    if(!m_pD3D9)
    {
        return E_FAIL;
    }

    HR(m_pD3D9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &m_displayMode));

    D3DCAPS9 deviceCaps;
    HR(m_pD3D9->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &deviceCaps));

    DWORD dwBehaviorFlags = D3DCREATE_MULTITHREADED;

    if( deviceCaps.VertexProcessingCaps != 0 )
        dwBehaviorFlags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
    else
        dwBehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    
    HR(GetPresentParams(&m_presentParams, TRUE));
	m_format = m_presentParams.BackBufferFormat;
    
    HR(m_pD3D9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hDisplayWindow, dwBehaviorFlags, &m_presentParams, &m_pDevice));
    
    return CreateResources();
}

D3D9RenderImpl::~D3D9RenderImpl(void)
{
    SafeRelease(m_pD3D9);
    SafeRelease(m_pDevice);
    SafeRelease(m_pOffsceenSurface);
    SafeRelease(m_pTextureSurface);
    SafeRelease(m_pTexture);
    SafeRelease(m_pVertexBuffer);
    SafeRelease(m_pVertexShader); 
    SafeRelease(m_pVertexConstantTable); 
    SafeRelease(m_pPixelConstantTable); 
    SafeRelease(m_pPixelShader);
}


HRESULT D3D9RenderImpl::CheckFormatConversion(D3DFORMAT format)
{
    HR(m_pD3D9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_displayMode.Format, 0, D3DRTYPE_SURFACE, format));
    
    return m_pD3D9->CheckDeviceFormatConversion(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, format, m_displayMode.Format);
}


HRESULT D3D9RenderImpl::CreateRenderTarget(int width, int height)
{
    // HR(m_pDevice->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, m_displayMode.Format, D3DPOOL_DEFAULT, &m_pTexture, NULL));
	HR(m_pDevice->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, m_format, D3DPOOL_DEFAULT, &m_pTexture, NULL));
    HR(m_pTexture->GetSurfaceLevel(0, &m_pTextureSurface));
    HR(m_pDevice->CreateVertexBuffer(sizeof(VERTEX) * 4, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &m_pVertexBuffer, NULL));
    
    VERTEX vertexArray[] =
    {
        { D3DXVECTOR3(0, 0, 0),          D3DCOLOR_ARGB(255, 255, 255, 255), D3DXVECTOR2(0, 0) },  // top left
        { D3DXVECTOR3(width, 0, 0),      D3DCOLOR_ARGB(255, 255, 255, 255), D3DXVECTOR2(1, 0) },  // top right
        { D3DXVECTOR3(width, height, 0), D3DCOLOR_ARGB(255, 255, 255, 255), D3DXVECTOR2(1, 1) },  // bottom right
        { D3DXVECTOR3(0, height, 0),     D3DCOLOR_ARGB(255, 255, 255, 255), D3DXVECTOR2(0, 1) },  // bottom left
    };

    VERTEX *vertices;
    HR(m_pVertexBuffer->Lock(0, 0, (void**)&vertices, D3DLOCK_DISCARD));

    memcpy(vertices, vertexArray, sizeof(vertexArray));

    HR(m_pVertexBuffer->Unlock());

	return m_pDevice->SetRenderTarget(0, m_pTextureSurface);
}

// Edit: Custom function similar to Display but that returns the result in a buffer.
HRESULT D3D9RenderImpl::ProcessFrame(const byte* src, int srcPitch, byte* dst, int dstPitch)
{
	HR(CheckDevice());
	HR(CopyToBuffer(src, srcPitch));
	HR(CreateScene());
	HR(Present());
	return CopyFromBuffer(dst, dstPitch);
}

HRESULT D3D9RenderImpl::Display(BYTE* pYplane, BYTE* pVplane, BYTE* pUplane)
{
    if(!pYplane)
    {
        return E_POINTER;
    }

    if(m_format == D3DFMT_NV12 && !pVplane)
    {
        return E_POINTER;
    }

    if(m_format == D3DFMT_YV12 && (!pVplane || !pUplane))
    {
        return E_POINTER;
    }

    HR(CheckDevice());
    HR(FillBuffer(pYplane, pVplane, pUplane));
    HR(CreateScene());
    return Present();
}

HRESULT D3D9RenderImpl::SetupMatrices(int width, int height)
{
    D3DXMATRIX matOrtho; 
    D3DXMATRIX matIdentity;

    D3DXMatrixOrthoOffCenterLH(&matOrtho, 0, width, height, 0, 0.0f, 1.0f);
    D3DXMatrixIdentity(&matIdentity);

    HR(m_pDevice->SetTransform(D3DTS_PROJECTION, &matOrtho));
    HR(m_pDevice->SetTransform(D3DTS_WORLD, &matIdentity));
    return m_pDevice->SetTransform(D3DTS_VIEW, &matIdentity);
}

HRESULT D3D9RenderImpl::CreateScene(void)
{
    HRESULT hr = m_pDevice->Clear(D3DADAPTER_DEFAULT, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

    HR(m_pDevice->BeginScene());

    hr = m_pDevice->SetFVF(D3DFVF_CUSTOMVERTEX);
    if(FAILED(hr))
    {
        m_pDevice->EndScene();
        return hr;
    }

    hr = m_pDevice->SetVertexShader(m_pVertexShader);
    if(FAILED(hr))
    {
        m_pDevice->EndScene();
        return hr;
    }
    
    hr = m_pDevice->SetPixelShader(m_pPixelShader);
    if(FAILED(hr))
    {
        m_pDevice->EndScene();
        return hr;
    }

    hr = m_pDevice->SetStreamSource(0, m_pVertexBuffer, 0, sizeof(VERTEX));
    if(FAILED(hr))
    {
        m_pDevice->EndScene();
        return hr;
    }

    hr = m_pDevice->SetTexture(0, m_pTexture);
    if(FAILED(hr))
    {
        m_pDevice->EndScene();
        return hr;
    }

    hr = m_pDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);
    if(FAILED(hr))
    {
        m_pDevice->EndScene();
        return hr;
    }

    m_overlays.Draw();

    return m_pDevice->EndScene();
}


HRESULT D3D9RenderImpl::CheckDevice(void)
{
    return m_pDevice->TestCooperativeLevel();
}

HRESULT D3D9RenderImpl::DiscardResources()
{
    SafeRelease(m_pOffsceenSurface);
    SafeRelease(m_pTextureSurface);
    SafeRelease(m_pTexture);
    SafeRelease(m_pVertexBuffer);
    SafeRelease(m_pVertexShader); 
    SafeRelease(m_pVertexConstantTable); 
    SafeRelease(m_pPixelConstantTable); 
    SafeRelease(m_pPixelShader);

    m_overlays.RemoveAll();

    return S_OK;
}

HRESULT D3D9RenderImpl::CreateResources()
{
    m_pDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE);
    m_pDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE);
    m_pDevice->SetRenderState( D3DRS_LIGHTING, FALSE);
    m_pDevice->SetRenderState( D3DRS_DITHERENABLE, TRUE);

    m_pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE);
    m_pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_SPECULAR);

    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

    HR(CreateRenderTarget(m_presentParams.BackBufferWidth, m_presentParams.BackBufferHeight));
    
    return SetupMatrices(m_presentParams.BackBufferWidth, m_presentParams.BackBufferHeight);
}

static inline void ApplyLetterBoxing(RECT& rendertTargetArea, FLOAT frameWidth, FLOAT frameHeight)
{
    const float aspectRatio = frameWidth / frameHeight;

    const float targetW = fabs((FLOAT)(rendertTargetArea.right - rendertTargetArea.left));
    const float targetH = fabs((FLOAT)(rendertTargetArea.bottom - rendertTargetArea.top));

    float tempH = targetW / aspectRatio;    
            
    if(tempH <= targetH)
    {               
        float deltaH = fabs(tempH - targetH) / 2;
        rendertTargetArea.top += deltaH;
        rendertTargetArea.bottom -= deltaH;
    }
    else
    {
        float tempW = targetH * aspectRatio;    
        float deltaW = fabs(tempW - targetW) / 2;

        rendertTargetArea.left += deltaW;
        rendertTargetArea.right -= deltaW;
    }
}

HRESULT D3D9RenderImpl::CopyToBuffer(const byte* src, int srcPitch) {
	D3DLOCKED_RECT d3drect;
	HR(m_pOffsceenSurface->LockRect(&d3drect, NULL, 0));
	BYTE* pict = (BYTE*)d3drect.pBits;

	for (int y = 0; y < m_videoHeight; y++) {
		memcpy(pict, src, m_videoWidth * m_precision);
		pict += d3drect.Pitch;
		src += srcPitch;
	}

	return  m_pOffsceenSurface->UnlockRect();
}

HRESULT D3D9RenderImpl::CopyFromBuffer(byte* dst, int dstPitch) {
	// Copy the texture from default pool to system memory to read it.
	IDirect3DTexture9* textureSysMem;
	IDirect3DSurface9* textureSurface;
	HR(m_pDevice->CreateTexture(m_videoWidth, m_videoHeight, 1, 0, m_format, D3DPOOL_SYSTEMMEM, &textureSysMem, NULL));
	HR(textureSysMem->GetSurfaceLevel(0, &textureSurface));
	m_pDevice->GetRenderTargetData(m_pTextureSurface, textureSurface);

	D3DLOCKED_RECT d3drect;
	HR(textureSurface->LockRect(&d3drect, NULL, 0));
	BYTE* pict = (BYTE*)d3drect.pBits;

	for (int y = 0; y < m_videoHeight; y++) {
		memcpy(dst, pict, m_videoWidth * m_precision);
		pict += d3drect.Pitch;
		dst += dstPitch;
	}

	return textureSurface->UnlockRect();
}


HRESULT D3D9RenderImpl::FillBuffer(const BYTE* pY, BYTE* pV, BYTE* pU)
{
    D3DLOCKED_RECT d3drect;
    HR(m_pOffsceenSurface->LockRect(&d3drect, NULL, 0));

    int newHeight  = m_videoHeight;
    int newWidth  = m_videoWidth;

    BYTE* pict = (BYTE*)d3drect.pBits;
    const BYTE* Y = pY;
    BYTE* V = pV;
    BYTE* U = pU;

    switch(m_format)
    {
        case D3DFMT_YV12:

            for (int y = 0 ; y < newHeight ; y++)
            {
                memcpy(pict, Y, newWidth);
                pict += d3drect.Pitch;
                Y += newWidth;
            }
            for (int y = 0 ; y < newHeight / 2 ; y++)
            {
                memcpy(pict, V, newWidth / 2);
                pict += d3drect.Pitch / 2;
                V += newWidth / 2;
            }
            for (int y = 0 ; y < newHeight / 2; y++)
            {
                memcpy(pict, U, newWidth / 2);
                pict += d3drect.Pitch / 2;
                U += newWidth / 2;
            }	
            break;

        case D3DFMT_NV12:
            
            for (int y = 0 ; y < newHeight ; y++)
            {
                memcpy(pict, Y, newWidth);
                pict += d3drect.Pitch;
                Y += newWidth;
            }
            for (int y = 0 ; y < newHeight / 2 ; y++)
            {
                memcpy(pict, V, newWidth);
                pict += d3drect.Pitch;
                V += newWidth;
            }
            break;

        case D3DFMT_YUY2:
        case D3DFMT_UYVY:
        case D3DFMT_R5G6B5:
        case D3DFMT_X1R5G5B5:
        case D3DFMT_A8R8G8B8:
        case D3DFMT_X8R8G8B8:
            memcpy(pict, Y, d3drect.Pitch * newHeight);
            break;
    }
     
    return  m_pOffsceenSurface->UnlockRect();
}


HRESULT D3D9RenderImpl::Present(void)
{
    RECT rect;
    ::GetClientRect(m_hDisplayWindow, &rect);

    if(m_fillMode == KeepAspectRatio)
    {	
        ApplyLetterBoxing(rect, (FLOAT)m_videoWidth, (FLOAT)m_videoHeight);
    }

    HR(m_pDevice->ColorFill(m_pTextureSurface, NULL, D3DCOLOR_ARGB(0xFF, 200, 200, 200)));
	HR(m_pDevice->StretchRect(m_pOffsceenSurface, NULL, m_pTextureSurface, &rect, D3DTEXF_LINEAR));

    return m_pDevice->Present(NULL, NULL, NULL, NULL);
}


HRESULT D3D9RenderImpl::CreateVideoSurface(int width, int height, int precision)
{
    m_videoWidth = width;
    m_videoHeight = height;
	m_precision = precision;
	// if (precision == 4)
	m_format = D3DFMT_A32B32G32R32F;

    HR(m_pDevice->CreateOffscreenPlainSurface(width, height, m_format , D3DPOOL_DEFAULT, &m_pOffsceenSurface, NULL));

    return m_pDevice->ColorFill(m_pOffsceenSurface, NULL, D3DCOLOR_ARGB(0xFF, 0, 0, 0));
}


HRESULT D3D9RenderImpl::GetPresentParams(D3DPRESENT_PARAMETERS* params, BOOL bWindowed)
{
    UINT height, width;

    if(bWindowed) // windowed mode
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

    D3DPRESENT_PARAMETERS presentParams = {0};
    presentParams.Flags                  = D3DPRESENTFLAG_VIDEO;
    presentParams.Windowed               = bWindowed;
    presentParams.hDeviceWindow          = m_hDisplayWindow;
    presentParams.BackBufferWidth        = width;
    presentParams.BackBufferHeight       = height;
    presentParams.SwapEffect             = D3DSWAPEFFECT_COPY;
    presentParams.MultiSampleType        = D3DMULTISAMPLE_NONE;
    presentParams.PresentationInterval   = D3DPRESENT_INTERVAL_DEFAULT;
    presentParams.BackBufferFormat       = m_displayMode.Format;
    presentParams.BackBufferCount        = 1;
    presentParams.EnableAutoDepthStencil = FALSE;

    memcpy(params, &presentParams, sizeof(D3DPRESENT_PARAMETERS));

    return S_OK;
}

HRESULT D3D9RenderImpl::SetVertexShader(LPCSTR pVertexShaderName, LPCSTR entryPoint, LPCSTR shaderModel, LPSTR* ppError)
{
    CComPtr<ID3DXBuffer> code;
    CComPtr<ID3DXBuffer> errMsg;

    HRESULT hr = D3DXCompileShaderFromFile(pVertexShaderName, NULL, NULL, entryPoint, shaderModel, 0, &code, &errMsg, &m_pVertexConstantTable);
    if (FAILED(hr))
    {	
        if(errMsg != NULL)
        {
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
    if (FAILED(hr))
    {	
        if(errMsg != NULL)
        {
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
    
HRESULT D3D9RenderImpl::SetPixelShaderVector(D3DXVECTOR4* vector, LPCSTR name)
{
    return m_pPixelConstantTable->SetVector(m_pDevice, name, vector);
}

HRESULT D3D9RenderImpl::DrawLine(SHORT key, POINT p1, POINT p2, FLOAT width, D3DCOLOR color, BYTE opacity)
{
    m_overlays.AddOverlay(new LineOverlay(m_pDevice, p1, p2, width, color, opacity), key);

    return S_OK;
}

HRESULT D3D9RenderImpl::DrawRectangle(SHORT key, RECT rectangle, FLOAT width, D3DCOLOR color, BYTE opacity)
{
    m_overlays.AddOverlay(new RectangleOverlay(m_pDevice, rectangle, width, color, opacity), key);

    return S_OK;
}

HRESULT D3D9RenderImpl::DrawPolygon(SHORT key, POINT* points, INT pointsLen, FLOAT width, D3DCOLOR color, BYTE opacity)
{
    m_overlays.AddOverlay(new PolygonOverlay(m_pDevice, points, pointsLen, width, color, opacity), key);

    return S_OK;
}

HRESULT D3D9RenderImpl::DrawText(SHORT key, LPCSTR text, RECT pos, INT size, D3DCOLOR color, LPCSTR font, BYTE opacity)
{
    m_overlays.AddOverlay(new TextOverlay(m_pDevice, text, pos, size, color, font, opacity), key);

    return S_OK;
}

HRESULT D3D9RenderImpl::DrawBitmap(SHORT key, POINT position, INT width, INT height, BYTE* pPixelData, BYTE opacity)
{
    m_overlays.AddOverlay(new BitmapOverlay(m_pDevice, position, width, height, pPixelData, opacity), key);

    return S_OK;
}

HRESULT D3D9RenderImpl::RemoveOverlay(SHORT key)
{
    m_overlays.RemoveOverlay(key);

    return S_OK;
}

HRESULT D3D9RenderImpl::RemoveAllOverlays()
{
    m_overlays.RemoveAll();

    return S_OK;
}

HRESULT D3D9RenderImpl::CaptureDisplayFrame(BYTE* pBuffer, INT* width, INT* height, INT* stride)
{
    CComPtr<IDirect3DSurface9> pTargetSurface;	
    CComPtr<IDirect3DSurface9> pTempSurface;

    HR(m_pDevice->GetRenderTarget(0, &pTargetSurface));	

    D3DSURFACE_DESC desc;		
    HR(pTargetSurface->GetDesc(&desc));	

    if(!pBuffer)
    {
        *width = desc.Width;
        *height = desc.Height;
        *stride = desc.Width * 4; // Always ARGB32
        return S_OK;
    }

    HR(m_pDevice->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &pTempSurface, NULL));				
    HR(m_pDevice->GetRenderTargetData(pTargetSurface, pTempSurface));					
    
    D3DLOCKED_RECT d3drect;
    HR(pTempSurface->LockRect(&d3drect, NULL, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY));		
    
    BYTE* pFrame = (BYTE*)d3drect.pBits;
    BYTE* pBuf = pBuffer;
    
    memcpy(pBuf, pFrame, desc.Height * d3drect.Pitch);

    return pTempSurface->UnlockRect();
}

HRESULT D3D9RenderImpl::CaptureVideoFrame(BYTE* pBuffer, INT* width, INT* height, INT* stride)
{
    if(!pBuffer)
    {
        *width = m_videoWidth;
        *height = m_videoHeight;
        *stride = m_videoWidth * 4; // Always ARGB32
        return S_OK;
    }

    CComPtr<IDirect3DSurface9> pTempSurface;			
    HR(m_pDevice->CreateOffscreenPlainSurface(m_videoWidth, m_videoHeight, m_displayMode.Format, D3DPOOL_DEFAULT, &pTempSurface, NULL));
    HR(m_pDevice->StretchRect(m_pOffsceenSurface, NULL, pTempSurface, NULL, D3DTEXF_LINEAR))	;
    
    D3DLOCKED_RECT d3drect;
    HR(pTempSurface->LockRect(&d3drect, NULL, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY));		
    
    BYTE* pFrame = (BYTE*)d3drect.pBits;
    BYTE* pBuf = pBuffer;

    memcpy(pBuf, pFrame, m_videoHeight * d3drect.Pitch);
    
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

void D3D9RenderImpl::SetDisplayMode(FillMode mode)
{
    m_fillMode = mode;
}

FillMode D3D9RenderImpl::GetDisplayMode()
{
    return m_fillMode;
}