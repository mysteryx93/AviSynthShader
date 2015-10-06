#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <cstdio>		//needed by OutputDebugString()
#include "avisynth.h"
#include "D3D9RenderImpl.h"


class Shader : public GenericVideoFilter {
public:
	Shader(PClip _child, const char* _path, const char* _entryPoint, const char* _shaderModel, int _precision,
		const char* _param1, const char* _param2, const char* _param3, const char* _param4, const char* _param5, const char* _param6, const char* _param7, const char* _param8, const char* _param9, 
		PClip _clip1, PClip _clip2, PClip _clip3, PClip _clip4, int _width, int _height, IScriptEnvironment* env);
	~Shader();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
private:
	void InitializeDevice(IScriptEnvironment* env);
	void CreateInputClip(int index, PClip clip);
	unsigned char* ReadBinaryFile(const char* filePath);
	void ParseParam(const char* param, IScriptEnvironment* env);
	bool SetParam(char* param);
	void CopyInputClip(int index, PClip clip, int n, IScriptEnvironment* env);
	const char* path;
	const int precision;
	const char* entryPoint;
	const char* shaderModel;
	const char *param1, *param2, *param3, *param4, *param5, *param6, *param7, *param8, *param9;
	PClip clip1, clip2, clip3, clip4;
	HWND dummyHWND;
	D3D9RenderImpl* render;
	VideoInfo destVI;
};