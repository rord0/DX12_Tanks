#include "array.h"
#include "core.h"
#include "includes.h"

#include "os_win32.cpp"
#include "array.cpp"
#include "arena.cpp"
#include "ring_buffer.cpp"

#include "renderer_dx12.cpp"
#include "networking.cpp"
#include "./engine/fonts.cpp"

#include "render_entry.h"
#include "./engine/fonts.hpp"
#include <cstddef>
#include <cstdio>

#define GAME_CODE_DLL "tanksgame.dll"

const u32 CLIENT_WIDTH = 1920;
const u32 CLIENT_HEIGHT = 1080;

bool RUNNING = false;

mat4 orthographicProjection(float right, float left, float top, float bottom, float n, float f)
{
    mat4 m = {};
    m.m[0][0] = 2.0f / (right - left);
    m.m[1][1] = 2.0f / (top - bottom);
    m.m[2][2] = 2.0f / (f - n);

    m.m[0][3] = -((right + left)/(right - left));
    m.m[1][3] = -((top + bottom)/(top - bottom));
    m.m[2][3] = -((f + n)/(f - n));

    m.m[3][3] = 1.0f;
    return m;
}                      

typedef struct
{
    KeyInput W;
    KeyInput A;
    KeyInput S;
    KeyInput D;
    KeyInput UP;
    KeyInput DOWN;
    KeyInput LEFT;
    KeyInput RIGHT;
    KeyInput ESC;
	KeyInput SPACE;
	KeyInput ENTER;
    KeyInput mouseL;
	vec2i mousePos;
} InputState;

void win32ProcessPendingMessages(HWND windowHandle, InputState & inputState)
{
    MSG msg = {};
    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
    {
        switch (msg.message)
        {
            case WM_KEYDOWN:
                if (msg.wParam == 'W')
                {
                    inputState.W.isDown = true;
                    inputState.W.wasDown = (msg.lParam & (1 << 30)) != 0;
                }
                if (msg.wParam == 'S')
                {
                    inputState.S.isDown = true;
                    inputState.S.wasDown = (msg.lParam & (1 << 30)) != 0;
                }
                if (msg.wParam == 'A')
                {
                    inputState.A.isDown = true;
                    inputState.A.wasDown = (msg.lParam & (1 << 30)) != 0;
                }
                if (msg.wParam == 'D')
                {
                    inputState.D.isDown = true;
                    inputState.D.wasDown = (msg.lParam & (1 << 30)) != 0;
                }
                if (msg.wParam == VK_UP)
                {
                    inputState.UP.isDown = true;
                    inputState.UP.wasDown = (msg.lParam & (1 << 30)) != 0;
                }
                if (msg.wParam == VK_DOWN)
                {
                    inputState.DOWN.isDown = true;
                    inputState.DOWN.wasDown = (msg.lParam & (1 << 30)) != 0;
                }
                if (msg.wParam == VK_LEFT)
                {
                    inputState.LEFT.isDown = true;
                    inputState.RIGHT.wasDown = (msg.lParam & (1 << 30)) != 0;
                }
                if (msg.wParam == VK_RIGHT)
                {
                    inputState.RIGHT.isDown = true;
                    inputState.RIGHT.wasDown = (msg.lParam & (1 << 30)) != 0;
                }
                if (msg.wParam == VK_ESCAPE)
                {
                    inputState.ESC.isDown = true;
                    inputState.ESC.wasDown = (msg.lParam & (1 << 30)) != 0;
                }
                if (msg.wParam == VK_SPACE)
                {
                    inputState.SPACE.isDown = true;
                    inputState.SPACE.wasDown = (msg.lParam & (1 << 30)) != 0;
                }
                if (msg.wParam == VK_RETURN)
                {
                    inputState.ENTER.isDown = true;
                    inputState.ENTER.wasDown = (msg.lParam & (1 << 30)) != 0;
                }
                break;
            case WM_KEYUP:
                if (msg.wParam == 'W')
                {
                    inputState.W.isDown = false;
                    inputState.W.wasDown = true;
                }
                if (msg.wParam == 'S')
                {
                    inputState.S.isDown = false;
                    inputState.S.wasDown = true;
                }
                if (msg.wParam == 'A')
                {
                    inputState.A.isDown = false;
                    inputState.A.wasDown = true;
                }
                if (msg.wParam == 'D')
                {
                    inputState.D.isDown = false;
                    inputState.D.wasDown = true;
                }
                if (msg.wParam == VK_UP)
                {
                    inputState.UP.isDown = false;
                    inputState.UP.wasDown = true;
                }
                if (msg.wParam == VK_DOWN)
                {
                    inputState.DOWN.isDown = false;
                    inputState.DOWN.wasDown = true;
                }
                if (msg.wParam == VK_LEFT)
                {
                    inputState.LEFT.isDown = false;
                    inputState.RIGHT.wasDown = true; 
                }
                if (msg.wParam == VK_RIGHT)
                {
                    inputState.RIGHT.isDown = false;
                    inputState.RIGHT.wasDown = true;
                }
                if (msg.wParam == VK_SPACE)
                {
                    inputState.SPACE.isDown = false;
                    inputState.SPACE.wasDown = true;
                }
                if (msg.wParam == VK_RETURN)
                {
                    inputState.ENTER.isDown = false;
                    inputState.ENTER.wasDown = true;
                }
                break;
			case WM_LBUTTONDOWN:
				{
					inputState.mouseL.isDown = true;
					OutputDebugStringA("WM_LBUTTONDOWN\n");
				}break;
			case WM_LBUTTONUP:
				{
					inputState.mouseL.isDown = false;
				}break;
			case WM_MOUSEMOVE:
				{
					int xPos = GET_X_LPARAM(msg.lParam);
					int yPos = GET_Y_LPARAM(msg.lParam);
					inputState.mousePos.x = xPos;
					inputState.mousePos.y = yPos;
				}break;
            default:
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                break;
        }
    }
}


GAME_UPDATE_FUNCTION(GameUpdateFunctionStub)
{
    // Do nothing...
}

GAME_START_FUNCTION(GameStartFunctionStub)
{
    // Do nothing...
}


FILETIME Win32GetLastFileWriteTime(const char * filename)
{
    FILETIME lastWriteTime = {};

    WIN32_FILE_ATTRIBUTE_DATA fileAttributeData = {};
    if (GetFileAttributesExA(filename, GetFileExInfoStandard, &fileAttributeData))
    {
        lastWriteTime = fileAttributeData.ftLastWriteTime;
    }

    return lastWriteTime;
}

Win32GameCode Win32LoadGameCode(const char * filename)
{
    Win32GameCode result = {};

    // Create Copy and load DLL
    const char * tempDLLName = "temp_game_code.dll";
    CopyFileA(filename, tempDLLName, false);
    result.DLL = LoadLibraryA(tempDLLName);

    if (result.DLL)
    {
        result.Update = (GameUpdateFunction*)GetProcAddress(result.DLL, "update");
        result.Start = (GameStartFunction*)GetProcAddress(result.DLL, "start");
        result.isValid = result.Update;
        result.lastWriteTime = Win32GetLastFileWriteTime(filename);
    }

    if (!result.isValid)
    {
        result.Update = GameUpdateFunctionStub;
        result.Start = GameStartFunctionStub;
    }

    return result;
}

void Win32UnloadGameCode(Win32GameCode * gameCode)
{
    if (gameCode->DLL)
    {
        FreeLibrary(gameCode->DLL);
    }
    gameCode->isValid = false;   
    gameCode->Update = GameUpdateFunctionStub;
    gameCode->Start = GameStartFunctionStub;
}


LRESULT mainWindowCallback(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (msg)
    {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(window, &ps);
            // NOTE: Do nothing here (Rendering happens in the render loop).
            OutputDebugStringA("WM_PAINT\n");
            EndPaint(window, &ps);
        } break;
        case WM_DESTROY:
            OutputDebugStringA("WM_DESTROY\n");
            break;
        case WM_CLOSE:
            OutputDebugStringA("WM_CLOSE\n");
            RUNNING = false;
            break;
        case WM_ENTERSIZEMOVE:
            OutputDebugStringA("WM_ENTERSIZEMOVE\n");
            break;
        case WM_EXITSIZEMOVE:
            OutputDebugStringA("WM_EXITSIZEMOVE\n");
            break;
        case WM_ACTIVATEAPP:
            OutputDebugStringA("WM_ACTIVATEAPP\n");
            break; 
        default:
            result = DefWindowProc(window, msg, wParam, lParam);
            break;
    }

    return result;
}

PLATFORM_LOAD_TEXTURE(PlatformLoadTexture)
{
    ImageData image = LoadImageFromFile(textureName);
    int textureHandle = -1;
    if (image.memory)
    {
        textureHandle = RendererCreateTexture(&image);
        stbi_image_free(image.memory);
    }

    return textureHandle;
}

PLATFORM_LOAD_FONT_ATLAS(PlatformLoadFontAtlas)
{
	return LoadFontAtlas(metadataPath, atlasPath);
}

void GetCLIArguments(int * argc, char *** argv)
{
	int arg_count;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &arg_count);

    *argc = arg_count;
    *argv = new char*[arg_count];

    for (int i = 0; i < arg_count; i++)
    {
        int len = wcstombs(nullptr, wargv[i], 0) + 1;
        (*argv)[i] = new char[len];
        wcstombs((*argv)[i], wargv[i], len);
    }

    LocalFree(wargv);
}

RendererPushBuffer PushBufferCreate(size_t bufferSize, u32 maxSortEntries)
{
	RendererPushBuffer pb = {0};
    pb.memory = (u8*)PlatformAlloc(bufferSize);
    pb.size = bufferSize;
    pb.maxSortEntries = maxSortEntries;
    pb.sortEntries = (RenderSortEntry*)PlatformAlloc(maxSortEntries * sizeof(RenderSortEntry));
	return pb;
}


int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd)
{
    // Convert to char**
	int argc = 0;
    char** argv = NULL;
	GetCLIArguments(&argc, &argv);

	if (AttachConsole(ATTACH_PARENT_PROCESS))
	{
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
	}

    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASS windowClass = {};

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    RECT windowRect = {0, 0, (LONG)CLIENT_WIDTH, (LONG)CLIENT_HEIGHT};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
    
    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;
    int windowX = std::max<int>(0,  (screenWidth - windowWidth)  / 2);
    int windowY = std::max<int>(0, (screenHeight - windowHeight) / 2);

    windowClass.style = CS_VREDRAW | CS_HREDRAW;
    windowClass.lpfnWndProc = &mainWindowCallback;
    windowClass.hInstance = hInstance;
    windowClass.lpszClassName = L"Cool Window Class";
    windowClass.hbrBackground = nullptr;
    windowClass.hCursor = LoadCursor(0, IDC_CROSS);

    if (!RegisterClass(&windowClass)) { return -1; }

    HWND windowHandle = CreateWindowEx(0, windowClass.lpszClassName,
                    L"TANKS!",
                    WS_OVERLAPPEDWINDOW,
                    windowX,
                    windowY,
                    windowWidth,
                    windowHeight,
                    0, 0, hInstance, 0);

    if (!windowHandle) { return -1; }

    bool contentLoaded = false;

    // -----------------------------------
    //             Profiling
    LARGE_INTEGER perfFrequencyResult;
    long long perfFrequency;
    QueryPerformanceFrequency(&perfFrequencyResult);
    perfFrequency = perfFrequencyResult.QuadPart;
    LARGE_INTEGER lastFrameStartCounter;
    QueryPerformanceCounter(&lastFrameStartCounter);
    // ----------------------------

    double time = 0.0f;
    double timer = 0.0f;
    vec2i prevResolution = {};

    Win32GameCode gameCode = Win32LoadGameCode(GAME_CODE_DLL);

    GameMemory gameMemory = {};
    gameMemory.permStorage = PlatformAlloc(MB(2));
    gameMemory.permStorageSize = MB(2);
    gameMemory.transientStorage = PlatformAlloc(MB(2));
    gameMemory.transStorageSize = MB(2);

    gameMemory.platform.platformLoadTexture = &PlatformLoadTexture;
    gameMemory.platform.platformLoadFile    = &DEBUG_PlatformReadEntireFile;
    gameMemory.platform.platformFreeFile    = &DEBUG_PlatformFreeFileMemory;
	gameMemory.platform.platformStartServer = &PlatformStartServer;
	gameMemory.platform.platformStartClient = &PlatformStartClient;
	gameMemory.platform.platformClientSend  = &PlatformClientSend;
	gameMemory.platform.platformServerSend  = &PlatformServerSend;
	gameMemory.platform.serverGetEvent      = &PlatformServerGetEvent;
	gameMemory.platform.loadFont 			= &PlatformLoadFontAtlas;

	GameInput gameInput = {0};

    InputState inputState = {};

	RendererPushBuffer pushBuffer   = PushBufferCreate(MB(1), 8096);
	RendererPushBuffer uiPushBuffer = PushBufferCreate(MB(1), 8096);

	InitializeFonts();
    InitializeRenderer(windowHandle, false, CLIENT_WIDTH, CLIENT_HEIGHT);
	InitializeNetworking();

    gameCode.Start(&gameMemory, argc, argv);

    ShowWindow(windowHandle, SW_SHOW);
    //----------------------
    // Main Loop           |
    // ---------------------
    RUNNING = true;
    while (RUNNING)
    {
        // Profiling
        LARGE_INTEGER endCounter;
        QueryPerformanceCounter(&endCounter);
        int64_t counterElapsed = endCounter.QuadPart - lastFrameStartCounter.QuadPart;

        u32 msPerFrame = (1000 * counterElapsed) / perfFrequency;
        double deltaTime = (double)(counterElapsed) / (double)perfFrequency;
        time += deltaTime;
        timer += deltaTime;

        u32 FPS = perfFrequency / counterElapsed;
        lastFrameStartCounter = endCounter;
        QueryPerformanceCounter(&lastFrameStartCounter);

        // Check for dll import
        FILETIME newDLLWriteTime = Win32GetLastFileWriteTime(GAME_CODE_DLL);
        if (CompareFileTime(&newDLLWriteTime, &gameCode.lastWriteTime))
        {
            Win32UnloadGameCode(&gameCode);
            gameCode = Win32LoadGameCode(GAME_CODE_DLL);
        }

        // Update
        win32ProcessPendingMessages(windowHandle, inputState);
		NetworkingUpdate(&gameInput, time);

        // Check for resize.
        RECT currentClientRect;
        GetClientRect(windowHandle, &currentClientRect);
        vec2i resolution = {currentClientRect.right - currentClientRect.left, currentClientRect.bottom - currentClientRect.top};
        if (resolution.x != prevResolution.x || resolution.y != prevResolution.y)
        {
            // Resize Swap Chain Frame Buffers
            RendererResizeFramebuffers(resolution.x, resolution.y);
            prevResolution = resolution;
        }

		gameInput.deltaTime = deltaTime;
        gameInput.WASD[0]  = inputState.W;
        gameInput.WASD[1]  = inputState.A;
        gameInput.WASD[2]  = inputState.S;
        gameInput.WASD[3]  = inputState.D;
        gameInput.ARROWS[0]  = inputState.UP;
        gameInput.ARROWS[1]  = inputState.LEFT;
        gameInput.ARROWS[2]  = inputState.DOWN;
        gameInput.ARROWS[3]  = inputState.RIGHT;
		gameInput.isSpacePressed = inputState.SPACE.isDown;
		gameInput.isEnterPressed = inputState.ENTER.isDown;
		gameInput.isMousePressed = inputState.mouseL.isDown;
		gameInput.viewportSize = vec2i{resolution.x, resolution.y};
		gameInput.mousePosVP = vec2i{inputState.mousePos.x, inputState.mousePos.y};

        gameCode.Update(&gameMemory, &gameInput, &pushBuffer, &uiPushBuffer);

		if (inputState.ESC.isDown)
		{
			RUNNING = false;
		}

        ///////////////
        // Rendering
        RendererProcessPushBuffer(&pushBuffer);
        RendererProcessPushBuffer(&uiPushBuffer);
        BeginFrame();
        Render();
        EndFrame();
        
        char buffer[256];
        snprintf(buffer, 256, "MS/Frame: %dms FPS: %d Time: %lf\n", msPerFrame, FPS, time);
        if (timer>0.5f)
        {
            OutputDebugStringA(buffer);
            timer = 0.0f;
        }
        SetWindowTextA(windowHandle, buffer);
        // ---------------------------------
    }

    return 0;
}
