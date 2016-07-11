#pragma once

#include "d3d9.h"
#include "atlbase.h"
#include "D3D9Macros.h"
#include "avisynth.h"
#include <windows.h>
#include <mutex>
#include <vector>
#include <algorithm>
#include "CommandStruct.h"
#include "MemoryPool.h"
#include "TextureList.h"
#include "Dither.h"
#include "D3D9Include.h"

struct RenderTargetMatrix {
	int Width, Height;
	D3DXMATRIX MatrixOrtho;
	CComPtr<IDirect3DVertexBuffer9> VertexBuffer;
};

struct ShaderItem {
	CComPtr<IDirect3DPixelShader9> Shader;
	CComPtr<ID3DXConstantTable> ConstantTable;
};

class D3D9RenderImpl {
public:
	D3D9RenderImpl();
	~D3D9RenderImpl();

	HRESULT Initialize(HWND hDisplayWindow, int clipPrecision[9], int precision, int outputPrecision, bool planarOut, bool isMT, IScriptEnvironment* env);
	HRESULT CreateTexture(int clipIndex, int width, int height, bool isInput, bool IsPlanar, bool isLast, int shaderPrecision, InputTexture* outTexture);
	HRESULT CopyBuffer(std::vector<InputTexture*>* textureList, InputTexture* src, CommandStruct* cmd);
	HRESULT ProcessFrame(std::vector<InputTexture*>* textureList, CommandStruct* cmd, int width, int height, bool isLast, int planeOut, IScriptEnvironment* env);
	HRESULT InitPixelShader(CommandStruct* cmd, int planeOut, IScriptEnvironment* env);
	HRESULT SetDefaults(LPD3DXCONSTANTTABLE table);
	HRESULT SetPixelShaderConstant(int index, const ParamStruct* param);
	HRESULT CopyDitherMatrix(std::vector<InputTexture*>* textureList, int outputIndex);
	HRESULT ResetSamplerState();
	static const int maxClips = 9;
	ShaderItem m_Shaders[80] = { 0 };
	std::mutex mutex_ProcessCommand;
	MemoryPool* m_Pool = nullptr;
	InputTexture* m_DitherMatrix;

private:
	bool StringEndsWith(const char * str, const char * suffix);
	HRESULT CreateDevice(IDirect3DDevice9Ex** device, HWND hDisplayWindow, bool isMT);
	HRESULT GetRenderTargetMatrix(int width, int height, RenderTargetMatrix** target);
	HRESULT CreateScene(std::vector<InputTexture*>* textureList, CommandStruct* cmd, bool isLast, int planeOut, IScriptEnvironment* env);
	HRESULT CopyFromRenderTarget(std::vector<InputTexture*>* textureList, CommandStruct* cmd, int width, int height, bool isLast, int planeOut, IScriptEnvironment* env);
	HRESULT PrepareReadTarget(std::vector<InputTexture*>* textureList, CommandStruct* cmd, int width, int height, int planeOut, bool isLast, bool isPlanar, InputTexture** dst);
	HRESULT SetRenderTarget(int width, int height, D3DFORMAT format, IScriptEnvironment* env);
	HRESULT ClearRenderTarget();
	HRESULT GetPresentParams(D3DPRESENT_PARAMETERS* params, HWND hDisplayWindow);

	CComPtr<IDirect3D9Ex>           m_pD3D9;
	CComPtr<IDirect3DDevice9Ex>     m_pDevice;
	PooledTexture* m_pCurrentRenderTarget = nullptr;
	std::vector<RenderTargetMatrix*> m_RenderTargetMatrixCache;
	std::mutex mutex_InitPixelShader;

	int m_Precision;
	int m_ClipPrecision[9];
	int m_OutputPrecision;
	bool m_PlanarOut;
};