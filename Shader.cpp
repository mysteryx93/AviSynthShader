#include "Shader.h"

Shader::Shader(PClip _child, IScriptEnvironment* env) :
GenericVideoFilter(_child) {
	if (vi.IsPlanar() || !vi.IsRGB32())
		env->ThrowError("Source must be float-precision RGB");

	InitDirectX();
}

void Shader::InitDirectX() {
	dx = Direct3DCreate9(D3D_SDK_VERSION);

	D3DDISPLAYMODE dxMode;
	dx->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &dxMode);
	D3DPRESENT_PARAMETERS dxSettings;
	ZeroMemory(&dxSettings, sizeof(dxSettings));
	dxSettings.Windowed = TRUE;
	dxSettings.SwapEffect = D3DSWAPEFFECT_DISCARD;
	dxSettings.BackBufferFormat = dxMode.Format;
	dxSettings.EnableAutoDepthStencil = TRUE;
	dxSettings.AutoDepthStencilFormat = D3DFMT_D16;
	dxSettings.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	dx->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, NULL, NULL, &dxSettings, &dxDevice);
}

Shader::~Shader() {
	if (dxDevice != NULL) {
		dxDevice->Release();
		dxDevice = NULL;
	}
	if (dx != NULL) {
		dx->Release();
		dx = NULL;
	}
}


PVideoFrame __stdcall Shader::GetFrame(int n, IScriptEnvironment* env) {
	PVideoFrame src = child->GetFrame(n, env);

	// Copy data to texture buffer
	unsigned char* textureBuffer = (unsigned char*)malloc(vi.width * vi.height * 16);
	CopyToTexture(src->GetReadPtr(), src->GetPitch(), textureBuffer);

	// Create texture
	LPDIRECT3DTEXTURE9 dxTexture = NULL;
	HANDLE* dxTextureHandle = (HANDLE*)textureBuffer;
	dxDevice->CreateTexture(vi.width, vi.height, 1, 0, D3DFMT_A32B32G32R32F, D3DPOOL_SYSTEMMEM, &dxTexture, dxTextureHandle);

	// Create shader
	const DWORD* data = NULL; // loadFile("precompiledShader.ext");
	IDirect3DPixelShader9 *ps = NULL;
	dxDevice->CreatePixelShader(data, &ps);


	// Copy data back to frame
	CopyFromTexture(textureBuffer, src->GetWritePtr(), src->GetPitch());

	// Release texture
	dxTexture->Release();
	dxTexture = NULL;
	free(textureBuffer);
	return src;
}

void Shader::CopyToTexture(const byte* src, int srcPitch, unsigned char* dst) {
	for (int i = 0; i < vi.height; i++) {
		memcpy(dst, src, vi.width * pixelSize);
		src += srcPitch;
		dst += vi.width * pixelSize;
	}
}

void Shader::CopyFromTexture(const byte* src, unsigned char* dst, int dstPitch) {
	for (int i = 0; i < vi.height; i++) {
		memcpy(dst, src, vi.width * pixelSize);
		src += vi.width * pixelSize;
		dst += dstPitch;
	}
}
