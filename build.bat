@echo off
if not exist build (
	mkdir build
)
mkdir build
pushd build
cl /std:c++20 -Zi ../win32_dx12_handmade.cpp /I ..\include\directx user32.lib dxgi.lib d3d12.lib xinput.lib d3dcompiler.lib /Fe: tanks.exe
popd