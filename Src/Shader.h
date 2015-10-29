#pragma once
#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <cstdio>		//needed by OutputDebugString()
#include "avisynth.h"
#include "D3D9RenderImpl.h"

class Shader : public GenericVideoFilter {
public:
	Shader(PClip _child, const char* _path, const char* _entryPoint, const char* _shaderModel, 
		const char* _param1, const char* _param2, const char* _param3, const char* _param4, const char* _param5, const char* _param6, const char* _param7, const char* _param8, const char* _param9, 
		int _clip1, int _clip2, int _clip3, int _clip4, int _clip5, int _clip6, int _clip7, int _clip8, int _clip9, int _output, int _width, int _height, IScriptEnvironment* env);
	~Shader();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
private:
	//void ConfigureShader(IScriptEnvironment* env);
	//unsigned char* ReadBinaryFile(const char* filePath);
	//void ParseParam(D3D9RenderImpl* render, const char* param, IScriptEnvironment* env);
	//bool SetParam(D3D9RenderImpl* render, char* param);
	const char* path;
	const char* entryPoint;
	const char* shaderModel;
	const char *param1, *param2, *param3, *param4, *param5, *param6, *param7, *param8, *param9;
	CommandStruct cmd;
};