@echo off

set BUILD_TYPE=debug
if /I "%1"=="release" (
    set BUILD_TYPE=release
)

if not exist build (
	mkdir build
)

if "%BUILD_TYPE%"=="debug" (
    set CFLAGS=/std:c++20 /Zi /Od
	set DLL_FLAGS=/LDd
) else (
    set CFLAGS=/std:c++20 /O2 /Ot
	set DLL_FLAGS=/LD
)

set ENGINE_INCLUDES=/I ..\include\directx /I ..\include\stb /I ..\include\portal
set ENGINE_LIBS=user32.lib dxgi.lib d3d12.lib xinput.lib d3dcompiler.lib ws2_32.lib portal.lib shell32.lib

pushd build
cl %CFLAGS% %ENGINE_INCLUDES% ../win32_dx12_handmade.cpp  %ENGINE_LIBS% /Fe:tanks.exe /link /LIBPATH:..\lib
cl %CFLAGS% %DLL_FLAGS% ../game/tanks.cpp /Fe:tanksgame.dll
popd

echo Build Complete: (%BUILD_TYPE% mode)
