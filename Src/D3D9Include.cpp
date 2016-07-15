#include "D3D9Include.h"

#pragma warning(disable: 4996)

HRESULT D3D9Include::Open(D3DXINCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID * ppData, UINT * pBytes)
{
    *ppData = GetResource(pFileName, pBytes, false);
    return ppData ? S_OK : E_FAIL;
}

HRESULT D3D9Include::Close(LPCVOID pData)
{
    if (pData) {
        // Only close if a file was opened; resources cannot be closed.
        for (auto const& item : m_OpenedFiles) {
            if (item == pData) {
                free((void*)pData);
                m_OpenedFiles.erase(std::remove(m_OpenedFiles.begin(), m_OpenedFiles.end(), pData), m_OpenedFiles.end());
                break;
            }
        }
    }
    return S_OK;
}

LPCVOID D3D9Include::GetResource(std::string filePath, UINT* fileLength, bool resource) {
    CComPtr<ID3DXBuffer> code;
    LPCVOID ShaderBuf = nullptr;
    DWORD* CodeBuffer = nullptr;

	if (!resource) {
		ShaderBuf = ReadBinaryFile(filePath, fileLength);
		if (!ShaderBuf) {
			// Try in same folder as DLL file.
			char path[MAX_PATH];
			GetDefaultPath(path, MAX_PATH, filePath.c_str());
			ShaderBuf = ReadBinaryFile(path, fileLength);
		}
		if (ShaderBuf) {
			// Only direct files need to be closed; not resource files.
			m_OpenedFiles.push_back(ShaderBuf);
		}
	}
    if (!ShaderBuf) {
        // Try resource file.
        std::string prefix = "./"; // Remove trailing ./ prefix
        if (filePath.substr(0, prefix.size()) == prefix) {
            filePath = filePath.substr(prefix.size());
        }
        static HMODULE hModule;
		if (hModule == NULL)
			GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&StaticFunction, &hModule);
        HRSRC hRes = FindResource(hModule, ("IDR_RCDATA_" + std::string(filePath)).c_str(), RT_RCDATA);
        *fileLength = SizeofResource(hModule, hRes);
        HGLOBAL hResData = LoadResource(hModule, hRes);
        ShaderBuf = (char*)LockResource(hResData);
    }
    return ShaderBuf;
}

LPCVOID D3D9Include::ReadBinaryFile(std::string filePath, UINT* fileLength) {
    FILE *fl = fopen(filePath.c_str(), "rb");
    if (fl)
    {
        fseek(fl, 0, SEEK_END);
        *fileLength = (UINT)ftell(fl);
        LPCVOID ret = (LPCVOID)malloc(*fileLength);
        fseek(fl, 0, SEEK_SET);
        fread((void*)ret, 1, *fileLength, fl);
        fclose(fl);
        return ret;
    }
    else
        return nullptr;
}

// Gets the path where the DLL file is located.
void D3D9Include::GetDefaultPath(char* outPath, int maxSize, const char* filePath)
{
    HMODULE hm = nullptr;

    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)&StaticFunction,
        &hm))
    {
        int ret = GetLastError();
        fprintf(stderr, "GetModuleHandle returned %d\n", ret);
    }
    GetModuleFileNameA(hm, outPath, maxSize);

    // Strip the file name to keep the path ending with '\'
    char *pos = strrchr(outPath, '\\') + 1;
    if (pos)
        strcpy(pos, filePath);
}
