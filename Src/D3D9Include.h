#pragma once

#include "d3d9.h"
#include "atlbase.h"
#include "D3D9Macros.h"
#include "avisynth.h"
#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>

class D3D9Include : public ID3DXInclude {
public:
    HRESULT __stdcall Open(D3DXINCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes);
    HRESULT __stdcall Close(LPCVOID pData);
    LPCVOID GetResource(std::string filePath, UINT* fileLength, bool searchFile);
private:
    LPCVOID ReadBinaryFile(std::string filePath, UINT* fileLength);
    void GetDefaultPath(char* outPath, int maxSize, const char* filePath);
    static void StaticFunction() {}; // needed by GetDefaultPath
    std::vector<LPCVOID> m_OpenedFiles; // list of files that need to be closed
};