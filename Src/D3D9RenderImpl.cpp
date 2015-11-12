// Code originally taken from Taygeta Video Presender available here.
// The code has been mostly rewritten for our needs.
// http://www.codeproject.com/Articles/207642/Video-Shadering-with-Direct-D

#include "D3D9RenderImpl.h"

HWND D3D9RenderImpl::staticDummyWindow;
CComPtr<IDirect3DDevice9Ex> D3D9RenderImpl::staticDevice;

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

HRESULT D3D9RenderImpl::Initialize(HWND hDisplayWindow, int precision) {
	if (precision == 1)
		m_format = D3DFMT_X8R8G8B8;
	else if (precision == 2)
		m_format = D3DFMT_A16B16G16R16;
	else if (precision == 3)
		m_format = D3DFMT_A16B16G16R16F;
	else
		return E_FAIL;

	m_precision = precision;
	if (precision == 3)
		m_precision = 2;
	else
		m_precision = precision;

	return CreateDevice(&m_pDevice, hDisplayWindow);
}

HRESULT D3D9RenderImpl::CreateDevice(IDirect3DDevice9Ex** device, HWND hDisplayWindow) {
	HR(Direct3DCreate9Ex(D3D_SDK_VERSION, &m_pD3D9));
	if (!m_pD3D9) {
		return E_FAIL;
	}

	D3DCAPS9 deviceCaps;
	HR(m_pD3D9->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &deviceCaps));

	DWORD dwBehaviorFlags = D3DCREATE_DISABLE_PSGP_THREADING;

	if (deviceCaps.VertexProcessingCaps != 0)
		dwBehaviorFlags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
	else
		dwBehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;

	HR(GetPresentParams(&m_presentParams, hDisplayWindow));

	HR(m_pD3D9->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hDisplayWindow, dwBehaviorFlags, &m_presentParams, NULL, device));
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

HRESULT D3D9RenderImpl::SetRenderTarget(int width, int height, IScriptEnvironment* env)
{
	// Skip if current render target has right dimensions.
	if (m_pCurrentRenderTarget != NULL && m_pCurrentRenderTarget->Width == width && m_pCurrentRenderTarget->Height == height)
		return S_OK;

	// Find a render target with desired proportions.
	RenderTarget* Target = NULL;
	int i = 0;
	while (Target == NULL && m_RenderTargets[i].Texture != NULL) {
		if (m_RenderTargets[i].Width == width && m_RenderTargets[i].Height == height)
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
		HR(m_pDevice->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, m_format, D3DPOOL_DEFAULT, &Target->Texture, NULL));
		HR(Target->Texture->GetSurfaceLevel(0, &Target->Surface));
		HR(m_pDevice->CreateOffscreenPlainSurface(width, height, m_format, D3DPOOL_SYSTEMMEM, &Target->Memory, NULL));

		HR(SetupMatrices(Target, width, height));
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

	if (memoryTexture) {
		HR(m_pDevice->CreateOffscreenPlainSurface(width, height, m_format, isSystemMemory ? D3DPOOL_SYSTEMMEM : D3DPOOL_DEFAULT, &Obj->Memory, NULL));
		HR(m_pDevice->ColorFill(Obj->Memory, NULL, D3DCOLOR_ARGB(0xFF, 0, 0, 0)));
	}

	HR(m_pDevice->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, m_format, D3DPOOL_DEFAULT, &Obj->Texture, NULL));
	HR(Obj->Texture->GetSurfaceLevel(0, &m_InputTextures[index].Surface));

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

HRESULT D3D9RenderImpl::ProcessFrame(CommandStruct* cmd, int width, int height, IScriptEnvironment* env)
{
	HR(m_pDevice->TestCooperativeLevel());
	HR(SetRenderTarget(width, height, env));
	HR(CreateScene(cmd, env));
	HR(Present());
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

HRESULT D3D9RenderImpl::Present(void)
{
	HR(m_pDevice->Present(NULL, NULL, NULL, NULL));
	// The RenderTarget returns the previously generated scene for an unknown reason.
	// As a fix, we render another scene so that the previous scene becomes the one returned.
	//HR(m_pDevice->Clear(D3DADAPTER_DEFAULT, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0));
	//HR(m_pDevice->BeginScene());
	//SCENE_HR(m_pDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2), m_pDevice);
	//HR(m_pDevice->EndScene());
	//return m_pDevice->Present(NULL, NULL, NULL, NULL);


	//IDirect3DQuery9* pEventQuery = NULL;
	//HR(m_pDevice->CreateQuery(D3DQUERYTYPE_EVENT, &pEventQuery));
	//HR(pEventQuery->Issue(D3DISSUE_END));
	//while (S_FALSE == pEventQuery->GetData(NULL, 0, D3DGETDATA_FLUSH));
	return S_OK;
}

HRESULT D3D9RenderImpl::CopyAviSynthToBuffer(const byte* src, int srcPitch, int index, int width, int height, IScriptEnvironment* env) {
	// Copies source frame into main surface buffer, or into additional input textures
	CComPtr<IDirect3DSurface9> destSurface = m_InputTextures[index].Memory;
	if (index < 0 || index >= maxTextures)
		return E_FAIL;

	D3DLOCKED_RECT d3drect;
	HR(destSurface->LockRect(&d3drect, NULL, 0));
	BYTE* pict = (BYTE*)d3drect.pBits;

	env->BitBlt(pict, d3drect.Pitch, src, srcPitch, width * m_precision * 4, height);

	HR(destSurface->UnlockRect());

	// Copy to GPU
	HR(m_pDevice->ColorFill(m_InputTextures[index].Surface, NULL, D3DCOLOR_ARGB(0xFF, 0, 0, 0)));
	return (m_pDevice->StretchRect(m_InputTextures[index].Memory, NULL, m_InputTextures[index].Surface, NULL, D3DTEXF_POINT));
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
		HR(m_pDevice->ColorFill(Output->Surface, NULL, D3DCOLOR_ARGB(0xFF, 0, 0, 0)));
		HR(m_pDevice->StretchRect(pReadSurfaceGpu, NULL, Output->Surface, NULL, D3DTEXF_POINT));
	}
	else {
		// If reading last command, copy it back to CPU directly
		HR(m_pDevice->GetRenderTargetData(pReadSurfaceGpu, Output->Memory));
	}
	return S_OK;
}

HRESULT D3D9RenderImpl::CopyBufferToAviSynth(int commandIndex, byte* dst, int dstPitch, IScriptEnvironment* env) {
	InputTexture* ReadSurface = &m_InputTextures[9 + commandIndex];

	D3DLOCKED_RECT srcRect, dstRect;
	HR(ReadSurface->Memory->LockRect(&srcRect, NULL, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY));
	BYTE* srcPict = (BYTE*)srcRect.pBits;

	env->BitBlt(dst, dstPitch, srcPict, srcRect.Pitch, ReadSurface->Width * m_precision * 4, ReadSurface->Height);

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
		// HR(D3DXGetShaderConstantTableEx((DWORD*)ShaderBuf, D3DXCONSTTABLE_LARGEADDRESSAWARE, &Shader->ConstantTable));
		CodeBuffer = (DWORD*)ShaderBuf;
	}
	else {
		// Compile HLSL shader code
		HR(D3DXCompileShaderFromFile(cmd->Path, NULL, NULL, cmd->EntryPoint, cmd->ShaderModel, 0, &code, NULL, &Shader->ConstantTable));
		CodeBuffer = (DWORD*)code->GetBufferPointer();
	}

	HR(m_pDevice->CreatePixelShader(CodeBuffer, &Shader->Shader));

	if (ShaderBuf != NULL)
		free(ShaderBuf);
	return S_OK;
}

unsigned char* D3D9RenderImpl::ReadBinaryFile(const char* filePath) {
	FILE *fl = fopen(filePath, "rb");
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

HRESULT D3D9RenderImpl::SetPixelShaderIntConstant(LPD3DXCONSTANTTABLE table, LPCSTR name, int value)
{
	return table->SetInt(m_pDevice, name, value);
}

HRESULT D3D9RenderImpl::SetPixelShaderFloatConstant(LPD3DXCONSTANTTABLE table, LPCSTR name, float value)
{
	return table->SetFloat(m_pDevice, name, value);
}

HRESULT D3D9RenderImpl::SetPixelShaderBoolConstant(LPD3DXCONSTANTTABLE table, LPCSTR name, bool value)
{
	return table->SetBool(m_pDevice, name, value);
}

HRESULT D3D9RenderImpl::SetPixelShaderConstant(LPD3DXCONSTANTTABLE table, LPCSTR name, LPVOID value, UINT size)
{
	return table->SetValue(m_pDevice, name, value, size);
}

HRESULT D3D9RenderImpl::SetPixelShaderMatrix(LPD3DXCONSTANTTABLE table, D3DXMATRIX* matrix, LPCSTR name)
{
	return table->SetMatrix(m_pDevice, name, matrix);
}

HRESULT D3D9RenderImpl::SetPixelShaderVector(LPD3DXCONSTANTTABLE table, LPCSTR name, D3DXVECTOR4* vector)
{
	return table->SetVector(m_pDevice, name, vector);
}