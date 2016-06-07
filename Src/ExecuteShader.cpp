#include "ExecuteShader.h"
// http://gamedev.stackexchange.com/questions/13435/loading-and-using-an-hlsl-shader

ExecuteShader::ExecuteShader(PClip _child, PClip _clip1, PClip _clip2, PClip _clip3, PClip _clip4, PClip _clip5, PClip _clip6, PClip _clip7, PClip _clip8, PClip _clip9, int _clipPrecision[9], int _precision, int _outputPrecision, IScriptEnvironment* env) :
	GenericVideoFilter(_child), m_Precision(_precision), m_OutputPrecision(_outputPrecision) {

	// Validate parameters
	if (!vi.IsY8())
		env->ThrowError("ExecuteShader: Source must be a command chain");

	memcpy(m_ClipPrecision, _clipPrecision, sizeof(int) * 9);
	m_clips[0] = _clip1;
	m_clips[1] = _clip2;
	m_clips[2] = _clip3;
	m_clips[3] = _clip4;
	m_clips[4] = _clip5;
	m_clips[5] = _clip6;
	m_clips[6] = _clip7;
	m_clips[7] = _clip8;
	m_clips[8] = _clip9;

	m_PrecisionMultiplier = AdjustPrecision(env, m_Precision);
	m_OutputMultiplier = AdjustPrecision(env, m_OutputPrecision);
	for (int i = 0; i < 9; i++) {
		m_ClipMultiplier[i] = AdjustPrecision(env, m_ClipPrecision[i]);
	}

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
	if (FAILED(render->Initialize(dummyHWND, m_ClipPrecision, m_Precision, m_OutputPrecision)))
		env->ThrowError("ExecuteShader: Initialize failed.");

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
		IsLast = i == srcHeight - 1; // Only create memory on the CPU side for the last command, as it is the only one that needs to be read back.

		if (cmd.Path != NULL && cmd.Path[0] != '\0')
			ConfigureShader(&cmd, env);
		else {
			if (cmd.ClipIndex[0] == cmd.OutputIndex)
				env->ThrowError("ExecuteShader: If Path is not specified, Output must be different than Clip1 to copy clip data");
			if (cmd.OutputWidth != 0 || cmd.OutputHeight != 0)
				env->ThrowError("ExecuteShader: If Path is not specified, OutputWidth and OutputHeight cannot be set");
		}

		texture = render->FindTextureByClipIndex(cmd.OutputIndex, env);
		// If clip at output position isn't defined, use dimensions of first clip by default.
		if (texture->Texture == NULL)
			texture = render->FindTextureByClipIndex(cmd.ClipIndex[0], env);
		OutputWidth = cmd.OutputWidth > 0 ? cmd.OutputWidth : texture->Width;
		OutputHeight = cmd.OutputHeight > 0 ? cmd.OutputHeight : texture->Height;

		if (FAILED(render->CreateInputTexture(9 + i, cmd.OutputIndex, OutputWidth, OutputHeight, IsLast, IsLast)))
			env->ThrowError("ExecuteShader: Failed to create input texture.");

		if (IsLast) {
			if (cmd.OutputIndex != 1)
				env->ThrowError("ExecuteShader: Last command must have Output = 1");

			vi.width = OutputWidth * m_OutputMultiplier;
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
	bool IsLast;
	InputTexture* texture;
	int OutputWidth, OutputHeight;
  
	std::unique_lock<std::mutex> my_lock(executeshader_mutex);
	// P.F. prevent parallel use of the class-global "render"
	// Mutex will be unlocked when gets out of scope (exit from GetFrame() or {} block )

	render->ResetTextureClipIndex();

	// Copy input clips from AviSynth
	for (int j = 0; j < 9; j++) {
		CopyInputClip(j, n, env);
	}

	for (int i = 0; i < srcHeight; i++) {
		memcpy(&cmd, srcReader, sizeof(CommandStruct));
		srcReader += src->GetPitch();
		IsLast = i == srcHeight - 1;

		if (cmd.Path != NULL && cmd.Path[0] != '\0') {
			// If clip at output position isn't defined, use dimensions of first clip by default.
			texture = render->FindTextureByClipIndex(cmd.OutputIndex, env);
			if (texture->Texture == NULL)
				texture = render->FindTextureByClipIndex(cmd.ClipIndex[0], env);
			OutputWidth = cmd.OutputWidth > 0 ? cmd.OutputWidth : texture->Width;
			OutputHeight = cmd.OutputHeight > 0 ? cmd.OutputHeight : texture->Height;

			// Set Param0 and Param1 default values.
			SetDefaultParamValue(&cmd.Param[0], (float)OutputWidth, (float)OutputHeight, 0, 0);
			SetDefaultParamValue(&cmd.Param[1], 1.0f / OutputWidth, 1.0f / OutputHeight, 0, 0);

			// Configure pixel shader
			for (int i = 0; i < 9; i++) {
				if (cmd.Param[i].Type != ParamType::None) {
					if (FAILED(render->SetPixelShaderConstant(i, &cmd.Param[i])))
						env->ThrowError("ExecuteShader failed to set parameters.");
				}
			}

			if FAILED(render->ProcessFrame(&cmd, OutputWidth, OutputHeight, IsLast, env))
				env->ThrowError("ExecuteShader: ProcessFrame failed.");
		}
		else {
			// Only copy Clip1 to Output without processing
			texture = render->FindTextureByClipIndex(cmd.ClipIndex[0], env);
			if FAILED(render->CopyBuffer(texture, cmd.CommandIndex, cmd.OutputIndex, env))
				env->ThrowError("ExecuteShader: CopyBuffer failed.");
		}
	}

	// After last command, copy result back to AviSynth.
	PVideoFrame dst = env->NewVideoFrame(vi);
	render->CopyBufferToAviSynth(srcHeight - 1, dst->GetWritePtr(), dst->GetPitch(), env);
	return dst;
}

// Returns how to multiply the AviSynth frame format's width based on the precision.
int ExecuteShader::AdjustPrecision(IScriptEnvironment* env, int precision) {
	if (precision == 0)
		return 1; // Same width as Y8
	if (precision == 1)
		return 1; // Same width as RGB24
	else if (precision == 2)
		return 2; // Double width as RGB32
	else if (precision == 3)
		return 2; // Double width as RGB32
	else
		env->ThrowError("ExecuteShader: Precision must be 0, 1, 2 or 3");
}

// Sets the default parameter value if it is not already defined.
void ExecuteShader::SetDefaultParamValue(ParamStruct* p, float value0, float value1, float value2, float value3) {
	if (p->Type == ParamType::None) {
		p->Type = ParamType::Float;
		p->Count = 1;
		p->Values = new float[4];
		p->Values[0] = value0;
		p->Values[1] = value1;
		p->Values[2] = value2;
		p->Values[3] = value3;
	}
}

void ExecuteShader::CreateInputClip(int index, IScriptEnvironment* env) {
	// clip1-clip9 take texture spots 0-8. Then, each shader execution will output in subsequent texture spots.
	PClip clip = m_clips[index];
	if (clip != NULL) {
		if (!clip->GetVideoInfo().IsRGB32())
			env->ThrowError("ExecuteShader: You must first call ConvertToShader on source");
		else if (m_ClipPrecision[index] == 0 && !clip->GetVideoInfo().IsY8())
			env->ThrowError("ExecuteShader: Clip with Precision=0 must be in Y8 format");

		if (FAILED(render->CreateInputTexture(index, index + 1, clip->GetVideoInfo().width / m_ClipMultiplier[index], clip->GetVideoInfo().height, true, false)))
			env->ThrowError("ExecuteShader: Failed to create input textures.");
	}
	else
		render->m_InputTextures[index].ClipIndex = index + 1;
}

// Copies the first clip from AviSynth before running the first command.
void ExecuteShader::CopyInputClip(int index, int n, IScriptEnvironment* env) {
	PClip clip = m_clips[index];
	if (m_clips[index] != NULL) {
		PVideoFrame frame = clip->GetFrame(n, env);
		if (FAILED(render->CopyAviSynthToBuffer(frame->GetReadPtr(), frame->GetPitch(), index, clip->GetVideoInfo().width / m_ClipMultiplier[index], clip->GetVideoInfo().height, env)))
			env->ThrowError("ExecuteShader: CopyInputClip failed");
	}
}

void ExecuteShader::ConfigureShader(CommandStruct* cmd, IScriptEnvironment* env) {
	if FAILED(render->InitPixelShader(cmd, env)) {
		char* ErrorText = "Shader: Failed to open pixel shader ";
		char* FullText;
		size_t TextLength = strlen(ErrorText) + strlen(cmd->Path) + 1;
		FullText = (char*)malloc(TextLength);
		strcpy_s(FullText, TextLength, ErrorText);
		strcat_s(FullText, TextLength, cmd->Path);
		env->ThrowError(FullText);
		free(FullText);
	}
}
