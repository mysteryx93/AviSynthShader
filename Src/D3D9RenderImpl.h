#pragma once

#include "d3d9.h"
#include "atlbase.h"
#include "D3D9Macros.h"
#include "avisynth.h"
#include <windows.h>
#include "CommandStruct.h"

struct InputTexture {
	int ClipIndex;
	int Width, Height;
	CComPtr<IDirect3DSurface9> Memory;
	CComPtr<IDirect3DTexture9> Texture;
	CComPtr<IDirect3DSurface9> Surface;
};

struct RenderTarget {
	int Width, Height;
	CComPtr<IDirect3DSurface9> Memory;
	CComPtr<IDirect3DTexture9> Texture;
	CComPtr<IDirect3DSurface9> Surface;
	
	D3DXMATRIX MatrixOrtho;
	CComPtr<IDirect3DVertexBuffer9> VertexBuffer;
};

struct ShaderItem {
	CComPtr<IDirect3DPixelShader9> Shader;
	CComPtr<ID3DXConstantTable> ConstantTable;
};

class D3D9RenderImpl
{
public:
	D3D9RenderImpl();
	~D3D9RenderImpl();

	HRESULT Initialize(HWND hDisplayWindow, int precision);
	HRESULT CreateInputTexture(int index, int clipIndex, int width, int height, bool memoryTexture, bool isSystemMemory);
	HRESULT CopyAviSynthToBuffer(const byte* src, int srcPitch, int index, int width, int height, IScriptEnvironment* env);
	HRESULT CopyBufferToAviSynth(int commandIndex, byte* dst, int dstPitch, IScriptEnvironment* env);
	HRESULT ProcessFrame(CommandStruct* cmd, int width, int height, IScriptEnvironment* env);
	InputTexture* FindTextureByClipIndex(int clipIndex, IScriptEnvironment* env);
	void ResetTextureClipIndex();

	HRESULT InitPixelShader(CommandStruct* cmd, IScriptEnvironment* env);
	HRESULT SetDefaults(LPD3DXCONSTANTTABLE table);
	HRESULT SetPixelShaderConstant(int index, const ParamStruct* param);
	static const int maxTextures = 50;
	InputTexture m_InputTextures[maxTextures];
	ShaderItem m_Shaders[maxTextures];

private:
	unsigned char* ReadBinaryFile(const char* filePath);
	void GetDefaultPath(char* outPath, int maxSize, const char* filePath);
	static void StaticFunction() {}; // needed by GetDefaultPath
	HRESULT CreateDevice(IDirect3DDevice9Ex** device, HWND hDisplayWindow);
	HRESULT SetupMatrices(RenderTarget* target, float width, float height);
	HRESULT CreateScene(CommandStruct* cmd, IScriptEnvironment* env);
	HRESULT CopyFromRenderTarget(int dstIndex, int outputIndex, int width, int height);
	HRESULT SetRenderTarget(int width, int height, IScriptEnvironment* env);
	HRESULT GetPresentParams(D3DPRESENT_PARAMETERS* params, HWND hDisplayWindow);

	CComPtr<IDirect3D9Ex>           m_pD3D9;
	CComPtr<IDirect3DDevice9Ex>     m_pDevice;
	RenderTarget m_RenderTargets[maxTextures];
	RenderTarget* m_pCurrentRenderTarget = NULL;

	int m_precision;
	D3DFORMAT m_format;
	D3DDISPLAYMODE m_displayMode;
	D3DPRESENT_PARAMETERS m_presentParams;
	IScriptEnvironment* m_env;
};