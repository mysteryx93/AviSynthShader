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
	CComPtr<IDirect3DTexture9> TextureY;
	CComPtr<IDirect3DSurface9> SurfaceY;
	CComPtr<IDirect3DTexture9> TextureU;
	CComPtr<IDirect3DSurface9> SurfaceU;
	CComPtr<IDirect3DTexture9> TextureV;
	CComPtr<IDirect3DSurface9> SurfaceV;
};

struct RenderTarget {
	int Width, Height;
	D3DFORMAT Format;
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

	HRESULT Initialize(HWND hDisplayWindow, int clipPrecision[9], int precision, int outputPrecision, bool planarOut);
	HRESULT CreateInputTexture(int index, int clipIndex, int width, int height, bool isInput, bool IsPlanar, bool isLast, int shaderPrecision);
	HRESULT CopyBuffer(InputTexture* srcSurface, int commandIndex, int outputIndex, IScriptEnvironment* env);
	HRESULT CopyAviSynthToBuffer(const byte* src, int srcPitch, int index, int width, int height, IScriptEnvironment* env);
	HRESULT CopyAviSynthToPlanarBuffer(PVideoFrame frame, int index, int width, int height, IScriptEnvironment* env);
	HRESULT CopyBufferToAviSynth(int commandIndex, byte* dst, int dstPitch, IScriptEnvironment* env);
	HRESULT CopyBufferToAviSynthPlanar(int commandIndex, byte* dstY, byte* dstU, byte* dstV, int dstPitch, IScriptEnvironment* env);
	HRESULT ProcessFrame(CommandStruct* cmd, int width, int height, bool isLast, int planeOut, IScriptEnvironment* env);
	InputTexture* FindTextureByClipIndex(int clipIndex, IScriptEnvironment* env);
	void ResetTextureClipIndex();

	HRESULT InitPixelShader(CommandStruct* cmd, int planeOut, IScriptEnvironment* env);
	HRESULT SetDefaults(LPD3DXCONSTANTTABLE table);
	HRESULT SetPixelShaderConstant(int index, const ParamStruct* param);
	static const int maxTextures = 100;
	static const int maxClips = 9;
	InputTexture m_InputTextures[maxTextures];
	ShaderItem m_Shaders[maxTextures] = { 0 };

private:
	HRESULT ApplyPrecision(int precision, int &precisionSizeOut, D3DFORMAT &formatOut);
	D3DFORMAT D3D9RenderImpl::GetD3DFormat(int precision);
	unsigned char* ReadBinaryFile(const char* filePath);
	bool StringEndsWith(const char * str, const char * suffix);
	void GetDefaultPath(char* outPath, int maxSize, const char* filePath);
	static void StaticFunction() {}; // needed by GetDefaultPath
	HRESULT CreateDevice(IDirect3DDevice9Ex** device, HWND hDisplayWindow);
	HRESULT SetupMatrices(RenderTarget* target, float width, float height);
	HRESULT CreateScene(CommandStruct* cmd, int planeOut, IScriptEnvironment* env);
	HRESULT CreateSurface(int width, int height, bool renderTarget, D3DFORMAT format, IDirect3DTexture9 **texture, IDirect3DSurface9 **surface);
	HRESULT CopyFromRenderTarget(int dstIndex, int outputIndex, int width, int height, bool isLast, int planeOut, IScriptEnvironment* env);
	HRESULT SetRenderTarget(int width, int height, D3DFORMAT format, IScriptEnvironment* env);
	HRESULT GetPresentParams(D3DPRESENT_PARAMETERS* params, HWND hDisplayWindow);
	HRESULT CopyBufferToAviSynthInternal(IDirect3DSurface9* surface, byte* dst, int dstPitch, int rowSize, int height, IScriptEnvironment* env);

	CComPtr<IDirect3D9Ex>           m_pD3D9;
	CComPtr<IDirect3DDevice9Ex>     m_pDevice;
	RenderTarget m_RenderTargets[maxTextures];
	RenderTarget* m_pCurrentRenderTarget = NULL;

	int m_Precision;
	int m_PrecisionSize;
	int m_ClipPrecision[9];
	int m_ClipPrecisionSize[9];
	int m_OutputPrecision;
	int m_OutputPrecisionSize;
	bool m_PlanarOut;
	D3DFORMAT m_Format;
	D3DFORMAT m_ClipFormat[9];
	D3DFORMAT m_OutputFormat;
	D3DDISPLAYMODE m_displayMode;
	D3DPRESENT_PARAMETERS m_presentParams;
	IScriptEnvironment* m_env;
};