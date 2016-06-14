#pragma once
#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <cstdio>		//needed by OutputDebugString()
#include "avisynth.h"
#include "D3D9RenderImpl.h"
#include <mutex>
#include <vector>
#include <DxErr.h>

// MT_MULTI_INSTANCE with 4 threads gives the best performance.
const bool SUPPORT_MT_NICE_FILTER = false;

class ExecuteShader : public GenericVideoFilter {
public:
	ExecuteShader(PClip _child, PClip _clip1, PClip _clip2, PClip _clip3, PClip _clip4, PClip _clip5, PClip _clip6, PClip _clip7, PClip _clip8, PClip _clip9, int _clipPrecision[9], int _precision, int _outputPrecision, bool _planarOut, IScriptEnvironment* env);
	~ExecuteShader();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
private:
	void GetFrameInternal(D3D9RenderImpl* render, std::vector<InputTexture*>* textureList, int n, bool init, IScriptEnvironment* env);
	void AllocateAndCopyInputTextures(D3D9RenderImpl* render, std::vector<InputTexture*>* list, int n, bool init, IScriptEnvironment* env);
	void CreateInputClip(int index, IScriptEnvironment* env);
	void CopyInputClip(int index, int n, IScriptEnvironment* env);
	void ConfigureShader(CommandStruct* cmd, IScriptEnvironment* env);
	bool SetDefaultParamValue(ParamStruct* p, float value0, float value1, float value2, float value3);
	int m_Precision;
	int m_PrecisionMultiplier;
	int m_OutputPrecision;
	int m_OutputMultiplier;
	PClip m_clips[9];
	int m_ClipPrecision[9];
	int m_ClipMultiplier[9];
	bool m_PlanarOut;
	HWND dummyHWND;
	D3D9RenderImpl* render1;
	D3D9RenderImpl* render2;
	int m_IterateDevice = 0;
	std::mutex mutex_IterateDevice;
	bool isMT;
	int srcHeight;
};