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

struct RenderTargetMatrix {
	int Width, Height;
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

	HRESULT Initialize(HWND hDisplayWindow, int clipPrecision[9], int precision, int outputPrecision, bool planarOut, bool isMT);
	HRESULT CreateTexture(int clipIndex, int width, int height, bool isInput, bool IsPlanar, bool isLast, int shaderPrecision, InputTexture* outTexture);
	HRESULT CopyBuffer(std::vector<InputTexture*>* textureList, InputTexture* src, int outputIndex, IScriptEnvironment* env);
	HRESULT ProcessFrame(std::vector<InputTexture*>* textureList, CommandStruct* cmd, int width, int height, bool isLast, int planeOut, IScriptEnvironment* env);
	HRESULT InitPixelShader(CommandStruct* cmd, int planeOut, IScriptEnvironment* env);
	HRESULT SetDefaults(LPD3DXCONSTANTTABLE table);
	HRESULT SetPixelShaderConstant(int index, const ParamStruct* param);
	static const int maxClips = 9;
	ShaderItem m_Shaders[80] = { 0 };
	std::mutex mutex_ProcessFrame;
	MemoryPool* m_Pool = NULL;

private:
	unsigned char* ReadBinaryFile(const char* filePath);
	bool StringEndsWith(const char * str, const char * suffix);
	void GetDefaultPath(char* outPath, int maxSize, const char* filePath);
	static void StaticFunction() {}; // needed by GetDefaultPath
	HRESULT CreateDevice(IDirect3DDevice9Ex** device, HWND hDisplayWindow, bool isMT);
	HRESULT GetRenderTargetMatrix(int width, int height, RenderTargetMatrix** target);
	HRESULT CreateScene(std::vector<InputTexture*>* textureList, CommandStruct* cmd, int planeOut, IScriptEnvironment* env);
	HRESULT CreateSurface(int width, int height, bool renderTarget, D3DFORMAT format, IDirect3DTexture9 **texture, IDirect3DSurface9 **surface);
	HRESULT CopyFromRenderTarget(std::vector<InputTexture*>* textureList, CommandStruct* cmd, int width, int height, bool isLast, int planeOut, IScriptEnvironment* env);
	HRESULT PrepareReadTarget(std::vector<InputTexture*>* textureList, int outputIndex, int width, int height, int planeOut, bool isLast, InputTexture** dst);
	HRESULT SetRenderTarget(int width, int height, D3DFORMAT format, IScriptEnvironment* env);
	HRESULT ClearRenderTarget();
	HRESULT GetPresentParams(D3DPRESENT_PARAMETERS* params, HWND hDisplayWindow);
	HRESULT CopyBufferToAviSynthInternal(IDirect3DSurface9* surface, byte* dst, int dstPitch, int rowSize, int height, IScriptEnvironment* env);

	CComPtr<IDirect3D9Ex>           m_pD3D9;
	CComPtr<IDirect3DDevice9Ex>     m_pDevice;
	PooledTexture* m_pCurrentRenderTarget = NULL;
	std::vector<RenderTargetMatrix*> m_MatrixCache;
	std::mutex mutex_InitPixelShader;

	int m_Precision;
	int m_ClipPrecision[9];
	int m_OutputPrecision;
	bool m_PlanarOut;
};