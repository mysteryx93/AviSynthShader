#include "Shader.h"


// http://gamedev.stackexchange.com/questions/13435/loading-and-using-an-hlsl-shader

Shader::Shader(PClip _child, const char* _path, const char* _entryPoint, const char* _shaderModel,
	const char* _param1, const char* _param2, const char* _param3, const char* _param4, const char* _param5, const char* _param6, const char* _param7, const char* _param8, const char* _param9,
	PClip _clip1, PClip _clip2, PClip _clip3, PClip _clip4, IScriptEnvironment* env) :
	GenericVideoFilter(_child), path(_path), precision(2), clip1(_clip1), clip2(_clip2), clip3(_clip3), clip4(_clip4), BUFFERSIZE(4096) {

	// Validate parameters
	if (vi.IsPlanar() || !vi.IsRGB32())
		env->ThrowError("Shader: Source must be float-precision RGB");
	if (path == NULL || path[0] == '\0')
		env->ThrowError("Shader: path to a compiled shader must be specified");

	// Initialize
	dummyHWND = CreateWindowA("STATIC", "dummy", 0, 0, 0, vi.width + 6, vi.height + 29, NULL, NULL, NULL, NULL);
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

	render.CreateInputTexture(0);
	if (clip1 != NULL)
		render.CreateInputTexture(1);
	if (clip2 != NULL)
		render.CreateInputTexture(2);
	if (clip3 != NULL)
		render.CreateInputTexture(3);
	if (clip4 != NULL)
		render.CreateInputTexture(4);
}

Shader::~Shader() {
	DestroyWindow(dummyHWND);
}


PVideoFrame __stdcall Shader::GetFrame(int n, IScriptEnvironment* env) {
	// PVideoFrame src = child->GetFrame(n, env);

	PVideoFrame dst = env->NewVideoFrame(vi);
	CopyInputClip(0, n, env);
	if (clip1 != NULL)
		CopyInputClip(1, n, env);
	else if (clip2 != NULL)
		CopyInputClip(2, n, env);
	else if (clip3 != NULL)
		CopyInputClip(3, n, env);
	else if (clip4 != NULL)
		CopyInputClip(4, n, env);

	if FAILED(render.ProcessFrame(dst->GetWritePtr(), dst->GetPitch()))
		env->ThrowError("Shader: ProcessFrame failed.");

	return dst;
}


void Shader::CopyInputClip(int index, int n, IScriptEnvironment* env) {
	PClip input;
	if (index == 0)
		input = child;
	else if (index == 1)
		input = clip1;
	else if (index == 2)
		input = clip2;
	else if (index == 3)
		input = clip3;
	else if (index == 4)
		input = clip4;
	else
		env->ThrowError("Shader: CopyInputClip invalid index");

	PVideoFrame frame = input->GetFrame(n, env);
	if (FAILED(render.CopyToBuffer(frame->GetReadPtr(), frame->GetPitch(), index)))
		env->ThrowError("Shader: CopyInputClip failed");
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
		}
	}
}

// Shader parameters have this format: "ParamName=1f"
// The last character is f for float, i for interet or b for boolean. For boolean, the value is 1 or 0.
// Returns True if parameter was valid and set successfully, otherwise false.
bool Shader::SetParam(char* param) {
	// Split parameter string into its values and validate data.
	char* Name = strtok(param, "=");
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
			D3DXVECTOR4 vector = {0, 0, 0, 0};
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