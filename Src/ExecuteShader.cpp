#include "ExecuteShader.h"
// http://gamedev.stackexchange.com/questions/13435/loading-and-using-an-hlsl-shader

ExecuteShader::ExecuteShader(PClip _child, PClip _clip1, PClip _clip2, PClip _clip3, PClip _clip4, PClip _clip5, PClip _clip6, PClip _clip7, PClip _clip8, PClip _clip9, int _clipPrecision[9], int _precision, int _outputPrecision, bool _planarOut, IScriptEnvironment* env) :
	GenericVideoFilter(_child), m_Precision(_precision), m_OutputPrecision(_outputPrecision), m_PlanarOut(_planarOut) {

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
	vi.pixel_type = m_PlanarOut ? VideoInfo::CS_YV24 : VideoInfo::CS_BGR32;

	render = new D3D9RenderImpl();
	if (FAILED(render->Initialize(dummyHWND, m_ClipPrecision, m_Precision, m_OutputPrecision, m_PlanarOut)))
		env->ThrowError("ExecuteShader: Initialize failed.");

	// vi.width and vi.height must be set during constructor
	std::vector<InputTexture*> TextureList;
	AllocateAndCopyInputTextures(&TextureList, 0, true, env);
	GetFrameInternal(&TextureList, 0, true, env);
	// Release all 
	render->ClearTextures(&TextureList);
}

ExecuteShader::~ExecuteShader() {
	DestroyWindow(dummyHWND);
	delete render;
}

PVideoFrame __stdcall ExecuteShader::GetFrame(int n, IScriptEnvironment* env) {
	std::unique_lock<std::mutex> my_lock(executeshader_mutex);

	std::vector<InputTexture*> TextureList;
	AllocateAndCopyInputTextures(&TextureList, n, false, env);

	GetFrameInternal(&TextureList, n, false, env);

	// After last command, copy result back to AviSynth.
	PVideoFrame dst = env->NewVideoFrame(vi);
	if (m_PlanarOut) {
		if FAILED(render->CopyBufferToAviSynthPlanar(srcHeight - 1, TextureList.back(), dst->GetWritePtr(PLANAR_Y), dst->GetWritePtr(PLANAR_U), dst->GetWritePtr(PLANAR_V), dst->GetPitch(PLANAR_Y), env))
			env->ThrowError("CopyBufferToAviSynthPlanar failed");
	}
	else {
		if FAILED(render->CopyBufferToAviSynth(srcHeight - 1, TextureList.back(), dst->GetWritePtr(), dst->GetPitch(), env))
			env->ThrowError("CopyBufferToAviSynth failed");
	}

	// Release all 
	render->ClearTextures(&TextureList);

	return dst;
}

// This part of GetFrame is protected with unique_lock to prevent parallel executions
void ExecuteShader::GetFrameInternal(std::vector<InputTexture*>* textureList, int n, bool init, IScriptEnvironment* env) {
	// Mutex will be unlocked when gets out of scope (exit from GetFrameInternal)
	//std::unique_lock<std::mutex> my_lock(executeshader_mutex);

	// Each row of input clip contains commands to execute
	CommandStruct cmd;
	bool IsLast;
	bool IsPlanar;
	InputTexture* texture;
	int OutputWidth, OutputHeight;
	InputTexture* NewTexture;

	PVideoFrame src = child->GetFrame(n, env);
	const byte* srcReader = src->GetReadPtr();

	for (int i = 0; i < srcHeight; i++) {
		memcpy(&cmd, srcReader, sizeof(CommandStruct));
		srcReader += src->GetPitch();
		IsLast = i == srcHeight - 1;

		if (cmd.Path != NULL && cmd.Path[0] != '\0') {
			ConfigureShader(&cmd, env);

			// If clip at output position isn't defined, use dimensions of first clip by default.
			texture = render->FindTexture(textureList, cmd.OutputIndex);
			if (texture == NULL || texture->Texture == NULL)
				texture = render->FindTexture(textureList, cmd.ClipIndex[0]);
			OutputWidth = cmd.OutputWidth > 0 ? cmd.OutputWidth : texture->Width;
			OutputHeight = cmd.OutputHeight > 0 ? cmd.OutputHeight : texture->Height;
			IsPlanar = texture->TextureY != NULL && (cmd.Path == NULL || cmd.Path[0] == '\0');

			if (IsLast) {
				if (cmd.OutputIndex != 1)
					env->ThrowError("ExecuteShader: Last command must have Output = 1");

				vi.width = OutputWidth * m_OutputMultiplier;
				vi.height = OutputHeight;
			}

			// Set Param0 and Param1 default values.
			bool SetDefault1 = SetDefaultParamValue(&cmd.Param[0], (float)OutputWidth, (float)OutputHeight, 0, 0);
			bool SetDefault2 = SetDefaultParamValue(&cmd.Param[1], 1.0f / OutputWidth, 1.0f / OutputHeight, 0, 0);

			// Configure pixel shader
			for (int i = 0; i < 9; i++) {
				if (cmd.Param[i].Type != ParamType::None) {
					if (FAILED(render->SetPixelShaderConstant(i, &cmd.Param[i])))
						env->ThrowError("ExecuteShader failed to set parameters.");
				}
			}

			if FAILED(render->ProcessFrame(textureList, &cmd, OutputWidth, OutputHeight, IsLast, 0, env))
				env->ThrowError("ExecuteShader: ProcessFrame failed.");

			// Delete memory allocated for default values
			if (SetDefault1)
				delete cmd.Param[0].Values;
			if (SetDefault2)
				delete cmd.Param[1].Values;
		}
		else {
			if (IsLast)
				env->ThrowError("ExecuteShader: A shader path must be specified for the last command");
			if (cmd.ClipIndex[0] == cmd.OutputIndex)
				env->ThrowError("ExecuteShader: If Path is not specified, Output must be different than Clip1 to copy clip data");
			if (cmd.OutputWidth != 0 || cmd.OutputHeight != 0)
				env->ThrowError("ExecuteShader: If Path is not specified, OutputWidth and OutputHeight cannot be set");

			// Only copy Clip1 to Output without processing
			texture = render->FindTexture(textureList, cmd.ClipIndex[0]);
			if FAILED(render->CopyBuffer(textureList, texture, cmd.OutputIndex, env))
				env->ThrowError("ExecuteShader: CopyBuffer failed.");
		}
	}
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
// Returns whether a value was assigned.
bool ExecuteShader::SetDefaultParamValue(ParamStruct* p, float value0, float value1, float value2, float value3) {
	if (p->Type == ParamType::None) {
		p->Type = ParamType::Float;
		p->Count = 1;
		p->Values = new float[4];
		p->Values[0] = value0;
		p->Values[1] = value1;
		p->Values[2] = value2;
		p->Values[3] = value3;
		return true;
	}
	return false;
}

void ExecuteShader::AllocateAndCopyInputTextures(std::vector<InputTexture*>* list, int n, bool init, IScriptEnvironment* env) {
	// Allocated textures must be released manually after use
	InputTexture* NewTexture;
	for (int i = 0; i < D3D9RenderImpl::maxClips; i++) {
		PClip clip = m_clips[i];
		if (clip != NULL) {
			// Allocate textures
			bool IsPlanar = clip->GetVideoInfo().IsYV24();
			if (!clip->GetVideoInfo().IsRGB32() && !IsPlanar)
				env->ThrowError("ExecuteShader: You must first call ConvertToShader on source");
			else if (m_ClipPrecision[i] == 0 && !clip->GetVideoInfo().IsY8())
				env->ThrowError("ExecuteShader: Clip with Precision=0 must be in Y8 format");

			NewTexture = new InputTexture();
			if (FAILED(render->CreateTexture(i + 1, clip->GetVideoInfo().width / m_ClipMultiplier[i], clip->GetVideoInfo().height, true, IsPlanar, false, -1, NewTexture)))
				env->ThrowError("ExecuteShader: Failed to create input textures.");

			list->push_back(NewTexture);

			if (!init) {
				// Copy frame data from AviSynth
				PVideoFrame frame = clip->GetFrame(n, env);
				if (clip->GetVideoInfo().IsYV24()) {
					// Copy planar data from YV24.
					if (FAILED(render->CopyAviSynthToPlanarBuffer(frame->GetReadPtr(PLANAR_Y), frame->GetReadPtr(PLANAR_U), frame->GetReadPtr(PLANAR_V), frame->GetPitch(PLANAR_Y), i, clip->GetVideoInfo().width, clip->GetVideoInfo().height, NewTexture, env)))
						env->ThrowError("ExecuteShader: CopyInputClip failed");
				}
				else {
					// Copy regular data after calling ConvertToShader.
					if (FAILED(render->CopyAviSynthToBuffer(frame->GetReadPtr(), frame->GetPitch(), i, clip->GetVideoInfo().width / m_ClipMultiplier[i], clip->GetVideoInfo().height, NewTexture, env)))
						env->ThrowError("ExecuteShader: CopyInputClip failed");
				}
			}
		}
	}
}

void ExecuteShader::ConfigureShader(CommandStruct* cmd, IScriptEnvironment* env) {
	if FAILED(render->InitPixelShader(cmd, 0, env)) {
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
