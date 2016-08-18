#include "Shader.h"
// http://gamedev.stackexchange.com/questions/13435/loading-and-using-an-hlsl-shader

Shader::Shader(PClip _child, const char* _path, const char* _entryPoint, const char* _shaderModel,
	const char* _param0, const char* _param1, const char* _param2, const char* _param3, const char* _param4, const char* _param5, const char* _param6, const char* _param7, const char* _param8,
	int _clip1, int _clip2, int _clip3, int _clip4, int _clip5, int _clip6, int _clip7, int _clip8, int _clip9, int _output, int _width, int _height, int _precision, const char* _defines, IScriptEnvironment* env) :
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
	cmd.Precision = _precision;

	// Validate parameters
	for (int i = 0; i < 9; i++) {
		if (cmd.ClipIndex[i] < 0 || cmd.ClipIndex[i] > 100)
			env->ThrowError("Shader: Clip index must be between 0 and 100");
	}
	if (cmd.OutputIndex < 0 || cmd.OutputIndex > 100)
		env->ThrowError("Shader: Output index must be between 0 and 100");
	if (_precision < -1 || _precision > 3)
		env->ThrowError("Shader: Invalid precision");

	if (vi.pixel_type != VideoInfo::CS_Y8 || vi.width != sizeof(CommandStruct)) {
		vi.pixel_type = VideoInfo::CS_Y8;
		vi.width = sizeof(CommandStruct);
		vi.height = 1;
	}
	else
		vi.height++;

	cmd.CommandIndex = vi.height - 1;

	// Configure pixel shader
	if (path && path[0] != '\0') {
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

	cmd.Defines = ParseDefines(_defines, env);
}

Shader::~Shader() {
	for (int i = 0; i < 9; i++) {
		if (cmd.Param[i].Count > 0)
			delete cmd.Param[i].Values;
	}
	if (cmd.Defines) {
		int i = 0;
		while (cmd.Defines[i].Name) {
			delete[] cmd.Defines[i].Name;
			delete[] cmd.Defines[i].Definition;
			++i;
		}
		delete[] cmd.Defines;
	}
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

int __stdcall Shader::SetCacheHints(int cachehints, int frame_range) {
	return cachehints == CachePolicyHint::CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
}

// The last character is f for float, i for interet or b for boolean. For boolean, the value is 1 or 0.
// Returns True if parameter was valid, otherwise false.
bool Shader::ParseParam(ParamStruct* param) {
	// Get value type
	char Type = param->String[strlen(param->String) - 1];
	// Create string with all but last character.
	std::string StrValue = std::string(param->String);
	StrValue = StrValue.substr(0, StrValue.size() - 1);

	// Split string on ','
	std::vector<std::string> StrVal = Split(StrValue, ',');

	param->Type = Type == 'f' ? ParamType::Float : Type == 'i' ? ParamType::Int : Type == 'b' ? ParamType::Bool : ParamType::None;
	if (param->Type == ParamType::None) // Invalid type
		return false;

	if (Type == 'b') {
		// Assign float array and store bool in it.
		param->Count = StrVal.size();
		param->Values = new float[param->Count];
	}
	else {
		// Assign blocks of Float4 vectors
		param->Count = (StrVal.size() + 3) / 4;
		param->Values = new float[param->Count * 4];
	}

	// Convert all values
	try {
		for (int i = 0; i < (int)StrVal.size(); i++) {
			if (Type == 'f')
				param->Values[i] = std::stof(StrVal[i]);
			else if (Type == 'i') {
				int IntVal = std::stoi(StrVal[i]);
				param->Values[i] = *(float*)&IntVal; // Store int data in float vector
			}
			else if (Type == 'b') {
				bool BoolVal = StrVal[i] == "0" ? false : true;
				param->Values[i] = *(float*)&BoolVal; // Store bool data in float vector (even if bool size may be smaller than float)
			}
		}
	}
	catch (...) {
		return false;
	}

	// Success
	return true;
}

D3DXMACRO* Shader::ParseDefines(const char* defines, IScriptEnvironment* env) {
	if (!defines || defines[0] == '\0')
		return NULL;

	const char* ErrorMsg = "Shader: Defines parameter is invalid";

	// Split string on ';'
	std::vector<std::string> StrPairs = Split(defines, ';');
	if (StrPairs.empty())
		env->ThrowError(ErrorMsg);

	D3DXMACRO* Result = new D3DXMACRO[StrPairs.size() + 1];

	int i = 0;
	for (auto const item: StrPairs) {
		// Allow terminating ';'
		if (item.length() == 0)
			break;

		// Split each pair of key/value on '='
		std::vector<std::string> StrValue = Split(item, '=');
		if (StrValue.size() != 2)
			env->ThrowError(ErrorMsg);
		
		// Copy string values
		Result[i].Name = new char[StrValue[0].length() + 1];
		strcpy((char*)Result[i].Name, StrValue[0].c_str());
		Result[i].Definition = new char[StrValue[1].length() + 1];
		strcpy((char*)Result[i].Definition, StrValue[1].c_str());
		++i;
	}
	*(int*)&Result[i] = NULL;
	return Result;
}

std::vector<std::string> &Shader::Split(const std::string &s, char delim, std::vector<std::string> &elems) {
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		elems.push_back(item);
	}
	return elems;
}

std::vector<std::string> Shader::Split(const std::string &s, char delim) {
	std::vector<std::string> elems;
	Split(s, delim, elems);
	return elems;
}