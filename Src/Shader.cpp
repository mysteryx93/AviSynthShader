#include "Shader.h"


// http://gamedev.stackexchange.com/questions/13435/loading-and-using-an-hlsl-shader

Shader::Shader(PClip _child, const char* _path, const char* _entryPoint, const char* _shaderModel, int _precision,
	const char* _param1, const char* _param2, const char* _param3, const char* _param4, const char* _param5, const char* _param6, const char* _param7, const char* _param8, const char* _param9,
	PClip _clip1, PClip _clip2, PClip _clip3, PClip _clip4, IScriptEnvironment* env) :
	GenericVideoFilter(_child), path(_path), precision(_precision), clip1(_clip1), clip2(_clip2), clip3(_clip3), clip4(_clip4) {

	// Validate parameters
	if (vi.IsPlanar() || !vi.IsRGB32())
		env->ThrowError("Shader: Source must be float-precision RGB");
	if (path == NULL || path[0] == '\0')
		env->ThrowError("Shader: path to a compiled shader must be specified");
	if (precision != 1 && precision != 2)
		env->ThrowError("Precision must be 1 or 2");

	// Initialize
	dummyHWND = CreateWindowA("STATIC", "dummy", 0, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
	if (FAILED(render.Initialize(dummyHWND, vi.width / precision, vi.height, precision)))
		env->ThrowError("Shader: Initialize failed.");

	// Set pixel shader
	if (_shaderModel == NULL || _shaderModel[0] == '\0') {
		// Precompiled shader
		unsigned char* ShaderBuf = ReadBinaryFile(_path);
		if (ShaderBuf == NULL)
			env->ThrowError("Shader: Cannot open shader specified by path");
		if (FAILED(render.SetPixelShader((DWORD*)ShaderBuf)))
			env->ThrowError("Shader: Failed to load pixel shader");
		free(ShaderBuf);
	}
	else {
		// Compile HLSL shader code
		LPSTR errorMsg = NULL;
		if (FAILED(render.SetPixelShader(_path, _entryPoint, _shaderModel, &errorMsg)))
			env->ThrowError("Shader: Failed to compile pixel shader");
	}

	// Configure pixel shader
	ParseParam(_param1, env);
	ParseParam(_param2, env);
	ParseParam(_param3, env);
	ParseParam(_param4, env);
	ParseParam(_param5, env);
	ParseParam(_param6, env);
	ParseParam(_param7, env);
	ParseParam(_param8, env);
	ParseParam(_param9, env);

	CreateInputClip(0, child);
	CreateInputClip(1, clip1);
	CreateInputClip(2, clip2);
	CreateInputClip(3, clip3);
	CreateInputClip(4, clip4);
}

Shader::~Shader() {
	DestroyWindow(dummyHWND);
}

PVideoFrame __stdcall Shader::GetFrame(int n, IScriptEnvironment* env) {
	CopyInputClip(0, child, n, env);
	CopyInputClip(1, clip1, n, env);
	CopyInputClip(2, clip2, n, env);
	CopyInputClip(3, clip3, n, env);
	CopyInputClip(4, clip4, n, env);
	PVideoFrame dst = env->NewVideoFrame(vi);

	if FAILED(render.ProcessFrame(dst->GetWritePtr(), dst->GetPitch(), vi.width / precision, vi.height))
		env->ThrowError("Shader: ProcessFrame failed.");

	return dst;
}

void Shader::CreateInputClip(int index, PClip clip) {
	if (clip != NULL)
		render.CreateInputTexture(index, clip->GetVideoInfo().width / precision, clip->GetVideoInfo().height);
}

void Shader::CopyInputClip(int index, PClip clip, int n, IScriptEnvironment* env) {
	if (clip != NULL) {
		PVideoFrame frame = clip->GetFrame(n, env);
		if (FAILED(render.CopyToBuffer(frame->GetReadPtr(), frame->GetPitch(), index, clip->GetVideoInfo().width / precision, clip->GetVideoInfo().height)))
			env->ThrowError("Shader: CopyInputClip failed");
	}
}

unsigned char* Shader::ReadBinaryFile(const char* filePath) {
	FILE *fl = fopen(filePath, "rb");
	if (fl != NULL)
	{
		fseek(fl, 0, SEEK_END);
		long len = ftell(fl);
		unsigned char *ret = (unsigned char*)malloc(len);
		fseek(fl, 0, SEEK_SET);
		fread(ret, 1, len, fl);
		fclose(fl);
		return ret;
	}
	else
		return NULL;
}

void Shader::ParseParam(const char* param, IScriptEnvironment* env) {
	if (param != NULL && param[0] != '\0') {
		if (!SetParam((char*)param)) {
			// Throw error if failed to set parameter.
			char* ErrorText = "Shader failed to set parameter: ";
			char* FullText;
			FullText = (char*)malloc(strlen(ErrorText) + strlen(param) + 1);
			strcpy(FullText, ErrorText);
			strcat(FullText, param);
			env->ThrowError(FullText);
			free(FullText);
		}
	}
}

// Shader parameters have this format: "ParamName=1f"
// The last character is f for float, i for interet or b for boolean. For boolean, the value is 1 or 0.
// Returns True if parameter was valid and set successfully, otherwise false.
bool Shader::SetParam(char* param) {
	// Copy string to avoid altering source parameter.
	char* ParamCopy = (char*)malloc(strlen(param) + 1);
	strcpy(ParamCopy, param);

	// Split parameter string into its values and validate data.
	char* Name = strtok(ParamCopy, "=");
	if (Name == NULL)
		return false;
	char* Value = strtok(NULL, "=");
	if (Value == NULL || strlen(Value) < 2)
		return false;
	char Type = Value[strlen(Value) - 1];
	Value[strlen(Value) - 1] = '\0'; // Remove last character from value

	// Set parameter value.
	if (Type == 'f') {
		char* VectorValue = strtok(Value, ",");
		if (VectorValue == NULL) {
			// Single float value
			float FValue = strtof(Value, NULL);
			if (FAILED(render.SetPixelShaderFloatConstant(Name, FValue)))
				return false;
		}
		else {
			// Vector of 2, 3 or 4 values.
			D3DXVECTOR4 vector = { 0, 0, 0, 0 };
			vector.x = strtof(VectorValue, NULL);
			// Parse 2nd vector value
			char* VectorValue = strtok(NULL, ",");
			if (VectorValue != NULL) {
				vector.y = strtof(VectorValue, NULL);
				// Parse 3rd vector value
				char* VectorValue = strtok(NULL, ",");
				if (VectorValue != NULL) {
					vector.z = strtof(VectorValue, NULL);
					// Parse 4th vector value
					char* VectorValue = strtok(NULL, ",");
					if (VectorValue != NULL) {
						vector.w = strtof(VectorValue, NULL);
					}
				}
			}

			if (FAILED(render.SetPixelShaderVector(Name, &vector)))
				return false;
		}
	}
	else if (Type == 'i') {
		int IValue = atoi(Value);
		if (FAILED(render.SetPixelShaderIntConstant(Name, IValue)))
			return false;
	}
	else if (Type == 'b') {
		bool BValue = Value[0] == '0' ? false : true;
		if (FAILED(render.SetPixelShaderBoolConstant(Name, BValue)))
			return false;
	}
	else // invalid type
		return false;

	// Success
	return true;
}