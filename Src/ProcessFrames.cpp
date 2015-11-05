#include "ProcessFrames.h"

ProcessFrames::ProcessFrames(IScriptEnvironment* env) : m_precision(2) {
	m_dummyHWND = CreateWindowA("STATIC", "dummy", 0, 0, 0, 100, 100, NULL, NULL, NULL, NULL);

	if (FAILED(render.Initialize(m_dummyHWND, m_precision, env)))
		env->ThrowError("ExecuteShader: Initialize failed.");
}

ProcessFrames::~ProcessFrames() {
	DestroyWindow(m_dummyHWND);
}

HRESULT ProcessFrames::Execute(CommandStruct* cmd, CommandStruct* previousCmd) {
	//std::lock_guard<std::mutex> lock(*cmd->Lock);

	// Compile shader if it's not yet compiled.
	render.SetPixelShader(cmd);

	int Width, Height;
	FrameRef Input;
	InputTexture* InputList[9]{ NULL };
	for (int i = 0; i < 9; i++) {
		if (cmd->Input[i].Pitch > 0) {
			Input = cmd->Input[i];
			//Width = Input->GetRowSize() / m_precision / 4;
			//Height = Input->GetHeight();
			HR(render.FindInputTexture(Input.Width, Input.Height, InputList[i]));
			HR(render.CopyAviSynthToBuffer(Input.Buffer, Input.Pitch, InputList[i]));
		}
	}
	HR(render.ProcessFrame(cmd, InputList, previousCmd));
	return S_OK;
}

HRESULT ProcessFrames::Flush(CommandStruct* cmd) {
	return render.FlushRenderTarget(cmd);
}