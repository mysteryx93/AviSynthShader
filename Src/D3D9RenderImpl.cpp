// Code originally taken from Taygeta Video Presender available here.
// The code has been mostly rewritten for our needs.
// http://www.codeproject.com/Articles/207642/Video-Shadering-with-Direct-D

// Memory transfers explained
// We can only read/write to a texture that is on the CPU side, and DirectX9 can only 
// do its work with textures on the GPU side. We must transfer data around.
//
// In this sample, we have 3 input textures out of 9 slots, and we run 3 shaders.
// D = D3DPOOL_DEFAULT, S = D3DPOOL_SYSTEMMEM, R = D3DUSAGE_RENDERTARGET
//
// m_InputTextures
// Index  0 1 2 3 4 5 6 7 8 9 10 11
// CPU                            S
// GPU    R R R             R  R  R
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

HRESULT D3D9RenderImpl::Initialize(HWND hDisplayWindow, int clipPrecision[9], int precision, int outputPrecision) {
	HR(ApplyPrecision(precision, m_Precision, m_Format));
	for (int i = 0; i < 9; i++) {
		HR(ApplyPrecision(clipPrecision[i], m_ClipPrecision[i], m_ClipFormat[i]));
	}
	HR(ApplyPrecision(outputPrecision, m_OutputPrecision, m_OutputFormat));

	HR(CreateDevice(&m_pDevice, hDisplayWindow));

	for (int i = 0; i < 9; i++) {
		HR(m_pDevice->SetSamplerState(i, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP));
		HR(m_pDevice->SetSamplerState(i, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP));
		HR(m_pDevice->SetSamplerState(i, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP));
	}

	return S_OK;
}

// Applies the video format corresponding to specified pixel precision.
HRESULT D3D9RenderImpl::ApplyPrecision(int precision, int &precisionOut, D3DFORMAT &formatOut) {
	if (precision == 0) {
		formatOut = D3DFMT_L8;
		precisionOut = 1;
	}
	else if (precision == 1) {
		formatOut = D3DFMT_A8R8G8B8;
		precisionOut = 4;
	}
	else if (precision == 2) {
		formatOut = D3DFMT_A16B16G16R16;
		precisionOut = 8;
	}
	else if (precision == 3) {
		formatOut = D3DFMT_A16B16G16R16F;
		precisionOut = 8;
	}
	else
		return E_FAIL;

	return S_OK;
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
	presentParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
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
		HR(m_pDevice->CreateOffscreenPlainSurface(width, height, format, D3DPOOL_SYSTEMMEM, &Target->Memory, NULL));

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

HRESULT D3D9RenderImpl::CreateInputTexture(int index, int clipIndex, int width, int height, bool memoryTexture, bool isSystemMemory) {
	if (index < 0 || index >= maxTextures)
		return E_FAIL;

	InputTexture* Obj = &m_InputTextures[index];
	Obj->ClipIndex = clipIndex;
	Obj->Width = width;
	Obj->Height = height;

	D3DFORMAT OutputFormat = m_Format;
	bool RenderTarget = true;
	if (memoryTexture && !isSystemMemory) {
		OutputFormat = m_ClipFormat[index];
		//if (m_ClipPrecision[index] == 1) {
		//	HR(CreateSurface(width, height, false, D3DFMT_L8, &Obj->TextureY, &Obj->SurfaceY));
		//	HR(CreateSurface(width, height, false, D3DFMT_L8, &Obj->TextureU, &Obj->SurfaceU));
		//	HR(CreateSurface(width, height, false, D3DFMT_L8, &Obj->TextureV, &Obj->SurfaceV));
		//}
		//else
		//	RenderTarget = false;
		//HR(m_pDevice->CreateOffscreenPlainSurface(width, height, m_ClipFormat[index], D3DPOOL_DEFAULT, &Obj->Memory, NULL));
	}
	else if (isSystemMemory) {
		HR(m_pDevice->CreateOffscreenPlainSurface(width, height, m_OutputFormat, D3DPOOL_SYSTEMMEM, &Obj->Memory, NULL));
	}
	if (!isSystemMemory) {
		HR(CreateSurface(width, height, RenderTarget, OutputFormat, &Obj->Texture, &Obj->Surface));
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
		env->ThrowError("Shader: Clip index not found");
	return &m_InputTextures[Result];
}

void D3D9RenderImpl::ResetTextureClipIndex() {
	for (int i = 9; i < maxTextures; i++) {
		m_InputTextures[i].ClipIndex = 0;
	}
}

HRESULT D3D9RenderImpl::ProcessFrame(CommandStruct* cmd, int width, int height, bool isLast, IScriptEnvironment* env)
{
	HR(m_pDevice->TestCooperativeLevel());
	HR(SetRenderTarget(width, height, isLast ? m_OutputFormat : m_Format, env));
	HR(CreateScene(cmd, env));
	HR(m_pDevice->Present(NULL, NULL, NULL, NULL));
	return CopyFromRenderTarget(9 + cmd->CommandIndex, cmd->OutputIndex, width, height);
}

HRESULT D3D9RenderImpl::CreateScene(CommandStruct* cmd, IScriptEnvironment* env)
{
	HR(m_pDevice->Clear(D3DADAPTER_DEFAULT, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0));
	HR(m_pDevice->BeginScene());
	SCENE_HR(m_pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1), m_pDevice);
	SCENE_HR(m_pDevice->SetPixelShader(m_Shaders[cmd->CommandIndex].Shader), m_pDevice);
	SCENE_HR(m_pDevice->SetStreamSource(0, m_pCurrentRenderTarget->VertexBuffer, 0, sizeof(VERTEX)), m_pDevice);

	// Set input clips.
	InputTexture* Input;
	for (int i = 0; i < 9; i++) {
		if (cmd->ClipIndex[i] > 0) {
			Input = FindTextureByClipIndex(cmd->ClipIndex[i], env);
			if (Input != NULL) {
				SCENE_HR(m_pDevice->SetTexture(i, Input->Texture), m_pDevice);
			}
			else
				env->ThrowError("Shader: Invalid clip index.");
		}
	}

	SCENE_HR(m_pDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2), m_pDevice);
	return m_pDevice->EndScene();
}

HRESULT D3D9RenderImpl::CopyBuffer(InputTexture* srcSurface, int commandIndex, int outputIndex, IScriptEnvironment* env) {
	InputTexture* dstSurface = &m_InputTextures[9 + commandIndex];
	dstSurface->ClipIndex = outputIndex;

	// HR(m_pDevice->ColorFill(dstSurface->Surface, NULL, D3DCOLOR_ARGB(0xFF, 0, 0, 0)));
	// HR(m_pDevice->StretchRect(srcSurface->Surface, NULL, dstSurface->Surface, NULL, D3DTEXF_POINT));
	HR(D3DXLoadSurfaceFromSurface(dstSurface->Surface, NULL, NULL, srcSurface->Surface, NULL, NULL, D3DX_FILTER_NONE, 0));
	return S_OK;
}

HRESULT D3D9RenderImpl::CopyAviSynthToBuffer(const byte* src, int srcPitch, int index, int width, int height, IScriptEnvironment* env) {
	// Copies source frame into main surface buffer, or into additional input textures
	CComPtr<IDirect3DSurface9> destSurface = m_InputTextures[index].Memory;
	if (index < 0 || index >= maxTextures)
		return E_FAIL;

	//D3DLOCKED_RECT d3drect;
	//HR(destSurface->LockRect(&d3drect, NULL, 0));
	//BYTE* pict = (BYTE*)d3drect.pBits;

	//env->BitBlt(pict, d3drect.Pitch, src, srcPitch, width * m_ClipPrecision[index], height);

	//HR(destSurface->UnlockRect());

	// Copy to GPU
	//HR(m_pDevice->ColorFill(m_InputTextures[index].Surface, NULL, D3DCOLOR_ARGB(0xFF, 0, 0, 0)));
	//HR(m_pDevice->StretchRect(m_InputTextures[index].Memory, NULL, m_InputTextures[index].Surface, NULL, D3DTEXF_POINT));
	RECT SrcRect;
	SrcRect.top = 0;
	SrcRect.left = 0;
	SrcRect.right = width;
	SrcRect.bottom = height;
	HR(D3DXLoadSurfaceFromMemory(m_InputTextures[index].Surface, NULL, NULL, src, m_ClipFormat[index], srcPitch, NULL, &SrcRect, D3DX_FILTER_NONE, 0));
	return S_OK;
}

HRESULT D3D9RenderImpl::CopyFromRenderTarget(int dstIndex, int outputIndex, int width, int height)
{
	if (dstIndex < 0 || dstIndex >= maxTextures)
		return E_FAIL;

	InputTexture* Output = &m_InputTextures[dstIndex];
	CComPtr<IDirect3DSurface9> pReadSurfaceGpu;
	HR(m_pDevice->GetRenderTarget(0, &pReadSurfaceGpu));
	Output->ClipIndex = outputIndex;
	if (Output->Memory == NULL) {
		//HR(m_pDevice->ColorFill(Output->Surface, NULL, D3DCOLOR_ARGB(0xFF, 0, 0, 0)));
		// HR(m_pDevice->StretchRect(pReadSurfaceGpu, NULL, Output->Surface, NULL, D3DTEXF_POINT));
		HR(D3DXLoadSurfaceFromSurface(Output->Surface, NULL, NULL, pReadSurfaceGpu, NULL, NULL, D3DX_FILTER_NONE, 0));
	}
	else {
		// If reading last command, copy it back to CPU directly
		HR(m_pDevice->GetRenderTargetData(pReadSurfaceGpu, Output->Memory));
	}
	return S_OK;
}

HRESULT D3D9RenderImpl::CopyBufferToAviSynth(int commandIndex, byte* dst, int dstPitch, IScriptEnvironment* env) {
	InputTexture* ReadSurface = &m_InputTextures[9 + commandIndex];

	D3DLOCKED_RECT srcRect;
	HR(ReadSurface->Memory->LockRect(&srcRect, NULL, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY));
	BYTE* srcPict = (BYTE*)srcRect.pBits;

	env->BitBlt(dst, dstPitch, srcPict, srcRect.Pitch, ReadSurface->Width * m_OutputPrecision, ReadSurface->Height);

	return ReadSurface->Memory->UnlockRect();
}

HRESULT D3D9RenderImpl::InitPixelShader(CommandStruct* cmd, IScriptEnvironment* env) {
	ShaderItem* Shader = &m_Shaders[cmd->CommandIndex];
	CComPtr<ID3DXBuffer> code;
	unsigned char* ShaderBuf = NULL;
	DWORD* CodeBuffer = NULL;

	if (cmd->ShaderModel == NULL || cmd->ShaderModel[0] == '\0') {
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
