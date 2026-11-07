#pragma once

#include <windows.h>

#include <stdexcept>

inline void CheckHRESULT(HRESULT result, const char* message)
{
    if (FAILED(result))
        throw std::runtime_error(message);
}
