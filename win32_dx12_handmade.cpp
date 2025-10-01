#include "includes.h"

#include "os_win32.cpp"
#include "array.cpp"

#include "renderer_dx12.cpp"

#include "render_entry.h"

#include <climits>

#define GAME_CODE_DLL "tanksgame.dll"

const u32 CLIENT_WIDTH = 1280;
const u32 CLIENT_HEIGHT = 720;

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
    bool isDown;
    bool wasDown;
} KeyInput;

typedef struct
{
    KeyInput W;
    KeyInput A;
    KeyInput S;
    KeyInput D;
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
                break;
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

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd)
{
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
    float aspect = 1.0f;

    Win32GameCode gameCode = Win32LoadGameCode(GAME_CODE_DLL);

    GameMemory gameMemory = {};
    gameMemory.permStorage = VirtualAlloc(0, MB(2), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    gameMemory.permStorageSize = MB(2);
    gameMemory.transientStorage = VirtualAlloc(0, MB(1), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    gameMemory.transStorageSize = MB(1);
    gameMemory.platformLoadTexture = &PlatformLoadTexture;

    InputState inputState = {};

    InitializeRenderer(windowHandle, false, CLIENT_WIDTH, CLIENT_HEIGHT);

    gameCode.Start(&gameMemory);

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

        // Check for resize.
        RECT currentClientRect;
        GetClientRect(windowHandle, &currentClientRect);
        vec2i resolution = {currentClientRect.right - currentClientRect.left, currentClientRect.bottom - currentClientRect.top};
        if (resolution.x != prevResolution.x || resolution.y != prevResolution.y)
        {
            // Resize Swap Chain Frame Buffers
            aspect = RendererResizeFramebuffers(resolution.x, resolution.y);
            prevResolution = resolution;
        }
        GameState * gameState = (GameState*)gameMemory.permStorage;
        gameState->tempInput.y = (float)inputState.W.isDown + -(float)(inputState.S.isDown); 
        gameState->tempInput.x = (float)inputState.D.isDown + -(float)(inputState.A.isDown); 

        gameCode.Update(&gameMemory, deltaTime);

        mat4 projectionMatrix = orthographicProjection(aspect, -aspect, 1.0f, -1.0f, -0.01f, 100.0f);

        ///////////////
        // Rendering
        RendererProcessPushBuffer(&gameState->renderPB);
        BeginFrame((float*)&gameState->clearColor, projectionMatrix);
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