#pragma once
#include "d3d9.h"
#include "atlbase.h"
#include "D3D9Macros.h"
#include "avisynth.h"
#include <windows.h>

struct FrameRef {
	FrameRef::FrameRef() {
		Width = 0, Height = 0, Pitch = 0;
	}

	FrameRef::FrameRef(PVideoFrame clip, byte* buffer, int precision) {
		Width = clip->GetRowSize() / precision / 4;
		Height = clip->GetHeight();
		Pitch = clip->GetPitch();
		Buffer = buffer;
	}

	int Width, Height, Pitch;
	byte* Buffer;
};

struct CommandStruct {
	const char* Path;
	const char* EntryPoint;
	const char* ShaderModel;
	const char* Param[9];
	FrameRef Input[9];
	FrameRef Output;
	HANDLE Event;
};