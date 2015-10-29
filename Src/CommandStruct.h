#pragma once
#include "d3d9.h"
#include "atlbase.h"
#include "D3D9Macros.h"
#include "avisynth.h"
#include <windows.h>

struct CommandStruct {
	byte CommandIndex;
	const char* Path;
	const char* EntryPoint;
	const char* ShaderModel;
	const char* Param[9];
	byte ClipIndex[9];
	byte OutputIndex;
	int OutputWidth, OutputHeight;

	//DWORD* ShaderBuffer;
	//ID3DXBuffer* ShaderBufferDX;
	//IDirect3DPixelShader9* Shader;
	//ID3DXConstantTable* ConstantTable;
};