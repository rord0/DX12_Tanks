@echo off

set BUILD_TYPE=debug
if /I "%1"=="release" (
    set BUILD_TYPE=release
)

if not exist build (
	mkdir build
)

if "%BUILD_TYPE%"=="debug" (
    set CFLAGS=/std:c++20 /Zi /Od /MDd
	set DLL_FLAGS=/LDd
    set PORTAL_LIB=portald.lib
) else (
    set CFLAGS=/std:c++20 /O2 /Ot /MD
	set DLL_FLAGS=/LD
    set PORTAL_LIB=portal.lib
)

set ENGINE_INCLUDES=/I ..\include\directx /I ..\include\stb /I ..\include\portal /I ..\engine\third-party\cJSON /I ..\engine\third-party\imgui-1.92.9b
set GAME_INCLUDES=/I ..\engine\third-party\imgui-1.92.9b
set ENGINE_LIBS=user32.lib dxgi.lib d3d12.lib xinput.lib d3dcompiler.lib ws2_32.lib %PORTAL_LIB% shell32.lib

pushd build
cl %CFLAGS% %ENGINE_INCLUDES% ../win32_dx12_handmade.cpp  %ENGINE_LIBS% /Fe:tanks.exe /link /LIBPATH:..\lib
cl %CFLAGS% %GAME_INCLUDES% %DLL_FLAGS% ..\game\tanks.cpp /link /out:tanksgame.dll
popd

echo Build Complete: (%BUILD_TYPE% mode)
