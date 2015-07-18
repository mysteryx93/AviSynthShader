#include "Shader.h"


// http://gamedev.stackexchange.com/questions/13435/loading-and-using-an-hlsl-shader

Shader::Shader(PClip _child, const char* _path, int _precision, IScriptEnvironment* env) :
GenericVideoFilter(_child), path(_path), precision(_precision), BUFFERSIZE(4096) {

	if (vi.IsPlanar() || !vi.IsRGB32())
		env->ThrowError("Shader: Source must be float-precision RGB");
	if (_precision != 1 && _precision != 2 && _precision != 4)
		env->ThrowError("Shader: Precision must be 1, 2 or 4");
	if (path && path[0] == '\0')
		env->ThrowError("Shader: path to a compiled shader must be specified");

	dummyHWND = CreateWindowA("STATIC", "dummy", NULL, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
	if (FAILED(render.Initialize(dummyHWND, vi.width, vi.height, 4)))
		env->ThrowError("Shader: Initialize failed.");
	
	unsigned char* ShaderBuf = ReadBinaryFile(_path);
	if (ShaderBuf == NULL)
		env->ThrowError("Shader: Cannot open shader specified by path");

	//if (FAILED(render.SetPixelShader((DWORD*)ShaderBuf)))
	//	env->ThrowError("Shader: Failed to load pixel shader");
	LPSTR errorMsg = NULL;
	HRESULT hr = render.SetPixelShader("C:\\GitHub\\AviSynthShader\\Shaders\\SampleShader.hlsl", "main", "ps_2_0", &errorMsg);
	free(ShaderBuf);
}

Shader::~Shader() {
	DestroyWindow(dummyHWND);
}

unsigned char* Shader::ReadBinaryFile(const char* filePath) {
	FILE *fl = fopen(filePath, "rb");
	fseek(fl, 0, SEEK_END);
	long len = ftell(fl);
	unsigned char *ret = (unsigned char*)malloc(len);
	fseek(fl, 0, SEEK_SET);
	fread(ret, 1, len, fl);
	fclose(fl);
	return ret;
}

PVideoFrame __stdcall Shader::GetFrame(int n, IScriptEnvironment* env) {
	PVideoFrame src = child->GetFrame(n, env);

	PVideoFrame dst = env->NewVideoFrame(vi);
	if FAILED(render.ProcessFrame(src->GetReadPtr(), src->GetPitch(), dst->GetWritePtr(), dst->GetPitch()))
		env->ThrowError("Shader: ProcessFrame failed.");

	return dst;
}