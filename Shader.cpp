#include "Shader.h"


// http://gamedev.stackexchange.com/questions/13435/loading-and-using-an-hlsl-shader

Shader::Shader(PClip _child, const char* _path, int _precision, IScriptEnvironment* env) :
GenericVideoFilter(_child), path(_path), precision(_precision) {
	if (vi.IsPlanar() || !vi.IsRGB32())
		env->ThrowError("Shader: Source must be float-precision RGB");
	if (_precision != 1 && _precision != 2 && _precision != 4)
		env->ThrowError("Shader: Precision must be 1, 2 or 4");
	if (path && path[0] == '\0')
		env->ThrowError("Shader: path to a compiled shader must be specified");

	FILE* ShaderFile = fopen(_path, "r");
	if (ShaderFile == NULL)
		env->ThrowError("Shader: Cannot open shader specified by path");

	dummyHWND = CreateWindowA("STATIC", "dummy", NULL, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
	if (FAILED(render.Initialize(dummyHWND)))
		env->ThrowError("Shader: Initialize failed.");
	if (FAILED(render.CreateVideoSurface(vi.width, vi.height, 4)))
		env->ThrowError("Shader: CreateVideoSurface failed.");
	
	// render.SetPixelShader((DWORD*)ShaderFile);
	// LPSTR errorMsg = NULL;
	// render.SetPixelShader("Shaders\SampleShader.hlsl", "main", "ps_2_0", &errorMsg);
	fclose(ShaderFile);
}

Shader::~Shader() {
	DestroyWindow(dummyHWND);
}

PVideoFrame __stdcall Shader::GetFrame(int n, IScriptEnvironment* env) {
	PVideoFrame src = child->GetFrame(n, env);

	PVideoFrame dst = env->NewVideoFrame(vi);
	if FAILED(render.ProcessFrame(src->GetReadPtr(), src->GetPitch(), dst->GetWritePtr(), dst->GetPitch()))
		env->ThrowError("Shader: ProcessFrame failed.");

	return dst;
}