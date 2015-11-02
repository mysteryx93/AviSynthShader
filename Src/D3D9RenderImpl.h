#pragma once

#include "d3d9.h"
#include "atlbase.h"
#include "D3D9Macros.h"
#include "avisynth.h"
#include <windows.h>
#include "CommandStruct.h"
#include <list>
#include <vector>

struct InputTexture {
	int Width, Height;
	bool IsTaken;
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
	const char* Path;
	const char* Param[9];
	CComPtr<IDirect3DPixelShader9> Shader;
	CComPtr<ID3DXConstantTable> ConstantTable;
};

class D3D9RenderImpl
{
public:
	D3D9RenderImpl();
	~D3D9RenderImpl();

	HRESULT Initialize(HWND hDisplayWindow, int precision, IScriptEnvironment* env);
	void ResetInputTextures();
	HRESULT FindInputTexture(int width, int height, InputTexture*& output);
	HRESULT CreateInputTexture(int width, int height, InputTexture*& outTexture);
	HRESULT CopyAviSynthToBuffer(const byte* src, int srcPitch, InputTexture* dst);
	HRESULT ProcessFrame(CommandStruct* cmd, InputTexture* inputList[9], CommandStruct* previousCmd);
	HRESULT FlushRenderTarget(CommandStruct* cmd);

	HRESULT SetPixelShader(CommandStruct* cmd);

	std::list<InputTexture*> m_InputTextures;
	std::vector<ShaderItem*> m_Shaders;
	ShaderItem* m_pCurrentShader = NULL;

private:
	static HWND staticDummyWindow;
	static CComPtr<IDirect3DDevice9Ex> staticDevice;

	unsigned char* ReadBinaryFile(const char* filePath);
	HRESULT CreateDevice(IDirect3DDevice9Ex** device, HWND hDisplayWindow);
	HRESULT SetupMatrices(RenderTarget* target, float width, float height);
	HRESULT CreateScene(CommandStruct* cmd, InputTexture* inputList[9]);
	HRESULT CopyFromRenderTarget(byte* dst, int dstPitch);
	HRESULT SetRenderTarget(int width, int height);
	HRESULT GetPresentParams(D3DPRESENT_PARAMETERS* params, HWND hDisplayWindow);

	HRESULT InitPixelShader(CommandStruct* cmd, ShaderItem*& outShader);
	bool CompareShader(CommandStruct* cmd, ShaderItem* shader);
	void ParseParam(LPD3DXCONSTANTTABLE table, const char* param, IScriptEnvironment* env);
	bool SetParam(LPD3DXCONSTANTTABLE table, char* param);
	HRESULT SetPixelShaderIntConstant(LPD3DXCONSTANTTABLE table, LPCSTR name, int value);
	HRESULT SetPixelShaderFloatConstant(LPD3DXCONSTANTTABLE table, LPCSTR name, float value);
	HRESULT SetPixelShaderBoolConstant(LPD3DXCONSTANTTABLE table, LPCSTR name, bool value);
	HRESULT SetPixelShaderConstant(LPD3DXCONSTANTTABLE table, LPCSTR name, LPVOID value, UINT size);
	HRESULT SetPixelShaderMatrix(LPD3DXCONSTANTTABLE table, D3DXMATRIX* matrix, LPCSTR name);
	HRESULT SetPixelShaderVector(LPD3DXCONSTANTTABLE table, LPCSTR name, D3DXVECTOR4* vector);

	CComPtr<IDirect3D9Ex>           m_pD3D9;
	CComPtr<IDirect3DDevice9Ex>     m_pDevice;
	std::list<RenderTarget*> m_RenderTargets;
	RenderTarget* m_pCurrentRenderTarget = NULL;

	int m_precision;
	D3DFORMAT m_format;
	D3DDISPLAYMODE m_displayMode;
	D3DPRESENT_PARAMETERS m_presentParams;
	IScriptEnvironment* m_env;
};