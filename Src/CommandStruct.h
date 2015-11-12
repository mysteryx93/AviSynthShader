#pragma once
#include "d3d9.h"
#include "atlbase.h"
#include "D3D9Macros.h"
#include "avisynth.h"
#include <windows.h>

enum ParamType {
	None,
	Float,
	Int,
	Bool
};

struct ParamStruct {
	const char* String;
	float Value[4]; // Can hold Vector of 4 floats
	ParamType Type;
};

struct CommandStruct {
	byte CommandIndex;
	const char* Path;
	const char* EntryPoint;
	const char* ShaderModel;
	ParamStruct Param[9];
	byte ClipIndex[9];
	byte OutputIndex;
	int OutputWidth, OutputHeight;
};
