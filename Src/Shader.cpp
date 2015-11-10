#include "Shader.h"
// http://gamedev.stackexchange.com/questions/13435/loading-and-using-an-hlsl-shader

Shader::Shader(PClip _child, const char* _path, const char* _entryPoint, const char* _shaderModel,
	const char* _param1, const char* _param2, const char* _param3, const char* _param4, const char* _param5, const char* _param6, const char* _param7, const char* _param8, const char* _param9,
	int _clip1, int _clip2, int _clip3, int _clip4, int _clip5, int _clip6, int _clip7, int _clip8, int _clip9, int _output, int _width, int _height, IScriptEnvironment* env) :
	GenericVideoFilter(_child), path(_path), entryPoint(_entryPoint), shaderModel(_shaderModel),
	param1(_param1), param2(_param2), param3(_param3), param4(_param4), param5(_param5), param6(_param6), param7(_param7), param8(_param8), param9(_param9) {

	ZeroMemory(&cmd, sizeof(CommandStruct));
	cmd.Path = _path;
	cmd.EntryPoint = _entryPoint;
	cmd.ShaderModel = _shaderModel;
	cmd.Param[0] = _param1;
	cmd.Param[1] = _param2;
	cmd.Param[2] = _param3;
	cmd.Param[3] = _param4;
	cmd.Param[4] = _param5;
	cmd.Param[5] = _param6;
	cmd.Param[6] = _param7;
	cmd.Param[7] = _param8;
	cmd.Param[8] = _param9;
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
	if (path == NULL || path[0] == '\0')
		env->ThrowError("Shader: path to a compiled shader must be specified");

	if (vi.pixel_type != VideoInfo::CS_Y8) {
		vi.pixel_type = VideoInfo::CS_Y8;
		vi.width = sizeof(CommandStruct);
		vi.height = 1;
	}
	else
		vi.height++;

	cmd.CommandIndex = vi.height - 1;
}

Shader::~Shader() {
	//if (cmd.ConstantTable != NULL)
	//	delete cmd.ConstantTable;
	//if (cmd.ShaderBuffer != NULL)
	//	delete cmd.ShaderBuffer;
	//if (cmd.Shader != NULL)
	//	delete cmd.Shader;
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