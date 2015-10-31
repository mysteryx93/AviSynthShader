#pragma once
#include "CommandStruct.h"
#include "D3D9RenderImpl.h"

class ProcessFrames {
public:
	ProcessFrames(IScriptEnvironment* env);
	~ProcessFrames();
	HRESULT Execute(CommandStruct* cmd, CommandStruct* previousCmd);
	HRESULT Flush(CommandStruct* cmd);

	const int m_precision;
	PClip m_clips[9];
	HWND m_dummyHWND;
	short m_nextShaderId = 0;
	D3D9RenderImpl render;
};
