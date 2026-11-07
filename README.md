# AtmosDX12

A lightweight atmospheric render engine built with **C++20, Direct3D 12, and HLSL**.

## Requirements

- Windows 10/11 and a Direct3D 12-capable hardware GPU
- Visual Studio C++ Build Tools (MSVC) and Windows SDK
- CMake 3.21 or newer

## Build and run

From a **Developer PowerShell for Visual Studio** shell:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
.\build\Debug\AtmosDX12.exe
```

## References

- [Direct3D 12 documentation](https://learn.microsoft.com/en-us/windows/win32/direct3d12/direct3d-12-graphics)
- [Microsoft D3D12 samples](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12HelloWorld)
