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
	float* Values;	// Array size must be divisible by 4.
	int Count;		// The quantity of Float4 vectors being set.
	ParamType Type;
};

struct CommandStruct {
	byte CommandIndex;
	const char* Path;
	const char* EntryPoint;
	const char* ShaderModel;
	ParamStruct Param[9];
	D3DXMACRO* Defines;
	byte ClipIndex[12];
	byte OutputIndex;
	int OutputWidth, OutputHeight;
	int Precision;
};
