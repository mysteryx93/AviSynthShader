// Code originally taken from Taygeta Video Presender available here.
// The code has been mostly rewritten for our needs.
// http://www.codeproject.com/Articles/207642/Video-Shadering-with-Direct-D

// Memory transfers explained
// We can only read/write to a texture that is on the CPU side, and DirectX9 can only 
// do its work with textures on the GPU side. We must transfer data around.
//
// In this sample, we have 3 input textures out of 9 slots, and we run 3 shaders.
// GPU = D3DPOOL_DEFAULT, CPU = D3DPOOL_SYSTEMMEM
//
// m_InputTextures
// Index  0 1 2 3 4 5 6 7 8 9 10 11
// CPU                            X
// GPU      X X             X  X  X
// YUV    X                           
//
// Planar allows transfering the data in and out as 3 planes which reduces transfers by 25% (no Alpha plane)
// PlanarIn: First clip has 3x L8 textures that will be passed to the first shader as s0, s1 and s2.
// PlanarOut: Output as YUV to GPU memory, then for each of Y, U and V, run shader to fill L8 plane and transfer L8 plane 
//            to CPU before returning all 3 planes to AviSynth. All 3 planes will be transfered via the same surface.
//
// m_RenderTargets contains one R texture per output resolution. The Render Target then gets 
// copied into the next available index. Command1 outputs to RenderTarget[0] and then
// to index 9, Command2 outputs to index 10, Command3 outputs to index 11, etc.
// Only the final output needs to be copied from the GPU back onto the CPU, 
// requiring a SYSTEMMEM texture.

#include "D3D9RenderImpl.h"

#pragma warning(disable: 4996)

D3D9RenderImpl::D3D9RenderImpl() {
	ZeroMemory(m_InputTextures, sizeof(InputTexture) * maxTextures);
	ZeroMemory(m_RenderTargets, sizeof(RenderTarget) * maxTextures);
	ZeroMemory(m_Shaders, sizeof(ShaderItem) * maxTextures);
}

D3D9RenderImpl::~D3D9RenderImpl(void) {
	for (int i = 0; i < maxTextures; i++) {
		SafeRelease(m_InputTextures[0].Surface);
		SafeRelease(m_InputTextures[0].Texture);
		SafeRelease(m_InputTextures[0].Memory);
	}
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

	// Find a render target with desired proportions.
	RenderTarget* Target = NULL;
	int i = 0;
	while (Target == NULL && m_RenderTargets[i].Texture != NULL) {
		if (m_RenderTargets[i].Width == width && m_RenderTargets[i].Height == height && m_RenderTargets[i].Format == format)
			Target = &m_RenderTargets[i];
		else
			i++;
	}

	// Create it if it doesn't yet exist.
	if (Target == NULL) {
		// Find next unasigned texture spot.
		i = 0;
		while (Target == NULL && m_RenderTargets[i].Texture != NULL) {
			if (i++ >= maxTextures)
				env->ThrowError("ExecuteShader: Cannot output to more than 50 resolutions in a command chain");
		}
		Target = &m_RenderTargets[i];

		Target->Width = width;
		Target->Height = height;
		Target->Format = format;
		HR(CreateSurface(width, height, true, format, &Target->Texture, &Target->Surface));

		HR(SetupMatrices(Target, float(width), float(height)));
	}

	// Set render target.
	m_pCurrentRenderTarget = Target;
	HR(m_pDevice->SetRenderTarget(0, Target->Surface));
	return m_pDevice->SetTransform(D3DTS_PROJECTION, &Target->MatrixOrtho);
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

HRESULT D3D9RenderImpl::CreateInputTexture(int index, int clipIndex, int width, int height, bool isInput, bool isPlanar, bool isLast, int shaderPrecision) {
	if (index < 0 || index >= maxTextures)
		return E_FAIL;

	InputTexture* Obj = &m_InputTextures[index];
	Obj->ClipIndex = clipIndex;
	Obj->Width = width;
	Obj->Height = height;

	if (isPlanar) {
		HR(CreateSurface(width, height, false, GetD3DFormat(m_ClipPrecision[clipIndex], true), &Obj->TextureY, &Obj->SurfaceY));
		HR(CreateSurface(width, height, false, GetD3DFormat(m_ClipPrecision[clipIndex], true), &Obj->TextureU, &Obj->SurfaceU));
		HR(CreateSurface(width, height, false, GetD3DFormat(m_ClipPrecision[clipIndex], true), &Obj->TextureV, &Obj->SurfaceV));
	}

	if (isLast) {
		if (m_PlanarOut) {
			HR(CreateSurface(width, height, true, GetD3DFormat(m_OutputPrecision, false), &Obj->Texture, &Obj->Surface));
			HR(m_pDevice->CreateOffscreenPlainSurface(width, height, GetD3DFormat(m_OutputPrecision, true), D3DPOOL_SYSTEMMEM, &Obj->SurfaceY, NULL));
			HR(m_pDevice->CreateOffscreenPlainSurface(width, height, GetD3DFormat(m_OutputPrecision, true), D3DPOOL_SYSTEMMEM, &Obj->SurfaceU, NULL));
			HR(m_pDevice->CreateOffscreenPlainSurface(width, height, GetD3DFormat(m_OutputPrecision, true), D3DPOOL_SYSTEMMEM, &Obj->SurfaceV, NULL));
		}
		else {
			HR(m_pDevice->CreateOffscreenPlainSurface(width, height, GetD3DFormat(m_OutputPrecision, false), D3DPOOL_SYSTEMMEM, &Obj->Memory, NULL));
		}
	} else {
		D3DFORMAT Format = GetD3DFormat(isInput ? m_ClipPrecision[clipIndex] : shaderPrecision > -1 ? shaderPrecision : m_Precision, false);
		HR(CreateSurface(width, height, true, Format, &Obj->Texture, &Obj->Surface));
	}

	return S_OK;
}

HRESULT D3D9RenderImpl::CreateSurface(int width, int height, bool renderTarget, D3DFORMAT format, IDirect3DTexture9 **texture, IDirect3DSurface9 **surface) {
	HR(m_pDevice->CreateTexture(width, height, 1, renderTarget ? D3DUSAGE_RENDERTARGET : NULL, format, D3DPOOL_DEFAULT, texture, NULL));
	HR((*texture)->GetSurfaceLevel(0, surface));
	return S_OK;
}

InputTexture* D3D9RenderImpl::FindTextureByClipIndex(int clipIndex, IScriptEnvironment* env) {
	int Result = -1;
	int ItemIndex;
	for (int i = 0; i < maxTextures; i++) {
		ItemIndex = m_InputTextures[i].ClipIndex;
		if (ItemIndex == clipIndex)
			Result = i;
		else if (ItemIndex == 0)
			break;
	}
	if (Result == -1)
		return NULL;
	return &m_InputTextures[Result];
}

void D3D9RenderImpl::ResetTextureClipIndex() {
	for (int i = maxClips; i < maxTextures; i++) {
		m_InputTextures[i].ClipIndex = 0;
	}
}

HRESULT D3D9RenderImpl::ProcessFrame(CommandStruct* cmd, int width, int height, bool isLast, int planeOut, IScriptEnvironment* env)
{
	HR(m_pDevice->TestCooperativeLevel());
	D3DFORMAT fmt = planeOut > 0 ? GetD3DFormat(m_OutputPrecision, true) : GetD3DFormat(isLast ? m_OutputPrecision : m_Precision, false);
	HR(SetRenderTarget(width, height, fmt, env));
	HR(CreateScene(cmd, planeOut,  env));
	return CopyFromRenderTarget(maxClips + cmd->CommandIndex, cmd->OutputIndex, width, height, isLast, planeOut, env);
}

HRESULT D3D9RenderImpl::CreateScene(CommandStruct* cmd, int planeOut, IScriptEnvironment* env)
{
	HR(m_pDevice->Clear(D3DADAPTER_DEFAULT, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0));
	HR(m_pDevice->BeginScene());
	SCENE_HR(m_pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1), m_pDevice);
	SCENE_HR(m_pDevice->SetPixelShader(m_Shaders[cmd->CommandIndex - 1 + planeOut].Shader), m_pDevice);
	SCENE_HR(m_pDevice->SetStreamSource(0, m_pCurrentRenderTarget->VertexBuffer, 0, sizeof(VERTEX)), m_pDevice);

	// Clear samplers
	for (int i = 0; i < 9; i++) {
		SCENE_HR(m_pDevice->SetTexture(i, NULL), m_pDevice);
	}

	// Set input clips.
	InputTexture* Input;
	for (int i = 0; i < 9; i++) {
		if (cmd->ClipIndex[i] > 0) {
			Input = FindTextureByClipIndex(cmd->ClipIndex[i], env);
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

HRESULT D3D9RenderImpl::CopyFromRenderTarget(int dstIndex, int outputIndex, int width, int height, bool isLast, int planeOut, IScriptEnvironment* env)
{
	if (dstIndex < 0 || dstIndex >= maxTextures)
		return E_FAIL;

	InputTexture* Output = &m_InputTextures[dstIndex];
	CComPtr<IDirect3DSurface9> pReadSurfaceGpu;
	HR(m_pDevice->GetRenderTarget(0, &pReadSurfaceGpu));
	Output->ClipIndex = outputIndex;
	if (!isLast) {
		HR(D3DXLoadSurfaceFromSurface(Output->Surface, NULL, NULL, pReadSurfaceGpu, NULL, NULL, D3DX_FILTER_NONE, 0));
	}
	else {
		if (planeOut > 0) {
			// This gets called recursively from the code below for each plane.
			IDirect3DSurface9* SurfaceOut = planeOut == 1 ? Output->SurfaceY : planeOut == 2 ? Output->SurfaceU : planeOut == 3 ? Output->SurfaceV : NULL;
			HR(m_pDevice->GetRenderTargetData(pReadSurfaceGpu, SurfaceOut));
		}
		else if (m_PlanarOut) {
			HR(D3DXLoadSurfaceFromSurface(Output->Surface, NULL, NULL, pReadSurfaceGpu, NULL, NULL, D3DX_FILTER_NONE, 0));

			CommandStruct cmd = { 0 };
			cmd.Path = "OutputY.cso";
			cmd.CommandIndex = dstIndex - maxClips;
			cmd.ClipIndex[0] = 1;
			cmd.OutputIndex = 1;
			cmd.Precision = 0;
			if FAILED(InitPixelShader(&cmd, 1, env))
				env->ThrowError("ExecuteShader: OutputY.cso not found");
			HR(ProcessFrame(&cmd, width, height, true, 1, env));
			cmd.Path = "OutputU.cso";
			if FAILED(InitPixelShader(&cmd, 2, env))
				env->ThrowError("ExecuteShader: OutputU.cso not found");
			HR(ProcessFrame(&cmd, width, height, true, 2, env));
			cmd.Path = "OutputV.cso";
			if FAILED(InitPixelShader(&cmd, 3, env))
				env->ThrowError("ExecuteShader: OutputV.cso not found");
			HR(ProcessFrame(&cmd, width, height, true, 3, env));
		}
		else {
			// If reading last command, copy it back to CPU directly
			HR(m_pDevice->GetRenderTargetData(pReadSurfaceGpu, Output->Memory));
		}
	}
	return S_OK;
}

HRESULT D3D9RenderImpl::CopyBuffer(InputTexture* srcSurface, int commandIndex, int outputIndex, IScriptEnvironment* env) {
	InputTexture* dstSurface = &m_InputTextures[maxClips + commandIndex];
	dstSurface->ClipIndex = outputIndex;

	if (srcSurface->TextureY == NULL) {
		HR(D3DXLoadSurfaceFromSurface(dstSurface->Surface, NULL, NULL, srcSurface->Surface, NULL, NULL, D3DX_FILTER_NONE, 0));
	}
	else {
		HR(D3DXLoadSurfaceFromSurface(dstSurface->SurfaceY, NULL, NULL, srcSurface->SurfaceY, NULL, NULL, D3DX_FILTER_NONE, 0));
		HR(D3DXLoadSurfaceFromSurface(dstSurface->SurfaceU, NULL, NULL, srcSurface->SurfaceU, NULL, NULL, D3DX_FILTER_NONE, 0));
		HR(D3DXLoadSurfaceFromSurface(dstSurface->SurfaceV, NULL, NULL, srcSurface->SurfaceV, NULL, NULL, D3DX_FILTER_NONE, 0));
	}
	return S_OK;
}

HRESULT D3D9RenderImpl::CopyAviSynthToBuffer(const byte* src, int srcPitch, int index, int width, int height, IScriptEnvironment* env) {
	// Copies source frame into main surface buffer, or into additional input textures
	if (index < 0 || index >= maxTextures)
		return E_FAIL;

	RECT SrcRect;
	SrcRect.top = 0;
	SrcRect.left = 0;
	SrcRect.right = width;
	SrcRect.bottom = height;
	HR(D3DXLoadSurfaceFromMemory(m_InputTextures[index].Surface, NULL, NULL, src, GetD3DFormat(m_ClipPrecision[index], false), srcPitch, NULL, &SrcRect, D3DX_FILTER_NONE, 0));
	return S_OK;
}

HRESULT D3D9RenderImpl::CopyAviSynthToPlanarBuffer(const byte* srcY, const byte* srcU, const byte* srcV, int srcPitch, int index, int width, int height, IScriptEnvironment* env) {
	// Copies source frame into main surface buffer, or into additional input textures
	if (index < 0 || index >= maxTextures)
		return E_FAIL;

	RECT SrcRect;
	SrcRect.top = 0;
	SrcRect.left = 0;
	SrcRect.right = width;
	SrcRect.bottom = height;
	D3DFORMAT fmt = GetD3DFormat(m_ClipPrecision[index], true);
	HR(D3DXLoadSurfaceFromMemory(m_InputTextures[index].SurfaceY, NULL, NULL, srcY, fmt, srcPitch, NULL, &SrcRect, D3DX_FILTER_NONE, 0));
	HR(D3DXLoadSurfaceFromMemory(m_InputTextures[index].SurfaceU, NULL, NULL, srcU, fmt, srcPitch, NULL, &SrcRect, D3DX_FILTER_NONE, 0));
	HR(D3DXLoadSurfaceFromMemory(m_InputTextures[index].SurfaceV, NULL, NULL, srcV, fmt, srcPitch, NULL, &SrcRect, D3DX_FILTER_NONE, 0));
	return S_OK;
}

HRESULT D3D9RenderImpl::CopyBufferToAviSynth(int commandIndex, byte* dst, int dstPitch, IScriptEnvironment* env) {
	InputTexture* ReadSurface = &m_InputTextures[maxClips + commandIndex];
	HR(CopyBufferToAviSynthInternal(ReadSurface->Memory, dst, dstPitch, ReadSurface->Width * m_OutputPrecisionSize, ReadSurface->Height, env));
	return S_OK;
}

HRESULT D3D9RenderImpl::CopyBufferToAviSynthPlanar(int commandIndex, byte* dstY, byte* dstU, byte* dstV, int dstPitch, IScriptEnvironment* env) {
	InputTexture* ReadSurface = &m_InputTextures[maxClips + commandIndex];
	HR(CopyBufferToAviSynthInternal(ReadSurface->SurfaceY, dstY, dstPitch, ReadSurface->Width, ReadSurface->Height, env));
	HR(CopyBufferToAviSynthInternal(ReadSurface->SurfaceU, dstU, dstPitch, ReadSurface->Width, ReadSurface->Height, env));
	HR(CopyBufferToAviSynthInternal(ReadSurface->SurfaceV, dstV, dstPitch, ReadSurface->Width, ReadSurface->Height, env));
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
	ShaderItem* Shader = &m_Shaders[cmd->CommandIndex - 1 + planeOut];
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
