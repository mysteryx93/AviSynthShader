#pragma once
#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <cstdio>		//needed by OutputDebugString()
#include "avisynth.h"
#include "D3D9RenderImpl.h"

class ExecuteShader : public GenericVideoFilter {
public:
	ExecuteShader(PClip _child, PClip _clip1, PClip _clip2, PClip _clip3, PClip _clip4, PClip _clip5, PClip _clip6, PClip _clip7, PClip _clip8, PClip _clip9, int _precision, IScriptEnvironment* env);
	~ExecuteShader();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
private:
	void InitializeDevice(IScriptEnvironment* env);
	void CreateInputClip(int index, IScriptEnvironment* env);
	void CopyInputClip(int index, int n, IScriptEnvironment* env);
	void ConfigureShader(CommandStruct* cmd, IScriptEnvironment* env);
	
	int precision;
	PClip m_clips[9];
	HWND dummyHWND;
	D3D9RenderImpl* render;
	int srcHeight;
};