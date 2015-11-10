#include "ExecuteShader.h"
// http://gamedev.stackexchange.com/questions/13435/loading-and-using-an-hlsl-shader

ExecuteShader::ExecuteShader(PClip _child, PClip _clip1, PClip _clip2, PClip _clip3, PClip _clip4, PClip _clip5, PClip _clip6, PClip _clip7, PClip _clip8, PClip _clip9, int _precision, IScriptEnvironment* env) :
	GenericVideoFilter(_child), precision(_precision) {

	m_clips[0] = _clip1;
	m_clips[1] = _clip2;
	m_clips[2] = _clip3;
	m_clips[3] = _clip4;
	m_clips[4] = _clip5;
	m_clips[5] = _clip6;
	m_clips[6] = _clip7;
	m_clips[7] = _clip8;
	m_clips[8] = _clip9;

	// Validate parameters
	if (!vi.IsY8())
		env->ThrowError("ExecuteShader: Source must be a command chain");
	if (precision < 1 || precision > 3)
		env->ThrowError("ExecuteShader: Precision must be 1, 2 or 3");

	// Initialize
	dummyHWND = CreateWindowA("STATIC", "dummy", 0, 0, 0, 100, 100, NULL, NULL, NULL, NULL);

	// We must change pixel type here for the next filter to recognize it properly during its initialization
	srcHeight = vi.height;
	vi.pixel_type = VideoInfo::CS_BGR32;

	InitializeDevice(env);
}

ExecuteShader::~ExecuteShader() {
	delete render;
	DestroyWindow(dummyHWND);
}

void ExecuteShader::InitializeDevice(IScriptEnvironment* env) {
	render = new D3D9RenderImpl();
	if (FAILED(render->Initialize(dummyHWND, precision)))
		env->ThrowError("ExecuteShader: Initialize failed.");

	// We only need to know the difference between precision 2 and 3 to initialize video buffers. Then, both are 16 bits.
	if (precision == 3)
		precision = 2;

	CreateInputClip(0, env);
	CreateInputClip(1, env);
	CreateInputClip(2, env);
	CreateInputClip(3, env);
	CreateInputClip(4, env);
	CreateInputClip(5, env);
	CreateInputClip(6, env);
	CreateInputClip(7, env);
	CreateInputClip(8, env);

	PVideoFrame src = child->GetFrame(0, env);
	const byte* srcReader = src->GetReadPtr();

	// Each row of input clip contains commands to execute. Create one texture for each command.
	CommandStruct cmd;
	InputTexture* texture;
	int OutputWidth, OutputHeight;
	bool IsLast;
	render->ResetTextureClipIndex();
	for (int i = 0; i < srcHeight; i++) {
		memcpy(&cmd, srcReader, sizeof(CommandStruct));
		srcReader += src->GetPitch();

		ConfigureShader(&cmd, env);

		texture = render->FindTextureByClipIndex(cmd.OutputIndex, env);
		// If clip at output position isn't defined, use dimensions of first clip by default.
		if (texture->Texture == NULL)
			texture = render->FindTextureByClipIndex(cmd.ClipIndex[0], env);
		OutputWidth = cmd.OutputWidth > 0 ? cmd.OutputWidth : texture->Width;
		OutputHeight = cmd.OutputHeight > 0 ? cmd.OutputHeight : texture->Height;
		IsLast = i == srcHeight - 1; // Only create memory on the CPU side for the last command, as it is the only one that needs to be read back.
		render->CreateInputTexture(9 + i, cmd.OutputIndex, OutputWidth, OutputHeight, IsLast, IsLast);

		if (IsLast) {
			if (cmd.OutputIndex != 1)
				env->ThrowError("ExecuteShader: Last command must have Output = 1");

			vi.width = OutputWidth * precision;
			vi.height = OutputHeight;
		}
	}
	render->ResetTextureClipIndex();
}

PVideoFrame __stdcall ExecuteShader::GetFrame(int n, IScriptEnvironment* env) {
	PVideoFrame src = child->GetFrame(n, env);
	const byte* srcReader = src->GetReadPtr();

	// Each row of input clip contains commands to execute
	CommandStruct cmd;
	InputTexture* texture;
	int OutputWidth, OutputHeight;
	render->ResetTextureClipIndex();
	for (int i = 0; i < srcHeight; i++) {
		memcpy(&cmd, srcReader, sizeof(CommandStruct));
		srcReader += src->GetPitch();

		// Copy input clips from AviSynth
		for (int j = 0; j < 9; j++) {
			CopyInputClip(j, n, env);
		}

		texture = render->FindTextureByClipIndex(cmd.OutputIndex, env);
		// If clip at output position isn't defined, use dimensions of first clip by default.
		if (texture->Texture == NULL)
			texture = render->FindTextureByClipIndex(cmd.ClipIndex[0], env);
		OutputWidth = cmd.OutputWidth > 0 ? cmd.OutputWidth : texture->Width;
		OutputHeight = cmd.OutputHeight > 0 ? cmd.OutputHeight : texture->Height;

		if FAILED(render->ProcessFrame(&cmd, OutputWidth, OutputHeight, env))
			env->ThrowError("Shader: ProcessFrame failed.");
	}

	// After last command, copy result back to AviSynth.
	PVideoFrame dst = env->NewVideoFrame(vi);
	render->CopyBufferToAviSynth(srcHeight - 1, dst->GetWritePtr(), dst->GetPitch(), env);
	return dst;
}

void ExecuteShader::CreateInputClip(int index, IScriptEnvironment* env) {
	// clip1-clip9 take texture spots 0-8. Then, each shader execution will output in subsequent texture spots.
	PClip clip = m_clips[index];
	if (clip != NULL) {
		if (!clip->GetVideoInfo().IsRGB32())
			env->ThrowError("ExecuteShader: Source must be float-precision RGB");

		render->CreateInputTexture(index, index + 1, clip->GetVideoInfo().width / precision, clip->GetVideoInfo().height, true, false);
	}
	else
		render->m_InputTextures[index].ClipIndex = index + 1;
}

// Copies the first clip from AviSynth before running the first command.
void ExecuteShader::CopyInputClip(int index, int n, IScriptEnvironment* env) {
	PClip clip = m_clips[index];
	if (m_clips[index] != NULL) {
		PVideoFrame frame = clip->GetFrame(n, env);
		if (FAILED(render->CopyAviSynthToBuffer(frame->GetReadPtr(), frame->GetPitch(), index, clip->GetVideoInfo().width / precision, clip->GetVideoInfo().height, env)))
			env->ThrowError("ExecuteShader: CopyInputClip failed");
	}
}

void ExecuteShader::ConfigureShader(CommandStruct* cmd, IScriptEnvironment* env) {
	if FAILED(render->InitPixelShader(cmd, env))
		env->ThrowError("Shader: Failed to compile pixel shader");

	// Configure pixel shader
	for (int i = 0; i < 9; i++) {
		ParseParam(render->m_Shaders[cmd->CommandIndex].ConstantTable, cmd->Param[i], env);
	}
}

void ExecuteShader::ParseParam(LPD3DXCONSTANTTABLE table, const char* param, IScriptEnvironment* env) {
	if (param != NULL && param[0] != '\0') {
		if (!SetParam(table, (char*)param)) {
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
bool ExecuteShader::SetParam(LPD3DXCONSTANTTABLE table, char* param) {
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
			if (FAILED(render->SetPixelShaderFloatConstant(table, Name, FValue)))
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

			if (FAILED(render->SetPixelShaderVector(table, Name, &vector)))
				return false;
		}
	}
	else if (Type == 'i') {
		int IValue = atoi(Value);
		if (FAILED(render->SetPixelShaderIntConstant(table, Name, IValue)))
			return false;
	}
	else if (Type == 'b') {
		bool BValue = Value[0] == '0' ? false : true;
		if (FAILED(render->SetPixelShaderBoolConstant(table, Name, BValue)))
			return false;
	}
	else // invalid type
		return false;

	// Success
	return true;
}