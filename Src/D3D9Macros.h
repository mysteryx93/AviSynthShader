#pragma once

template <typename T>
inline void SafeRelease(T& p)
{
    if (NULL != p)
    {
        p.Release();
        p = NULL;
    }
}

#define HR(x) if(FAILED(x)) { return x; }
