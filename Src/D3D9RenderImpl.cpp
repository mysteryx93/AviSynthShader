// Code originally taken from Taygeta Video Presender available here.
// The code has been mostly rewritten for our needs.
// http://www.codeproject.com/Articles/207642/Video-Shadering-with-Direct-D

// Memory transfers explained
// We can only read/write to a texture that is on the CPU side, and DirectX9 can only 
// do its work with textures on the GPU side. We must transfer data around.
//
// Planar allows transfering the data in and out as 3 planes which reduces transfers by 25% (no Alpha plane)
// PlanarIn: First clip has 3x L8 textures that will be passed to the first shader as s0, s1 and s2.
// PlanarOut: Output as YUV to GPU memory, then for each of Y, U and V, run shader to fill L8 plane and transfer L8 plane 
//            to CPU before returning all 3 planes to AviSynth. All 3 planes will be transfered via the same surface.

#include "D3D9RenderImpl.h"

D3D9RenderImpl::D3D9RenderImpl() {
}

D3D9RenderImpl::~D3D9RenderImpl(void) {
    ClearRenderTarget();
    if (m_DitherMatrix)
        ReleaseTexture(m_DitherMatrix);
    if (m_Pool)
        delete m_Pool;
    for (auto const item : m_RenderTargetMatrixCache) {
        delete item;
    }
}

HRESULT D3D9RenderImpl::Initialize(HWND hDisplayWindow, int clipPrecision[9], int precision, int outputPrecision, bool planarOut, bool isMT, IScriptEnvironment* env) {
    m_PlanarOut = planarOut;
    m_Precision = precision;
    for (int i = 0; i < 9; i++) {
        m_ClipPrecision[i] = clipPrecision[i];
    }
    m_OutputPrecision = outputPrecision;

    HR(CreateDevice(&m_pDevice, hDisplayWindow, isMT));
    ResetSamplerState();

    m_Pool = new MemoryPool();

    m_DitherMatrix = new InputTexture();
    CreateTexture(-1, DITHER_MATRIX_SIZE, DITHER_MATRIX_SIZE, true, false, false, 1, m_DitherMatrix);
    CopyDitherMatrixToSurface(m_DitherMatrix, env);

    return S_OK;
}

HRESULT D3D9RenderImpl::CreateDevice(IDirect3DDevice9Ex** device, HWND hDisplayWindow, bool isMT) {
    HR(Direct3DCreate9Ex(D3D_SDK_VERSION, &m_pD3D9));
    if (!m_pD3D9) {
        return E_FAIL;
    }

    D3DCAPS9 deviceCaps;
    HR(m_pD3D9->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &deviceCaps));

    DWORD dwBehaviorFlags = isMT ? D3DCREATE_MULTITHREADED : 0;

    if (deviceCaps.VertexProcessingCaps != 0)
        dwBehaviorFlags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
    else
        dwBehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;

    D3DPRESENT_PARAMETERS m_presentParams;
    HR(GetPresentParams(&m_presentParams, hDisplayWindow));

    HR(m_pD3D9->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hDisplayWindow, dwBehaviorFlags, &m_presentParams, nullptr, device));
    return S_OK;
}

HRESULT D3D9RenderImpl::GetPresentParams(D3DPRESENT_PARAMETERS* params, HWND hDisplayWindow)
{
    // windowed mode
    RECT rect;
    ::GetClientRect(hDisplayWindow, &rect);
    UINT height = rect.bottom - rect.top;
    UINT width = rect.right - rect.left;

    D3DPRESENT_PARAMETERS presentParams = { 0 };
    presentParams.Flags = D3DPRESENTFLAG_VIDEO;
    presentParams.Windowed = true;
    presentParams.hDeviceWindow = hDisplayWindow;
    presentParams.BackBufferWidth = 10;
    presentParams.BackBufferHeight = 10;
    presentParams.SwapEffect = D3DSWAPEFFECT_COPY;
    presentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
    presentParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    presentParams.BackBufferFormat = D3DFMT_UNKNOWN;
    presentParams.BackBufferCount = 0;
    presentParams.EnableAutoDepthStencil = FALSE;

    memcpy(params, &presentParams, sizeof(D3DPRESENT_PARAMETERS));

    return S_OK;
}

HRESULT D3D9RenderImpl::ResetSamplerState() {
    for (int i = 0; i < 9; i++) {
        HR(m_pDevice->SetSamplerState(i, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP));
        HR(m_pDevice->SetSamplerState(i, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP));
        //HR(m_pDevice->SetSamplerState(i, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP));
    }
    return S_OK;
}

HRESULT D3D9RenderImpl::SetRenderTarget(int width, int height, D3DFORMAT format, IScriptEnvironment* env)
{
    // Skip if current render target has right dimensions.
    if (m_pCurrentRenderTarget && m_pCurrentRenderTarget->Width == width && m_pCurrentRenderTarget->Height == height && m_pCurrentRenderTarget->Format == format)
        return S_OK;

    ClearRenderTarget();
    m_pCurrentRenderTarget = new PooledTexture{ 0 };

    m_pCurrentRenderTarget->Width = width;
    m_pCurrentRenderTarget->Height = height;
    m_pCurrentRenderTarget->Format = format;
    HR(m_Pool->AllocateTexture(m_pDevice, width, height, true, format, m_pCurrentRenderTarget->Texture, m_pCurrentRenderTarget->Surface));

    HR(m_pDevice->SetRenderTarget(0, m_pCurrentRenderTarget->Surface));
    return S_OK;
}

HRESULT D3D9RenderImpl::ClearRenderTarget() {
    if (m_pCurrentRenderTarget) {
        HR(m_Pool->Release(m_pCurrentRenderTarget->Surface));
        delete m_pCurrentRenderTarget;
        m_pCurrentRenderTarget = nullptr;
    }
    return S_OK;
}

HRESULT D3D9RenderImpl::GetRenderTargetMatrix(int width, int height, RenderTargetMatrix** outMatrix)
{
    // First look whether a matrix was already created for those dimensions.
    for (auto const item : m_RenderTargetMatrixCache) {
        if (item->Width == width && item->Height == height) {
            *outMatrix = item;
            return S_OK;
        }
    }

    // If not, create a new one and store in cache.
    RenderTargetMatrix* Obj = new RenderTargetMatrix();
    Obj->Width = width;
    Obj->Height = height;
    D3DXMatrixOrthoOffCenterLH(&Obj->MatrixOrtho, 0, (float)width, (float)height, 0, 0.0f, 1.0f);

    HR(m_pDevice->CreateVertexBuffer(sizeof(VERTEX) * 4, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_XYZRHW | D3DFVF_TEX1, D3DPOOL_DEFAULT, &Obj->VertexBuffer, nullptr));

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
    HR(Obj->VertexBuffer->Lock(0, 0, (void**)&vertices, D3DLOCK_DISCARD));
    memcpy(vertices, vertexArray, sizeof(vertexArray));
    HR(Obj->VertexBuffer->Unlock());

    m_RenderTargetMatrixCache.push_back(Obj);
    *outMatrix = Obj;
    return S_OK;
}

HRESULT D3D9RenderImpl::CreateTexture(int clipIndex, int width, int height, bool isInput, bool isPlanar, bool isLast, int shaderPrecision, InputTexture* outTexture) {
    outTexture->Pool = m_Pool;
    outTexture->ClipIndex = clipIndex;
    outTexture->Width = width;
    outTexture->Height = height;

    D3DFORMAT Format;
    if (isPlanar) {
        Format = GetD3DFormat(m_ClipPrecision[clipIndex], true);
        HR(m_Pool->AllocateTexture(m_pDevice, width, height, false, Format, outTexture->TextureY, outTexture->SurfaceY));
        HR(m_Pool->AllocateTexture(m_pDevice, width, height, false, Format, outTexture->TextureU, outTexture->SurfaceU));
        HR(m_Pool->AllocateTexture(m_pDevice, width, height, false, Format, outTexture->TextureV, outTexture->SurfaceV));
    }

    if (isLast) {
        Format = GetD3DFormat(m_OutputPrecision, m_PlanarOut);
        if (m_PlanarOut) {
            HR(m_Pool->AllocateTexture(m_pDevice, width, height, true, GetD3DFormat(m_OutputPrecision, false), outTexture->Texture, outTexture->Surface));
            HR(m_Pool->AllocatePlainSurface(m_pDevice, width, height, Format, outTexture->SurfaceY));
            HR(m_Pool->AllocatePlainSurface(m_pDevice, width, height, Format, outTexture->SurfaceU));
            HR(m_Pool->AllocatePlainSurface(m_pDevice, width, height, Format, outTexture->SurfaceV));
        }
        else {
            HR(m_Pool->AllocatePlainSurface(m_pDevice, width, height, Format, outTexture->Memory));
        }
    }
    else {
        Format = GetD3DFormat(isInput ? m_ClipPrecision[clipIndex] : shaderPrecision > -1 ? shaderPrecision : m_Precision, false);
        HR(m_Pool->AllocateTexture(m_pDevice, width, height, true, Format, outTexture->Texture, outTexture->Surface));
    }

    return S_OK;
}


HRESULT D3D9RenderImpl::ProcessFrame(std::vector<InputTexture*>* textureList, CommandStruct* cmd, int width, int height, bool isLast, int planeOut, IScriptEnvironment* env)
{
    HR(m_pDevice->TestCooperativeLevel());
    D3DFORMAT fmt = planeOut > 0 ? GetD3DFormat(m_OutputPrecision, true) : GetD3DFormat(isLast ? m_OutputPrecision : cmd->Precision > -1 ? cmd->Precision : m_Precision, false);
    HR(SetRenderTarget(width, height, fmt, env));
    HR(CreateScene(textureList, cmd, isLast, planeOut, env));
    HR(CopyFromRenderTarget(textureList, cmd, width, height, isLast, planeOut, env));
    return S_OK;
}

HRESULT D3D9RenderImpl::CreateScene(std::vector<InputTexture*>* textureList, CommandStruct* cmd, bool isLast, int planeOut, IScriptEnvironment* env)
{
    RenderTargetMatrix* Matrix;
    SCENE_HR(GetRenderTargetMatrix(m_pCurrentRenderTarget->Width, m_pCurrentRenderTarget->Height, &Matrix), m_pDevice);
    HR(m_pDevice->SetTransform(D3DTS_PROJECTION, &Matrix->MatrixOrtho));

    HR(m_pDevice->Clear(D3DADAPTER_DEFAULT, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0));
    HR(m_pDevice->BeginScene());
    SCENE_HR(m_pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1), m_pDevice);
    SCENE_HR(m_pDevice->SetPixelShader(m_Shaders[cmd->CommandIndex + planeOut].Shader), m_pDevice);
    SCENE_HR(m_pDevice->SetStreamSource(0, Matrix->VertexBuffer, 0, sizeof(VERTEX)), m_pDevice);

    // Clear samplers
    for (int i = 0; i < 9; i++) {
        SCENE_HR(m_pDevice->SetTexture(i, nullptr), m_pDevice);
    }

    // Set input clips.
    InputTexture* Input;
    for (int i = 0; i < 9; i++) {
        if (cmd->ClipIndex[i] > 0) {
            Input = FindTexture(textureList, cmd->ClipIndex[i]);
            if (Input && (Input->Texture || Input->TextureY)) {
                if (!Input->TextureY) {
                    SCENE_HR(m_pDevice->SetTexture(i, Input->Texture), m_pDevice);
                }
                else {
                    // Copy planar data to the 3 sampler spots starting at specified clip index.
                    SCENE_HR(m_pDevice->SetTexture(i + 0, Input->TextureY), m_pDevice);
                    SCENE_HR(m_pDevice->SetTexture(i + 1, Input->TextureU), m_pDevice);
                    SCENE_HR(m_pDevice->SetTexture(i + 2, Input->TextureV), m_pDevice);
                }
            }
            else
                env->ThrowError("Shader: Invalid clip index.");
        }
    }

    SCENE_HR(m_pDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2), m_pDevice);
    return m_pDevice->EndScene();
}

HRESULT D3D9RenderImpl::CopyFromRenderTarget(std::vector<InputTexture*>* textureList, CommandStruct* cmd, int width, int height, bool isLast, int planeOut, IScriptEnvironment* env)
{
    InputTexture* dst;
    HR(PrepareReadTarget(textureList, cmd, width, height, planeOut, isLast, false, &dst));

    CComPtr<IDirect3DSurface9> pReadSurfaceGpu;
    HR(m_pDevice->GetRenderTarget(0, &pReadSurfaceGpu));

	dst->ClipIndex = cmd->OutputIndex;
    if (!isLast) {
        HR(D3DXLoadSurfaceFromSurface(dst->Surface, nullptr, nullptr, pReadSurfaceGpu, nullptr, nullptr, D3DX_FILTER_NONE, 0));
    }
    else { // IsLast
        if (!m_PlanarOut && planeOut == 0) {
            // If reading last command, copy it back to CPU directly
            HR(m_pDevice->GetRenderTargetData(pReadSurfaceGpu, dst->Memory));
        }
        else if (m_PlanarOut && planeOut == 0) {
            HR(D3DXLoadSurfaceFromSurface(dst->Surface, nullptr, nullptr, pReadSurfaceGpu, nullptr, nullptr, D3DX_FILTER_NONE, 0));

            CommandStruct PlanarCmd = { 0 };
            PlanarCmd.Path = "OutputY.cso";
            PlanarCmd.CommandIndex = cmd->CommandIndex;
            PlanarCmd.ClipIndex[0] = 1;
            PlanarCmd.OutputIndex = 1;
            PlanarCmd.Precision = m_OutputPrecision;
            if FAILED(InitPixelShader(&PlanarCmd, 1, env))
                env->ThrowError("ExecuteShader: OutputY.cso not found");
            HR(ProcessFrame(textureList, &PlanarCmd, width, height, true, 1, env));
            PlanarCmd.Path = "OutputU.cso";
            if FAILED(InitPixelShader(&PlanarCmd, 2, env))
                env->ThrowError("ExecuteShader: OutputU.cso not found");
            HR(ProcessFrame(textureList, &PlanarCmd, width, height, true, 2, env));
            PlanarCmd.Path = "OutputV.cso";
            if FAILED(InitPixelShader(&PlanarCmd, 3, env))
                env->ThrowError("ExecuteShader: OutputV.cso not found");
            HR(ProcessFrame(textureList, &PlanarCmd, width, height, true, 3, env));
        }
        else if (planeOut > 0) {
            // This gets called recursively from the code below for each plane.
            IDirect3DSurface9* SurfaceOut = planeOut == 1 ? dst->SurfaceY : planeOut == 2 ? dst->SurfaceU : planeOut == 3 ? dst->SurfaceV : nullptr;
            HR(m_pDevice->GetRenderTargetData(pReadSurfaceGpu, SurfaceOut));
        }
    }
    return S_OK;
}

HRESULT D3D9RenderImpl::CopyBuffer(std::vector<InputTexture*>* textureList, InputTexture* src, CommandStruct* cmd) {
    bool IsPlanar = src->TextureY != nullptr;
    InputTexture* dst;
    HR(PrepareReadTarget(textureList, cmd, src->Width, src->Height, 0, false, IsPlanar, &dst));
    dst->ClipIndex = cmd->OutputIndex;

    if (!IsPlanar) {
        HR(D3DXLoadSurfaceFromSurface(dst->Surface, nullptr, nullptr, src->Surface, nullptr, nullptr, D3DX_FILTER_NONE, 0));
    }
    else {
        HR(D3DXLoadSurfaceFromSurface(dst->SurfaceY, nullptr, nullptr, src->SurfaceY, nullptr, nullptr, D3DX_FILTER_NONE, 0));
        HR(D3DXLoadSurfaceFromSurface(dst->SurfaceU, nullptr, nullptr, src->SurfaceU, nullptr, nullptr, D3DX_FILTER_NONE, 0));
        HR(D3DXLoadSurfaceFromSurface(dst->SurfaceV, nullptr, nullptr, src->SurfaceV, nullptr, nullptr, D3DX_FILTER_NONE, 0));
    }
    return S_OK;
}

HRESULT D3D9RenderImpl::CopyDitherMatrix(std::vector<InputTexture*>* textureList, int outputIndex) {
    CommandStruct cmd{};
    cmd.OutputIndex = 2;
    cmd.Precision = -1;
    CopyBuffer(textureList, m_DitherMatrix, &cmd);
    HR(m_pDevice->SetSamplerState(outputIndex - 1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP));
    HR(m_pDevice->SetSamplerState(outputIndex - 1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP));
    //HR(m_pDevice->SetSamplerState(outputIndex - 1, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP));
    return S_OK;
}

HRESULT D3D9RenderImpl::PrepareReadTarget(std::vector<InputTexture*>* textureList, CommandStruct* cmd, int width, int height, int planeOut, bool isLast, bool isPlanar, InputTexture** outDst) {
    InputTexture* dst = FindTexture(textureList, cmd->OutputIndex);
    if (planeOut == 0) {
        // Remove previous item with OutputIndex and replace it with new texture
        if (dst)
            HR(RemoveTexture(textureList, dst));
        dst = new InputTexture();
        HR(CreateTexture(cmd->OutputIndex, width, height, false, isPlanar, isLast, cmd->Precision, dst));
        textureList->push_back(dst);
    }
    *outDst = dst;
    return S_OK;
}

HRESULT D3D9RenderImpl::InitPixelShader(CommandStruct* cmd, int planeOut, IScriptEnvironment* env) {
    // PlaneOut will use the next 3 shader positions
    ShaderItem* Shader = &m_Shaders[cmd->CommandIndex + planeOut];
    if (Shader->Shader)
        return S_OK;

    std::unique_lock<std::mutex> my_lock(mutex_InitPixelShader);

    CComPtr<ID3DXBuffer> code;
    LPCVOID ShaderBuf = nullptr;
    UINT ShaderBufLength = 0;
    DWORD* CodeBuffer = nullptr;
    bool FromResource = false;

    D3D9Include Include;
    ShaderBuf = Include.GetResource(cmd->Path, &ShaderBufLength);
    if (ShaderBuf == nullptr)
        return E_FAIL;

    if (!StringEndsWith(cmd->Path, ".hlsl")) {
        // Precompiled shader
        HR(D3DXGetShaderConstantTable((DWORD*)ShaderBuf, &Shader->ConstantTable));
        CodeBuffer = (DWORD*)ShaderBuf;
    }
    else {
        // Compile HLSL shader code
        HR(D3DXCompileShader((LPCSTR)ShaderBuf, (UINT)ShaderBufLength, cmd->Defines, &Include, cmd->EntryPoint, cmd->ShaderModel, 0, &code, nullptr, &Shader->ConstantTable));
        CodeBuffer = (DWORD*)code->GetBufferPointer();
    }

    HR(m_pDevice->CreatePixelShader(CodeBuffer, &Shader->Shader));

    Include.Close(ShaderBuf);      
    return S_OK;
}


// returns 1 if str ends with suffix
bool D3D9RenderImpl::StringEndsWith(const char * str, const char * suffix) {
    if (str == nullptr || suffix == nullptr)
        return false;

    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > str_len)
        return false;

    return 0 == strncmp(str + str_len - suffix_len, suffix, suffix_len);
}

HRESULT D3D9RenderImpl::SetDefaults(LPD3DXCONSTANTTABLE table) {
    return table->SetDefaults(m_pDevice);
}

HRESULT D3D9RenderImpl::SetPixelShaderConstant(int index, const ParamStruct* param) {
    if (param->Type == ParamType::Float) {
        HR(m_pDevice->SetPixelShaderConstantF(index, (float*)param->Values, param->Count));
    }
    else if (param->Type == ParamType::Int) {
        HR(m_pDevice->SetPixelShaderConstantI(index, (int*)param->Values, param->Count));
    }
    else if (param->Type == ParamType::Bool) {
        HR(m_pDevice->SetPixelShaderConstantB(index, (const BOOL*)param->Values, param->Count));
    }
    return S_OK;
}
