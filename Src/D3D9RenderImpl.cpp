// Code originally taken from Taygeta Video Presender available here.
// The code has been mostly rewritten for our needs.
// http://www.codeproject.com/Articles/207642/Video-Shadering-with-Direct-D

#include "D3D9RenderImpl.h"

HWND D3D9RenderImpl::staticDummyWindow;
CComPtr<IDirect3DDevice9Ex> D3D9RenderImpl::staticDevice;

D3D9RenderImpl::D3D9RenderImpl() {
}

D3D9RenderImpl::~D3D9RenderImpl(void) {
	// Delete objects created in lists. Associated ressources should be automatically freed with CComPtr.
	for (auto it = m_InputTextures.begin(); it != m_InputTextures.end(); ++it) {
		delete (*it);
	}
	for (auto it = m_RenderTargets.begin(); it != m_RenderTargets.end(); ++it) {
		delete (*it);
	}
	for (auto it = m_Shaders.begin(); it != m_Shaders.end(); ++it) {
		delete *it;
	}
}

HRESULT D3D9RenderImpl::Initialize(HWND hDisplayWindow, int precision, IScriptEnvironment* env) {
	m_precision = precision;
	m_env = env;
	if (precision == 1)
		m_format = D3DFMT_X8R8G8B8;
	else if (precision == 2)
		m_format = D3DFMT_A16B16G16R16F;
	else
		return E_FAIL;

	return CreateDevice(&m_pDevice, hDisplayWindow);
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

HRESULT D3D9RenderImpl::SetRenderTarget(int width, int height)
{
	// Skip if current render target has right dimensions.
	if (m_pCurrentRenderTarget != NULL && m_pCurrentRenderTarget->Width == width && m_pCurrentRenderTarget->Height == height && !m_pCurrentRenderTarget->IsTaken) {
		m_pCurrentRenderTarget->IsTaken = true;
		return S_OK;
	}

	// Find a render target with desired proportions.
	RenderTarget* Target = NULL;
	int i = 0;
	RenderTarget* Item;
	for (auto it = m_RenderTargets.begin(); it != m_RenderTargets.end(); ++it) {
		Item = (*it);
		if (Item->Width == width && Item->Height == height && Item->IsTaken == false)
			Target = Item;
	}

	// Create it if it doesn't yet exist.
	if (Target == NULL) {
		Target = new RenderTarget();
		Target->Width = width;
		Target->Height = height;
		Target->IsTaken = true;
		HR(m_pDevice->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, m_format, D3DPOOL_DEFAULT, &Target->Texture, NULL));
		HR(Target->Texture->GetSurfaceLevel(0, &Target->Surface));
		HR(m_pDevice->CreateOffscreenPlainSurface(width, height, m_format, D3DPOOL_SYSTEMMEM, &Target->Memory, NULL));
		HR(SetupMatrices(Target, width, height));
		m_RenderTargets.push_back(Target);
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

/// Marks all input textures as being unused.
void D3D9RenderImpl::ResetInputTextures() {
	for (auto it = m_InputTextures.begin(); it != m_InputTextures.end(); ++it) {
		(*it)->IsTaken = false;
	}
}

/// Finds an input texture with specified dimensions that isn't yet being used.
HRESULT D3D9RenderImpl::FindInputTexture(int width, int height, InputTexture*& output) {
	InputTexture* Item;
	for (auto it = m_InputTextures.begin(); it != m_InputTextures.end(); ++it) {
		Item = *it;
		if (Item->Width == width && Item->Height == height && !Item->IsTaken) {
			output = Item;
			return S_OK;
		}
	}

	// If no matching texture is found, create it.
	HR(CreateInputTexture(width, height, output));
	m_InputTextures.push_back(output);
	return S_OK;
}

/// Creates a shader input texture.
HRESULT D3D9RenderImpl::CreateInputTexture(int width, int height, InputTexture*& outTexture) {
	outTexture = new InputTexture();
	outTexture->IsTaken = false;
	outTexture->Width = width;
	outTexture->Height = height;

	HR(m_pDevice->CreateOffscreenPlainSurface(width, height, m_format, D3DPOOL_DEFAULT, &outTexture->Memory, NULL));
	HR(m_pDevice->ColorFill(outTexture->Memory, NULL, D3DCOLOR_ARGB(0xFF, 0, 0, 0)));

	HR(m_pDevice->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, m_format, D3DPOOL_DEFAULT, &outTexture->Texture, NULL));
	HR(outTexture->Texture->GetSurfaceLevel(0, &outTexture->Surface));
	return S_OK;
}

HRESULT D3D9RenderImpl::ProcessFrame(CommandStruct* cmd, InputTexture* inputList[9], CommandStruct* previousCmd)
{
	//int Width = cmd->Output->GetRowSize() / m_precision / 4;
	//int Height = cmd->Output->GetHeight();
	HR(m_pDevice->TestCooperativeLevel());
	HR(SetRenderTarget(cmd->Output.Width, cmd->Output.Height));
	HR(CreateScene(cmd, inputList));
	HR(m_pDevice->Present(NULL, NULL, NULL, NULL));

	// It always returns the previously generated frame while we fill in the current frame data.
	if (previousCmd != NULL)
		HR(CopyFromRenderTarget(previousCmd->Output.Buffer, previousCmd->Output.Pitch));
	return S_OK;
}

/// Returns the result of the last frame from the Render Target.
HRESULT D3D9RenderImpl::FlushRenderTarget(CommandStruct* cmd) {
	// Flush the device by creating a dummy scene.
	HR(m_pDevice->Clear(D3DADAPTER_DEFAULT, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0));
	HR(m_pDevice->BeginScene());
	SCENE_HR(m_pDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2), m_pDevice);
	HR(m_pDevice->EndScene());
	HR(m_pDevice->Present(NULL, NULL, NULL, NULL));

	// Get the output frame.
	HR(CopyFromRenderTarget(cmd->Output.Buffer, cmd->Output.Pitch));
	m_pCurrentRenderTarget->IsTaken = false;
	return S_OK;
}

HRESULT D3D9RenderImpl::CreateScene(CommandStruct* cmd, InputTexture* inputList[9])
{
	HR(m_pDevice->Clear(D3DADAPTER_DEFAULT, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0));
	HR(m_pDevice->BeginScene());
	SCENE_HR(m_pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1), m_pDevice);
	SCENE_HR(m_pDevice->SetPixelShader(m_pCurrentShader->Shader), m_pDevice);
	SCENE_HR(m_pDevice->SetStreamSource(0, m_pCurrentRenderTarget->VertexBuffer, 0, sizeof(VERTEX)), m_pDevice);

	// Set input clips.
	InputTexture* Input;
	for (int i = 0; i < 9; i++) {
		if (inputList[i] != NULL)
			SCENE_HR(m_pDevice->SetTexture(i, inputList[i]->Texture), m_pDevice);
	}

	SCENE_HR(m_pDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2), m_pDevice);
	return m_pDevice->EndScene();
}

HRESULT D3D9RenderImpl::CopyAviSynthToBuffer(const byte* src, int srcPitch, InputTexture* dst) {
	D3DLOCKED_RECT d3drect;
	HR(dst->Memory->LockRect(&d3drect, NULL, 0));
	BYTE* pict = (BYTE*)d3drect.pBits;

	m_env->BitBlt(pict, d3drect.Pitch, src, srcPitch, dst->Width * m_precision * 4, dst->Height);

	HR(dst->Memory->UnlockRect());

	// Copy to GPU
	HR(m_pDevice->ColorFill(dst->Surface, NULL, D3DCOLOR_ARGB(0xFF, 0, 0, 0)));
	return (m_pDevice->StretchRect(dst->Memory, NULL, dst->Surface, NULL, D3DTEXF_POINT));
}

HRESULT D3D9RenderImpl::CopyFromRenderTarget(byte* dst, int dstPitch)
{
	CComPtr<IDirect3DSurface9> pReadSurfaceGpu;
	HR(m_pDevice->GetRenderTarget(0, &pReadSurfaceGpu));
	HR(m_pDevice->GetRenderTargetData(pReadSurfaceGpu, m_pCurrentRenderTarget->Memory));

	D3DLOCKED_RECT srcRect;
	HR(m_pCurrentRenderTarget->Memory->LockRect(&srcRect, NULL, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY));
	BYTE* srcPict = (BYTE*)srcRect.pBits;

	m_env->BitBlt(dst, dstPitch, srcPict, srcRect.Pitch, m_pCurrentRenderTarget->Width * m_precision * 4, m_pCurrentRenderTarget->Height);

	HR(m_pCurrentRenderTarget->Memory->UnlockRect());
	m_pCurrentRenderTarget->IsTaken = false;
	return S_OK;
}

HRESULT D3D9RenderImpl::SetPixelShader(CommandStruct* cmd) {
	// If previous shader is still good, keep that one.
	if (CompareShader(cmd, m_pCurrentShader))
		return S_OK;

	// Loop through shaders to find one that matches.
	m_pCurrentShader = NULL;
	for (auto it = m_Shaders.begin(); it != m_Shaders.end(); ++it) {
		if (CompareShader(cmd, *it)) {
			m_pCurrentShader = *it;
			break;
		}
	}

	// If no matching shader exists, create it.
	if (m_pCurrentShader == NULL) {
		HR(InitPixelShader(cmd, m_pCurrentShader));
		//env->ThrowError("Shader: Failed to compile pixel shader");
	}
}

HRESULT D3D9RenderImpl::InitPixelShader(CommandStruct* cmd, ShaderItem*& outShader) {
	ShaderItem* Shader = new ShaderItem();
	CComPtr<ID3DXBuffer> code;
	unsigned char* ShaderBuf = NULL;
	DWORD* CodeBuffer = NULL;

	if (cmd->ShaderModel == NULL || cmd->ShaderModel[0] == '\0') {
		// Precompiled shader
		ShaderBuf = ReadBinaryFile(cmd->Path);
		if (ShaderBuf == NULL)
			return E_FAIL;
		HR(D3DXGetShaderConstantTable((DWORD*)ShaderBuf, &(Shader->ConstantTable)));
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

	// Configure pixel shader
	Shader->Path = cmd->Path;
	for (int i = 0; i < 9; i++) {
		Shader->Param[i] = cmd->Param[i];
		ParseParam(Shader->ConstantTable, cmd->Param[i], NULL);
	}

	m_Shaders.push_back(Shader);
	outShader = Shader;
	return S_OK;
}

/// Returns whether Path and Params are the same for specified command and shader.
bool D3D9RenderImpl::CompareShader(CommandStruct* cmd, ShaderItem* shader) {
	if (cmd == NULL || shader == NULL)
		return false;
	if (strcmp(cmd->Path, shader->Path) != 0)
		return false;
	for (int i = 0; i < 9; i++) {
		if (strcmp(cmd->Param[i], shader->Param[i]) != 0)
			return false;
	}
	return true;
}

void D3D9RenderImpl::ParseParam(LPD3DXCONSTANTTABLE table, const char* param, IScriptEnvironment* env) {
	if (param != NULL && param[0] != '\0') {
		if (!SetParam(table, (char*)param)) {
			// Throw error if failed to set parameter.
			char* ErrorText = "Shader failed to set parameter: ";
			char* FullText;
			FullText = (char*)malloc(strlen(ErrorText) + strlen(param) + 1);
			strcpy(FullText, ErrorText);
			strcat(FullText, param);
			env->ThrowError(FullText);
			free(FullText);
		}
	}
}

// Shader parameters have this format: "ParamName=1f"
// The last character is f for float, i for interet or b for boolean. For boolean, the value is 1 or 0.
// Returns True if parameter was valid and set successfully, otherwise false.
bool D3D9RenderImpl::SetParam(LPD3DXCONSTANTTABLE table, char* param) {
	// Copy string to avoid altering source parameter.
	char* ParamCopy = (char*)malloc(strlen(param) + 1);
	strcpy(ParamCopy, param);

	// Split parameter string into its values and validate data.
	char* Name = strtok(ParamCopy, "=");
	if (Name == NULL)
		return false;
	char* Value = strtok(NULL, "=");
	if (Value == NULL || strlen(Value) < 2)
		return false;
	char Type = Value[strlen(Value) - 1];
	Value[strlen(Value) - 1] = '\0'; // Remove last character from value

									 // Set parameter value.
	if (Type == 'f') {
		char* VectorValue = strtok(Value, ",");
		if (VectorValue == NULL) {
			// Single float value
			float FValue = strtof(Value, NULL);
			if (FAILED(SetPixelShaderFloatConstant(table, Name, FValue)))
				return false;
		}
		else {
			// Vector of 2, 3 or 4 values.
			D3DXVECTOR4 vector = { 0, 0, 0, 0 };
			vector.x = strtof(VectorValue, NULL);
			// Parse 2nd vector value
			char* VectorValue = strtok(NULL, ",");
			if (VectorValue != NULL) {
				vector.y = strtof(VectorValue, NULL);
				// Parse 3rd vector value
				char* VectorValue = strtok(NULL, ",");
				if (VectorValue != NULL) {
					vector.z = strtof(VectorValue, NULL);
					// Parse 4th vector value
					char* VectorValue = strtok(NULL, ",");
					if (VectorValue != NULL) {
						vector.w = strtof(VectorValue, NULL);
					}
				}
			}

			if (FAILED(SetPixelShaderVector(table, Name, &vector)))
				return false;
		}
	}
	else if (Type == 'i') {
		int IValue = atoi(Value);
		if (FAILED(SetPixelShaderIntConstant(table, Name, IValue)))
			return false;
	}
	else if (Type == 'b') {
		bool BValue = Value[0] == '0' ? false : true;
		if (FAILED(SetPixelShaderBoolConstant(table, Name, BValue)))
			return false;
	}
	else // invalid type
		return false;

	// Success
	return true;
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