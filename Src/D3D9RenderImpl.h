#pragma once

#include "d3d11.h"
#include "D3Dcompiler.h"
#include "atlbase.h"
#include "D3D9Macros.h"
#include "avisynth.h"
#include <windows.h>
#include "CommandStruct.h"

struct InputTexture {
	int ClipIndex;
	int Width, Height;
	CComPtr<ID3D11Texture2D> Memory;
	CComPtr<ID3D11Texture2D> Texture;
	CComPtr<ID3D11ShaderResourceView> View;
};

struct RenderTarget {
	int Width, Height;
	DXGI_FORMAT Format;
	//CComPtr<ID3D11Texture2D> Memory;
	CComPtr<ID3D11Texture2D> Texture;
	//CComPtr<ID3D11ShaderResourceView> View;
	
	//D3DXMATRIX MatrixOrtho;
	//CComPtr<IDirect3DVertexBuffer9> VertexBuffer;
};

struct ShaderItem {
	CComPtr<ID3D11ComputeShader> Shader;
	CComPtr<ID3D11Buffer> ConstantTable;
};

class D3D9RenderImpl
{
public:
	D3D9RenderImpl();
	~D3D9RenderImpl();

	HRESULT Initialize(int clipPrecision[9], int precision, int outputPrecision);
	HRESULT CreateInputTexture(int index, int clipIndex, int width, int height, bool memoryTexture, bool isSystemMemory);
	HRESULT CopyBuffer(InputTexture* src, int commandIndex, int outputIndex, IScriptEnvironment* env);
	HRESULT CopyAviSynthToBuffer(const byte* src, int srcPitch, int index, int width, int height, IScriptEnvironment* env);
	HRESULT CopyBufferToAviSynth(int commandIndex, byte* dst, int dstPitch, IScriptEnvironment* env);
	HRESULT ProcessFrame(CommandStruct* cmd, int width, int height, bool isLast, IScriptEnvironment* env);
	InputTexture* FindTextureByClipIndex(int clipIndex, IScriptEnvironment* env);
	void ResetTextureClipIndex();

	HRESULT InitPixelShader(CommandStruct* cmd, IScriptEnvironment* env);
	HRESULT SetPixelShaderConstant(int index, const ParamStruct* param);
	static const int maxTextures = 50;
	InputTexture m_InputTextures[maxTextures];
	ShaderItem m_Shaders[maxTextures];

private:
	HRESULT ApplyPrecision(int precision, int &precisionOut, DXGI_FORMAT &formatOut);
	unsigned char* ReadBinaryFile(const char* filePath, long* length);
	void GetDefaultPath(char* outPath, int maxSize, const char* filePath);
	static void StaticFunction() {}; // needed by GetDefaultPath
	HRESULT CreateDevice();
	HRESULT CreateScene(CommandStruct* cmd, IScriptEnvironment* env);
	HRESULT CopyFromRenderTarget(int dstIndex, int outputIndex, int width, int height);
	HRESULT SetRenderTarget(int width, int height, DXGI_FORMAT format, IScriptEnvironment* env);
	wchar_t* ConvertToLPCWSTR(const char* charArray);

	CComPtr<ID3D11Device>			m_pDevice;
	CComPtr<ID3D11DeviceContext>    m_pContext;
	CComPtr<ID3D11ComputeShader>    m_pCS;

	RenderTarget m_RenderTargets[maxTextures];
	RenderTarget* m_pCurrentRenderTarget = NULL;

	int m_Precision;
	int m_ClipPrecision[9];
	int m_OutputPrecision;
	DXGI_FORMAT m_Format;
	DXGI_FORMAT m_ClipFormat[9];
	DXGI_FORMAT m_OutputFormat;
	IScriptEnvironment* m_env;
};