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

#pragma warning(disable: 4996)


D3D9RenderImpl::D3D9RenderImpl() {
}

D3D9RenderImpl::~D3D9RenderImpl(void) {
	if (m_Pool != NULL)
		delete m_Pool;
	ClearRenderTarget();
}

HRESULT D3D9RenderImpl::Initialize(HWND hDisplayWindow, int clipPrecision[9], int precision, int outputPrecision, bool planarOut) {
	m_PlanarOut = planarOut;
	m_Precision = precision;
	HR(ApplyPrecision(precision, m_PrecisionSize));
	for (int i = 0; i < 9; i++) {
		m_ClipPrecision[i] = clipPrecision[i];
		HR(ApplyPrecision(clipPrecision[i], m_ClipPrecisionSize[i]));
	}
	m_OutputPrecision = outputPrecision;
	HR(ApplyPrecision(outputPrecision, m_OutputPrecisionSize));

	HR(CreateDevice(&m_pDevice, hDisplayWindow));
	m_Pool = new MemoryPool(m_pDevice);

	for (int i = 0; i < 9; i++) {
		HR(m_pDevice->SetSamplerState(i, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP));
		HR(m_pDevice->SetSamplerState(i, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP));
		HR(m_pDevice->SetSamplerState(i, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP));
	}

	return S_OK;
}

// Applies the video format corresponding to specified pixel precision.
HRESULT D3D9RenderImpl::ApplyPrecision(int precision, int &precisionSizeOut) {
	if (precision == 0)
		precisionSizeOut = 1;
	else if (precision == 1)
		precisionSizeOut = 4;
	else if (precision == 2)
		precisionSizeOut = 8;
	else if (precision == 3)
		precisionSizeOut = 8;
	else
		return E_FAIL;

	return S_OK;
}

D3DFORMAT D3D9RenderImpl::GetD3DFormat(int precision, bool planar) {
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

HRESULT D3D9RenderImpl::CreateDevice(IDirect3DDevice9Ex** device, HWND hDisplayWindow) {
	HR(Direct3DCreate9Ex(D3D_SDK_VERSION, &m_pD3D9));
	if (!m_pD3D9) {
		return E_FAIL;
	}

	D3DCAPS9 deviceCaps;
	HR(m_pD3D9->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &deviceCaps));

	DWORD dwBehaviorFlags = 0; // D3DCREATE_DISABLE_PSGP_THREADING;

	if (deviceCaps.VertexProcessingCaps != 0)
		dwBehaviorFlags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
	else
		dwBehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;

	D3DPRESENT_PARAMETERS m_presentParams;
	HR(GetPresentParams(&m_presentParams, hDisplayWindow));

	HR(m_pD3D9->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hDisplayWindow, dwBehaviorFlags, &m_presentParams, NULL, device));
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

HRESULT D3D9RenderImpl::SetRenderTarget(int width, int height, D3DFORMAT format, IScriptEnvironment* env)
{
	// Skip if current render target has right dimensions.
	if (m_pCurrentRenderTarget != NULL && m_pCurrentRenderTarget->Width == width && m_pCurrentRenderTarget->Height == height && m_pCurrentRenderTarget->Format == format)
		return S_OK;

	ClearRenderTarget();
	m_pCurrentRenderTarget = new RenderTarget{ 0 };

	m_pCurrentRenderTarget->Width = width;
	m_pCurrentRenderTarget->Height = height;
	m_pCurrentRenderTarget->Format = format;
	HR(m_Pool->Allocate(true, width, height, true, format, &m_pCurrentRenderTarget->Texture, &m_pCurrentRenderTarget->Surface));

	HR(SetupMatrices(m_pCurrentRenderTarget, float(width), float(height)));

	HR(m_pDevice->SetRenderTarget(0, m_pCurrentRenderTarget->Surface));
	return m_pDevice->SetTransform(D3DTS_PROJECTION, &m_pCurrentRenderTarget->MatrixOrtho);
}

HRESULT D3D9RenderImpl::ClearRenderTarget() {
	if (m_pCurrentRenderTarget != NULL) {
		m_Pool->Release(m_pCurrentRenderTarget->Surface);
		SafeRelease(m_pCurrentRenderTarget->Surface);
		SafeRelease(m_pCurrentRenderTarget->Texture);
		delete m_pCurrentRenderTarget;
	}
	return S_OK;
}

HRESULT D3D9RenderImpl::SetupMatrices(RenderTarget* target, float width, float height)
{
	D3DXMatrixOrthoOffCenterLH(&target->MatrixOrtho, 0, width, height, 0, 0.0f, 1.0f);

	HR(m_pDevice->CreateVertexBuffer(sizeof(VERTEX) * 4, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_XYZRHW | D3DFVF_TEX1, D3DPOOL_DEFAULT, &target->VertexBuffer, NULL));

	// Create vertexes for FVF (Fixed Vector Function) set to pre-processed vertex coordinates.
	VERTEX vertexArray[] =
	{
		{ 0, 0, 1, 1, 0, 0 }, // top left
		{ width, 0, 1, 1, 1, 0 }, // top right
		{ width, height, 1, 1, 1, 1 }, // bottom right
		{ 0, height, 1, 1, 0, 1 } // bottom left
	};
	// Prevent texture distortion during rasterization. https://msdn.microsoft.com/en-us/library/windows/desktop/bb219690%28v=vs.85%29.aspx?f=255&MSPPError=-2147217396
	for (int i = 0; i < 4; i++) {
		vertexArray[i].x -= .5f;
		vertexArray[i].y -= .5f;
	}

	VERTEX *vertices;
	HR(target->VertexBuffer->Lock(0, 0, (void**)&vertices, D3DLOCK_DISCARD));
	memcpy(vertices, vertexArray, sizeof(vertexArray));
	return(target->VertexBuffer->Unlock());
}

HRESULT D3D9RenderImpl::CreateTexture(int clipIndex, int width, int height, bool isInput, bool isPlanar, bool isLast, int shaderPrecision, InputTexture* outTexture) {
	outTexture->ClipIndex = clipIndex;
	outTexture->Width = width;
	outTexture->Height = height;

	D3DFORMAT Format;
	if (isPlanar) {
		Format = GetD3DFormat(m_ClipPrecision[clipIndex], true);
		HR(m_Pool->Allocate(true, width, height, false, Format, &outTexture->TextureY, &outTexture->SurfaceY));
		HR(m_Pool->Allocate(true, width, height, false, Format, &outTexture->TextureU, &outTexture->SurfaceU));
		HR(m_Pool->Allocate(true, width, height, false, Format, &outTexture->TextureV, &outTexture->SurfaceV));
	}

	if (isLast) {
		if (m_PlanarOut) {
			HR(m_Pool->Allocate(true, width, height, true, GetD3DFormat(m_OutputPrecision, false), &outTexture->Texture, &outTexture->Surface));
			Format = GetD3DFormat(m_OutputPrecision, true);
			HR(m_Pool->Allocate(false, width, height, false, Format, NULL, &outTexture->SurfaceY));
			HR(m_Pool->Allocate(false, width, height, false, Format, NULL, &outTexture->SurfaceU));
			HR(m_Pool->Allocate(false, width, height, false, Format, NULL, &outTexture->SurfaceV));
		}
		else {
			HR(m_Pool->Allocate(false, width, height, false, GetD3DFormat(m_OutputPrecision, false), NULL, &outTexture->Memory));
		}
	}
	else {
		Format = GetD3DFormat(isInput ? m_ClipPrecision[clipIndex] : shaderPrecision > -1 ? shaderPrecision : m_Precision, false);
		HR(m_Pool->Allocate(true, width, height, true, Format, &outTexture->Texture, &outTexture->Surface));
	}
	return S_OK;
}

// Only one texture per ClipIndex should be kept in the list.
InputTexture* D3D9RenderImpl::FindTexture(std::vector<InputTexture*>* textureList, int clipIndex) {
	for (auto const& item : *textureList) {
		if (item->ClipIndex == clipIndex)
			return item;
	}
	return NULL;
}

HRESULT D3D9RenderImpl::RemoveTexture(std::vector<InputTexture*>* textureList, InputTexture* item) {
	textureList->erase(std::remove(textureList->begin(), textureList->end(), item), textureList->end());
	HR(ReleaseTexture(item));
	return S_OK;
}

HRESULT D3D9RenderImpl::ClearTextures(std::vector<InputTexture*>* textureList) {
	for (auto const& item : *textureList) {
		HR(ReleaseTexture(item));
	}
	textureList->clear();
	return S_OK;
}

HRESULT D3D9RenderImpl::ReleaseTexture(InputTexture* obj) {
	m_Pool->Release(obj->Surface);
	SafeRelease(obj->Surface);
	SafeRelease(obj->Texture);
	m_Pool->Release(obj->Memory);
	SafeRelease(obj->Memory);
	m_Pool->Release(obj->SurfaceY);
	SafeRelease(obj->SurfaceY);
	m_Pool->Release(obj->SurfaceU);
	SafeRelease(obj->SurfaceU);
	m_Pool->Release(obj->SurfaceV);
	SafeRelease(obj->SurfaceV);
	delete obj;
	return S_OK;
}

HRESULT D3D9RenderImpl::ProcessFrame(std::vector<InputTexture*>* textureList, CommandStruct* cmd, int width, int height, bool isLast, int planeOut, IScriptEnvironment* env)
{
	HR(m_pDevice->TestCooperativeLevel());
	D3DFORMAT fmt = planeOut > 0 ? GetD3DFormat(m_OutputPrecision, true) : GetD3DFormat(isLast ? m_OutputPrecision : m_Precision, false);
	HR(SetRenderTarget(width, height, fmt, env));
	HR(CreateScene(textureList, cmd, planeOut, env));
	return CopyFromRenderTarget(textureList, cmd, width, height, isLast, planeOut, env);
}

HRESULT D3D9RenderImpl::CreateScene(std::vector<InputTexture*>* textureList, CommandStruct* cmd, int planeOut, IScriptEnvironment* env)
{
	HR(m_pDevice->Clear(D3DADAPTER_DEFAULT, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0));
	HR(m_pDevice->BeginScene());
	SCENE_HR(m_pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1), m_pDevice);
	SCENE_HR(m_pDevice->SetPixelShader(m_Shaders[cmd->CommandIndex + planeOut].Shader), m_pDevice);
	SCENE_HR(m_pDevice->SetStreamSource(0, m_pCurrentRenderTarget->VertexBuffer, 0, sizeof(VERTEX)), m_pDevice);

	// Clear samplers
	for (int i = 0; i < 9; i++) {
		SCENE_HR(m_pDevice->SetTexture(i, NULL), m_pDevice);
	}

	// Set input clips.
	InputTexture* Input;
	for (int i = 0; i < 9; i++) {
		if (cmd->ClipIndex[i] > 0) {
			Input = FindTexture(textureList, cmd->ClipIndex[i]);
			if (Input != NULL && (Input->Texture != NULL || Input->TextureY != NULL)) {
				if (Input->TextureY == NULL) {
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
	PrepareReadTarget(textureList, cmd->OutputIndex, width, height, planeOut, isLast, &dst);

	CComPtr<IDirect3DSurface9> pReadSurfaceGpu;
	HR(m_pDevice->GetRenderTarget(0, &pReadSurfaceGpu));
	dst->ClipIndex = cmd->OutputIndex;
	if (!isLast) {
		HR(D3DXLoadSurfaceFromSurface(dst->Surface, NULL, NULL, pReadSurfaceGpu, NULL, NULL, D3DX_FILTER_NONE, 0));
	}
	else {
		if (planeOut > 0) {
			// This gets called recursively from the code below for each plane.
			IDirect3DSurface9* SurfaceOut = planeOut == 1 ? dst->SurfaceY : planeOut == 2 ? dst->SurfaceU : planeOut == 3 ? dst->SurfaceV : NULL;
			HR(m_pDevice->GetRenderTargetData(pReadSurfaceGpu, SurfaceOut));
		}
		else if (m_PlanarOut) {
			HR(D3DXLoadSurfaceFromSurface(dst->Surface, NULL, NULL, pReadSurfaceGpu, NULL, NULL, D3DX_FILTER_NONE, 0));

			CommandStruct PlanarCmd = { 0 };
			PlanarCmd.Path = "OutputY.cso";
			PlanarCmd.CommandIndex = cmd->CommandIndex;
			PlanarCmd.ClipIndex[0] = 1;
			PlanarCmd.OutputIndex = 1;
			PlanarCmd.Precision = 0;
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
		else {
			// If reading last command, copy it back to CPU directly
			HR(m_pDevice->GetRenderTargetData(pReadSurfaceGpu, dst->Memory));
		}
	}
	return S_OK;
}

HRESULT D3D9RenderImpl::CopyBuffer(std::vector<InputTexture*>* textureList, InputTexture* src, int outputIndex, IScriptEnvironment* env) {
	InputTexture* dst;
	PrepareReadTarget(textureList, outputIndex, src->Width, src->Height, 0, false, &dst);
	dst->ClipIndex = outputIndex;

	if (src->TextureY == NULL) {
		HR(D3DXLoadSurfaceFromSurface(dst->Surface, NULL, NULL, src->Surface, NULL, NULL, D3DX_FILTER_NONE, 0));
	}
	else {
		HR(D3DXLoadSurfaceFromSurface(dst->SurfaceY, NULL, NULL, src->SurfaceY, NULL, NULL, D3DX_FILTER_NONE, 0));
		HR(D3DXLoadSurfaceFromSurface(dst->SurfaceU, NULL, NULL, src->SurfaceU, NULL, NULL, D3DX_FILTER_NONE, 0));
		HR(D3DXLoadSurfaceFromSurface(dst->SurfaceV, NULL, NULL, src->SurfaceV, NULL, NULL, D3DX_FILTER_NONE, 0));
	}
	return S_OK;
}

HRESULT D3D9RenderImpl::PrepareReadTarget(std::vector<InputTexture*>* textureList, int outputIndex, int width, int height, int planeOut, bool isLast, InputTexture** outDst) {
	InputTexture* dst = FindTexture(textureList, outputIndex);
	if (planeOut == 0) {
		// Remove previous item with OutputIndex and replace it with new texture
		if (dst != NULL)
			RemoveTexture(textureList, dst);
		dst = new InputTexture();
		HR(CreateTexture(outputIndex, width, height, false, false, isLast, -1, dst));
		textureList->push_back(dst);
	}
	*outDst = dst;
}

HRESULT D3D9RenderImpl::CopyAviSynthToBuffer(const byte* src, int srcPitch, int index, int width, int height, InputTexture* dst, IScriptEnvironment* env) {
	// Copies source frame into main surface buffer, or into additional input textures
	RECT SrcRect;
	SrcRect.top = 0;
	SrcRect.left = 0;
	SrcRect.right = width;
	SrcRect.bottom = height;
	HR(D3DXLoadSurfaceFromMemory(dst->Surface, NULL, NULL, src, GetD3DFormat(m_ClipPrecision[index], false), srcPitch, NULL, &SrcRect, D3DX_FILTER_NONE, 0));
	return S_OK;
}

HRESULT D3D9RenderImpl::CopyAviSynthToPlanarBuffer(const byte* srcY, const byte* srcU, const byte* srcV, int srcPitch, int index, int width, int height, InputTexture* dst, IScriptEnvironment* env) {
	// Copies source frame into main surface buffer, or into additional input textures
	RECT SrcRect;
	SrcRect.top = 0;
	SrcRect.left = 0;
	SrcRect.right = width;
	SrcRect.bottom = height;
	D3DFORMAT fmt = GetD3DFormat(m_ClipPrecision[index], true);
	HR(D3DXLoadSurfaceFromMemory(dst->SurfaceY, NULL, NULL, srcY, fmt, srcPitch, NULL, &SrcRect, D3DX_FILTER_NONE, 0));
	HR(D3DXLoadSurfaceFromMemory(dst->SurfaceU, NULL, NULL, srcU, fmt, srcPitch, NULL, &SrcRect, D3DX_FILTER_NONE, 0));
	HR(D3DXLoadSurfaceFromMemory(dst->SurfaceV, NULL, NULL, srcV, fmt, srcPitch, NULL, &SrcRect, D3DX_FILTER_NONE, 0));
	return S_OK;
}

HRESULT D3D9RenderImpl::CopyBufferToAviSynth(int commandIndex, InputTexture* src, byte* dst, int dstPitch, IScriptEnvironment* env) {
	HR(CopyBufferToAviSynthInternal(src->Memory, dst, dstPitch, src->Width * m_OutputPrecisionSize, src->Height, env));
	return S_OK;
}

HRESULT D3D9RenderImpl::CopyBufferToAviSynthPlanar(int commandIndex, InputTexture* src, byte* dstY, byte* dstU, byte* dstV, int dstPitch, IScriptEnvironment* env) {
	HR(CopyBufferToAviSynthInternal(src->SurfaceY, dstY, dstPitch, src->Width, src->Height, env));
	HR(CopyBufferToAviSynthInternal(src->SurfaceU, dstU, dstPitch, src->Width, src->Height, env));
	HR(CopyBufferToAviSynthInternal(src->SurfaceV, dstV, dstPitch, src->Width, src->Height, env));
	return S_OK;
}

HRESULT D3D9RenderImpl::CopyBufferToAviSynthInternal(IDirect3DSurface9* surface, byte* dst, int dstPitch, int rowSize, int height, IScriptEnvironment* env) {
	D3DLOCKED_RECT srcRect;
	HR(surface->LockRect(&srcRect, NULL, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY));
	BYTE* srcPict = (BYTE*)srcRect.pBits;
	env->BitBlt(dst, dstPitch, srcPict, srcRect.Pitch, rowSize, height);
	HR(surface->UnlockRect());
	return S_OK;
}

HRESULT D3D9RenderImpl::InitPixelShader(CommandStruct* cmd, int planeOut, IScriptEnvironment* env) {
	// PlaneOut will use the next 3 shader positions
	ShaderItem* Shader = &m_Shaders[cmd->CommandIndex + planeOut];
	if (Shader->Shader != NULL)
		return S_OK;

	CComPtr<ID3DXBuffer> code;
	unsigned char* ShaderBuf = NULL;
	DWORD* CodeBuffer = NULL;

	if (!StringEndsWith(cmd->Path, ".hlsl")) {
		// Precompiled shader
		ShaderBuf = ReadBinaryFile(cmd->Path);
		if (ShaderBuf == NULL)
			return E_FAIL;
		HR(D3DXGetShaderConstantTable((DWORD*)ShaderBuf, &Shader->ConstantTable));
		CodeBuffer = (DWORD*)ShaderBuf;
	}
	else {
		// Compile HLSL shader code
		if (D3DXCompileShaderFromFile(cmd->Path, NULL, NULL, cmd->EntryPoint, cmd->ShaderModel, 0, &code, NULL, &Shader->ConstantTable) != S_OK) {
			// Try in same folder as DLL file.
			char path[MAX_PATH];
			GetDefaultPath(path, MAX_PATH, cmd->Path);
			HR(D3DXCompileShaderFromFile(path, NULL, NULL, cmd->EntryPoint, cmd->ShaderModel, 0, &code, NULL, &Shader->ConstantTable));
		}
		CodeBuffer = (DWORD*)code->GetBufferPointer();
	}

	HR(m_pDevice->CreatePixelShader(CodeBuffer, &Shader->Shader));

	if (ShaderBuf != NULL)
		free(ShaderBuf);
	return S_OK;
}

unsigned char* D3D9RenderImpl::ReadBinaryFile(const char* filePath) {
	FILE *fl = fopen(filePath, "rb");
	if (fl == NULL) {
		// Try in same folder as DLL file.
		char path[MAX_PATH];
		GetDefaultPath(path, MAX_PATH, filePath);
		fl = fopen(path, "rb");
	}
	if (fl != NULL)
	{
		fseek(fl, 0, SEEK_END);
		long len = ftell(fl);
		unsigned char *ret = (unsigned char*)malloc(len);
		fseek(fl, 0, SEEK_SET);
		fread(ret, 1, len, fl);
		fclose(fl);
		return ret;
	}
	else
		return NULL;
}

// returns 1 if str ends with suffix
bool D3D9RenderImpl::StringEndsWith(const char * str, const char * suffix) {
	if (str == NULL || suffix == NULL)
		return false;

	size_t str_len = strlen(str);
	size_t suffix_len = strlen(suffix);

	if (suffix_len > str_len)
		return false;

	return 0 == strncmp(str + str_len - suffix_len, suffix, suffix_len);
}

// Gets the path where the DLL file is located.
void D3D9RenderImpl::GetDefaultPath(char* outPath, int maxSize, const char* filePath)
{
	HMODULE hm = NULL;

	if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
		GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCSTR)&StaticFunction,
		&hm))
	{
		int ret = GetLastError();
		fprintf(stderr, "GetModuleHandle returned %d\n", ret);
	}
	GetModuleFileNameA(hm, outPath, maxSize);

	// Strip the file name to keep the path ending with '\'
	char *pos = strrchr(outPath, '\\') + 1;
	if (pos != NULL) {
		strcpy(pos, filePath);
	}
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
