#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <cstdio>		//needed by OutputDebugString()
#include "avisynth.h"	//version 5, AviSynth 2.6 +

class Shader : public GenericVideoFilter {
public:
	Shader(PClip _child, IScriptEnvironment* env);
	~Shader();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
private:
	void InitDirectX();
	void Shader::CopyToTexture(const byte*, int srcPitch, unsigned char* dst);
	void Shader::CopyFromTexture(const byte* src, unsigned char* dst, int dstPitch);
	LPDIRECT3D9 dx = NULL;
	LPDIRECT3DDEVICE9 dxDevice = NULL;
	const int pixelSize = 16;
};