#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <cstdio>		//needed by OutputDebugString()
#include "avisynth.h"
#include "VideoPresenter\D3D9RenderImpl.h"


class Shader : public GenericVideoFilter {
public:
	Shader(PClip _child, const char* path, int precision, IScriptEnvironment* env);
	~Shader();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
private:
	HWND dummyHWND;
	D3D9RenderImpl render;
	void Shader::CopyToTexture(const byte*, int srcPitch, unsigned char* dst);
	void Shader::CopyFromTexture(const byte* src, unsigned char* dst, int dstPitch);
	const char* path;
	const int precision;
};