#include "Shader.h"
// http://gamedev.stackexchange.com/questions/13435/loading-and-using-an-hlsl-shader

Shader::Shader(PClip _child, const char* _path, const char* _entryPoint, const char* _shaderModel,
	const char* _param0, const char* _param1, const char* _param2, const char* _param3, const char* _param4, const char* _param5, const char* _param6, const char* _param7, const char* _param8,
	int _clip1, int _clip2, int _clip3, int _clip4, int _clip5, int _clip6, int _clip7, int _clip8, int _clip9, int _output, int _width, int _height, IScriptEnvironment* env) :
	GenericVideoFilter(_child), path(_path), entryPoint(_entryPoint), shaderModel(_shaderModel),
	param1(_param0), param2(_param1), param3(_param2), param4(_param3), param5(_param4), param6(_param5), param7(_param6), param8(_param7), param9(_param8) {

	ZeroMemory(&cmd, sizeof(CommandStruct));
	cmd.Path = _path;
	cmd.EntryPoint = _entryPoint;
	cmd.ShaderModel = _shaderModel;
	cmd.Param[0].String = _param0;
	cmd.Param[1].String = _param1;
	cmd.Param[2].String = _param2;
	cmd.Param[3].String = _param3;
	cmd.Param[4].String = _param4;
	cmd.Param[5].String = _param5;
	cmd.Param[6].String = _param6;
	cmd.Param[7].String = _param7;
	cmd.Param[8].String = _param8;
	cmd.ClipIndex[0] = _clip1;
	cmd.ClipIndex[1] = _clip2;
	cmd.ClipIndex[2] = _clip3;
	cmd.ClipIndex[3] = _clip4;
	cmd.ClipIndex[4] = _clip5;
	cmd.ClipIndex[5] = _clip6;
	cmd.ClipIndex[6] = _clip7;
	cmd.ClipIndex[7] = _clip8;
	cmd.ClipIndex[8] = _clip9;
	cmd.OutputIndex = _output;
	cmd.OutputWidth = _width;
	cmd.OutputHeight = _height;

	// Validate parameters
	//if (path == NULL || path[0] == '\0')
	//	env->ThrowError("Shader: path to a compiled shader must be specified");

	if (vi.pixel_type != VideoInfo::CS_Y8) {
		vi.pixel_type = VideoInfo::CS_Y8;
		vi.width = sizeof(CommandStruct);
		vi.height = 1;
	}
	else
		vi.height++;

	cmd.CommandIndex = vi.height - 1;

	// Configure pixel shader
	if (path != NULL && path[0] != '\0') {
		for (int i = 0; i < 9; i++) {
			ParamStruct* param = &cmd.Param[i];
			if (param->String && param->String[0] != '\0') {
				if (!ParseParam(param)) {
					char* ErrorText = "Shader invalid parameter: Param%d = %s";
					char* FullText;
					int FullTextLength = strlen(ErrorText) - 3 + strlen(param->String) + 1;
					FullText = (char*)malloc(FullTextLength);
					sprintf_s(FullText, FullTextLength, ErrorText, i + 1, param->String);
					env->ThrowError(FullText);
					free(FullText);
				}
			}
		}
	}
}

Shader::~Shader() {
}


PVideoFrame __stdcall Shader::GetFrame(int n, IScriptEnvironment* env) {
	// Create an empty frame and insert command data as the last line. One command is stored per frame line and the source clip is discarded.
	PVideoFrame dst = env->NewVideoFrame(vi);
	BYTE* dstWriter = dst->GetWritePtr();
	if (vi.height > 1) {
		PVideoFrame src = child->GetFrame(n, env);
		const BYTE* srcReader = src->GetReadPtr();
		env->BitBlt(dstWriter, dst->GetPitch(), srcReader, src->GetPitch(), sizeof(CommandStruct), vi.height - 1);
	}
	dstWriter += dst->GetPitch() * (vi.height - 1);
	memcpy(dstWriter, &cmd, sizeof(CommandStruct));
	return dst;
}

// The last character is f for float, i for interet or b for boolean. For boolean, the value is 1 or 0.
// Returns True if parameter was valid, otherwise false.
bool Shader::ParseParam(ParamStruct* param) {
	// Count ',' to determine vector size.
	int Count = 0;
	int Pos = 0;
	while (param->String[Pos] != '\0') {
		if (param->String[Pos++] == ',')
			Count++;
	}

	// Set parameter value.
	char Type = param->String[strlen(param->String) - 1];
	if (Type == 'f') {
		param->Type = ParamType::Float;
		if (Count == 0) {
			if (sscanf_s(param->String, "%ff", &param->Value[0]) < 1)
				return false;
		}
		else if (Count == 1) {
			if (sscanf_s(param->String, "%f,%ff", &param->Value[0], &param->Value[1]) < 2)
				return false;
		}
		else if (Count == 2) {
			if (sscanf_s(param->String, "%f,%f,%ff", &param->Value[0], &param->Value[1], &param->Value[2]) < 3)
				return false;
		}
		else if (Count == 3) {
			if (sscanf_s(param->String, "%f,%f,%f,%ff", &param->Value[0], &param->Value[1], &param->Value[2], &param->Value[3]) < 4)
				return false;
		}
		else
			return false;
	}
	else if (Type == 'i') {
		param->Type = ParamType::Int;
		if (Count == 0) {
			if (sscanf_s(param->String, "%ii", (int*)&param->Value[0]) < 1)
				return false;
		}
		else if (Count == 1) {
			if (sscanf_s(param->String, "%i,%ii", (int*)&param->Value[0], (int*)&param->Value[1]) < 2)
				return false;
		}
		else if (Count == 2) {
			if (sscanf_s(param->String, "%i,%i,%ii", (int*)&param->Value[0], (int*)&param->Value[1], (int*)&param->Value[2]) < 3)
				return false;
		}
		else if (Count == 3) {
			if (sscanf_s(param->String, "%i,%i,%i,%ii", (int*)&param->Value[0], (int*)&param->Value[1], (int*)&param->Value[2], (int*)&param->Value[3]) < 4)
				return false;
		}
		else
			return false;
	}
	else if (Type == 'b') {
		param->Type = ParamType::Bool;
		bool* pOut = (bool*)param->Value;
		pOut[0] = param->String[0] == '0' ? false : true;
	}
	else // invalid type
		return false;

	// Success
	return true;
}
