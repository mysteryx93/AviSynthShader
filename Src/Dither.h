#pragma once
#include "d3d9.h"
#include "D3D9Macros.h"
#include "avisynth.h"
#include "TextureList.h"
#include "CommandStruct.h"
#include "D3D9RenderImpl.h"

const int DITHER_MATRIX_SIZE = 16;
extern const unsigned short DITHER_MATRIX[DITHER_MATRIX_SIZE][DITHER_MATRIX_SIZE];

HRESULT __stdcall CopyDitherMatrixToSurface(InputTexture* dst, IScriptEnvironment* env);
HRESULT __stdcall CreateDitherCommand(D3D9RenderImpl* render, std::vector<InputTexture*>* textureList, CommandStruct* cmd, int commandIndex, int outputPrecision);
