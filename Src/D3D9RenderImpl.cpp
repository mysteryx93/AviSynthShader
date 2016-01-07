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
// CPU    D D D                   S
// GPU    R R R             R  R  R
//
// m_RenderTargets contains one R texture per output resolution. The Render Target then gets 
// copied into the next available index. Command1 outputs to RenderTarget[0] and then
// to index 9, Command2 outputs to index 10, Command3 outputs to index 11, etc.
// Only the final output needs to be copied from the GPU back onto the CPU, 
// requiring a SYSTEMMEM texture.

#include "D3D9RenderImpl.h"

D3D9RenderImpl::D3D9RenderImpl() {
	ZeroMemory(m_InputTextures, sizeof(InputTexture) * maxTextures);
	ZeroMemory(m_RenderTargets, sizeof(RenderTarget) * maxTextures);
	ZeroMemory(m_Shaders, sizeof(ShaderItem) * maxTextures);
}

D3D9RenderImpl::~D3D9RenderImpl(void) {
	for (int i = 0; i < maxTextures; i++) {
		//SafeRelease(m_InputTextures[0].Surface);
		SafeRelease(m_InputTextures[0].Texture);
		SafeRelease(m_InputTextures[0].Memory);
	}
}

HRESULT D3D9RenderImpl::Initialize(int clipPrecision[9], int precision, int outputPrecision) {
	HR(ApplyPrecision(precision, m_Precision, m_Format));
	for (int i = 0; i < 9; i++) {
		HR(ApplyPrecision(clipPrecision[i], m_ClipPrecision[i], m_ClipFormat[i]));
	}
	HR(ApplyPrecision(outputPrecision, m_OutputPrecision, m_OutputFormat));
	HR(CreateDevice());
	return S_OK;
}

// Applies the video format corresponding to specified pixel precision.
HRESULT D3D9RenderImpl::ApplyPrecision(int precision, int &precisionOut, DXGI_FORMAT &formatOut) {
	if (precision == 1)
		formatOut = DXGI_FORMAT_R8G8B8A8_UINT;
	else if (precision == 2)
		formatOut = DXGI_FORMAT_R16G16B16A16_UINT;
	else if (precision == 3)
		formatOut = DXGI_FORMAT_R16G16B16A16_FLOAT;
	else
		return E_FAIL;

	if (precision == 3)
		precisionOut = 2;
	else
		precisionOut = precision;

	return S_OK;
}

HRESULT D3D9RenderImpl::CreateDevice() {
	D3D_FEATURE_LEVEL fl[] = {
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0
	};

	HR(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_DEBUG, fl, ARRAYSIZE(fl), D3D11_SDK_VERSION, &m_pDevice, NULL, &m_pContext));
	return S_OK;
}

HRESULT D3D9RenderImpl::SetRenderTarget(int width, int height, DXGI_FORMAT format, IScriptEnvironment* env)
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

		D3D11_TEXTURE2D_DESC pDesc;
		ZeroMemory(&pDesc, sizeof(pDesc));
		pDesc.Width = width;
		pDesc.Height = height;
		pDesc.MipLevels = 1;
		pDesc.ArraySize = 1;
		pDesc.Format = format;
		pDesc.Usage = D3D11_USAGE_DEFAULT;
		
		HR(m_pDevice->CreateTexture2D(&pDesc, NULL, &Target->Texture));
		//HR(m_pDevice->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, format, D3DPOOL_DEFAULT, &Target->Texture, NULL));
		//HR(Target->Texture->GetSurfaceLevel(0, &Target->Surface));
		//HR(m_pDevice->CreateOffscreenPlainSurface(width, height, format, D3DPOOL_SYSTEMMEM, &Target->Memory, NULL));
	}

	// Set render target.
	m_pCurrentRenderTarget = Target;
	return S_OK;
}

HRESULT D3D9RenderImpl::CreateInputTexture(int index, int clipIndex, int width, int height, bool memoryTexture, bool isSystemMemory) {
	if (index < 0 || index >= maxTextures)
		return E_FAIL;

	InputTexture* Obj = &m_InputTextures[index];
	Obj->ClipIndex = clipIndex;
	Obj->Width = width;
	Obj->Height = height;

	D3D11_TEXTURE2D_DESC pDesc;
	ZeroMemory(&pDesc, sizeof(pDesc));
	pDesc.Width = width;
	pDesc.Height = height;
	pDesc.MipLevels = 1;
	pDesc.ArraySize = 1;
	pDesc.SampleDesc.Count = 1;

	if (memoryTexture && !isSystemMemory) {
		pDesc.Format = m_ClipFormat[index];
		pDesc.Usage = D3D11_USAGE_DYNAMIC;
		pDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		pDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		m_pDevice->CreateTexture2D(&pDesc, NULL, &Obj->Memory);
	}
	else if (isSystemMemory) {
		pDesc.Format = m_OutputFormat;
		pDesc.Usage = D3D11_USAGE_STAGING;
		pDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		m_pDevice->CreateTexture2D(&pDesc, NULL, &Obj->Memory);
	}
	if (!isSystemMemory) {
		pDesc.Format = m_Format;
		pDesc.Usage = D3D11_USAGE_DEFAULT;
		pDesc.CPUAccessFlags = 0;
		m_pDevice->CreateTexture2D(&pDesc, NULL, &Obj->Texture);
		m_pDevice->CreateShaderResourceView(Obj->Texture, NULL, &Obj->View);
	}

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
	HR(SetRenderTarget(width, height, isLast ? m_OutputFormat : m_Format, env));
	HR(CreateScene(cmd, env));
	//HR(m_pDevice->Present(NULL, NULL, NULL, NULL));
	return CopyFromRenderTarget(9 + cmd->CommandIndex, cmd->OutputIndex, width, height);
}

HRESULT D3D9RenderImpl::CreateScene(CommandStruct* cmd, IScriptEnvironment* env)
{
	m_pContext->CSSetShader(m_Shaders[cmd->CommandIndex].Shader, NULL, 0);

	//HR(m_pDevice->Clear(D3DADAPTER_DEFAULT, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0));

	// Set input clips.
	InputTexture* Input;
	for (int i = 0; i < 9; i++) {
		if (cmd->ClipIndex[i] > 0) {
			Input = FindTextureByClipIndex(cmd->ClipIndex[i], env);
			if (Input != NULL) {
				m_pContext->CSSetShaderResources(i, 1, &Input->View);
			}
			else
				env->ThrowError("Shader: Invalid clip index.");
		}
	}
	return S_OK;
}

HRESULT D3D9RenderImpl::CopyBuffer(InputTexture* src, int commandIndex, int outputIndex, IScriptEnvironment* env) {
	InputTexture* dst = &m_InputTextures[9 + commandIndex];
	dst->ClipIndex = outputIndex;

	//HR(m_pDevice->ColorFill(dstSurface->Surface, NULL, D3DCOLOR_ARGB(0xFF, 0, 0, 0)));
	m_pContext->CopyResource(src->Texture, dst->Texture);
	return S_OK;
}

HRESULT D3D9RenderImpl::CopyAviSynthToBuffer(const byte* src, int srcPitch, int index, int width, int height, IScriptEnvironment* env) {
	// Copies source frame into main surface buffer, or into additional input textures
	CComPtr<ID3D11Texture2D> dst = m_InputTextures[index].Memory;
	if (index < 0 || index >= maxTextures)
		return E_FAIL;

	D3D11_MAPPED_SUBRESOURCE dstRect;
	HR(m_pContext->Map(dst, 0, D3D11_MAP_WRITE_DISCARD, 0, &dstRect));
	env->BitBlt((byte*)dstRect.pData, dstRect.RowPitch, src, srcPitch, width * m_ClipPrecision[index] * 4, height);
	m_pContext->Unmap(dst, 0);

	// Copy to GPU
	//HR(m_pDevice->ColorFill(m_InputTextures[index].Surface, NULL, D3DCOLOR_ARGB(0xFF, 0, 0, 0)));
	m_pContext->CopyResource(m_InputTextures[index].Memory, m_InputTextures[index].Texture);
	return S_OK;
}

HRESULT D3D9RenderImpl::CopyFromRenderTarget(int dstIndex, int outputIndex, int width, int height)
{
	if (dstIndex < 0 || dstIndex >= maxTextures)
		return E_FAIL;

	//InputTexture* Output = &m_InputTextures[dstIndex];
	//CComPtr<ID3D11Texture2D> pReadSurfaceGpu;
	//HR(m_pDevice->GetRenderTarget(0, &pReadSurfaceGpu));
	//Output->ClipIndex = outputIndex;
	//if (Output->Memory == NULL) {
	//	//HR(m_pDevice->ColorFill(Output->Surface, NULL, D3DCOLOR_ARGB(0xFF, 0, 0, 0)));
	//	HR(m_pDevice->StretchRect(pReadSurfaceGpu, NULL, Output->Surface, NULL, D3DTEXF_POINT));
	//}
	//else {
	//	// If reading last command, copy it back to CPU directly
	//	HR(m_pDevice->GetRenderTargetData(pReadSurfaceGpu, Output->Memory));
	//}
	return S_OK;
}

HRESULT D3D9RenderImpl::CopyBufferToAviSynth(int commandIndex, byte* dst, int dstPitch, IScriptEnvironment* env) {
	InputTexture* src = &m_InputTextures[9 + commandIndex];
	D3D11_MAPPED_SUBRESOURCE srcRect;
	HR(m_pContext->Map(src->Memory, 0, D3D11_MAP_READ, 0, &srcRect));
	env->BitBlt(dst, dstPitch, (byte*)srcRect.pData, srcRect.RowPitch, src->Width * m_OutputPrecision * 4, src->Height);
	m_pContext->Unmap(src->Memory, 0);
	return S_OK;
}

HRESULT D3D9RenderImpl::InitPixelShader(CommandStruct* cmd, IScriptEnvironment* env) {
	ShaderItem* Shader = &m_Shaders[cmd->CommandIndex];
	CComPtr<ID3DBlob> code;
	unsigned char* ShaderBuf = NULL;
	void* CodeBuffer = NULL;
	long BufferLength = 0;

	if (cmd->ShaderModel == NULL || cmd->ShaderModel[0] == '\0') {
		// Precompiled shader
		ShaderBuf = ReadBinaryFile(cmd->Path, &BufferLength);
		if (ShaderBuf == NULL)
			return E_FAIL;
		//HR(D3DXGetShaderConstantTable((DWORD*)ShaderBuf, &Shader->ConstantTable));
		CodeBuffer = (DWORD*)ShaderBuf;
	}
	else {
		CComPtr<ID3DBlob> pBlob = nullptr;
		HR(D3DCompileFromFile(ConvertToLPCWSTR(cmd->Path), NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, cmd->EntryPoint, cmd->ShaderModel, D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlob, NULL));
		CodeBuffer = pBlob->GetBufferPointer();
		BufferLength = pBlob->GetBufferSize();
	}

	HR(m_pDevice->CreateComputeShader(CodeBuffer, BufferLength, NULL, &Shader->Shader));

	if (ShaderBuf != NULL)
		free(ShaderBuf);
	return S_OK;
}

unsigned char* D3D9RenderImpl::ReadBinaryFile(const char* filePath, long* length) {
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
		*length = ftell(fl);
		unsigned char *ret = (unsigned char*)malloc(*length);
		fseek(fl, 0, SEEK_SET);
		fread(ret, 1, *length, fl);
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

wchar_t* D3D9RenderImpl::ConvertToLPCWSTR(const char* charArray)
{
	wchar_t* wString = new wchar_t[4096];
	MultiByteToWideChar(CP_ACP, 0, charArray, -1, wString, 4096);
	return wString;
}

HRESULT D3D9RenderImpl::SetPixelShaderConstant(int index, const ParamStruct* param) {
	//if (param->Type == ParamType::Float) {
	//	HR(m_pDevice->SetPixelShaderConstantF(index, (float*)param->Value, 1));
	//}
	//else if (param->Type == ParamType::Int) {
	//	HR(m_pDevice->SetPixelShaderConstantI(index, (int*)param->Value, 1));
	//}
	//else if (param->Type == ParamType::Bool) {
	//	HR(m_pDevice->SetPixelShaderConstantB(index, (const BOOL*)param->Value, 1));
	//}
	return S_OK;
}
