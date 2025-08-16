#include "includes.h"
#include "render.cpp"

const u8 NUM_FRAMES = 3;
bool USE_WARP = false;
u32 CLIENT_WIDTH = 1280;
u32 CLIENT_HEIGHT = 720;

bool IS_INITIALIZED = false;
bool RUNNING = false;

#define _DEBUG


LRESULT mainWindowCallback(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (msg)
    {
        case WM_SIZE:
            OutputDebugStringA("WM_SIZE\n");
            break;
        case WM_DESTROY:
            OutputDebugStringA("WM_DESTROY\n");
            break;
        case WM_CLOSE:
            OutputDebugStringA("WM_CLOSE\n");
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

HANDLE createEventHandle()
{
    HANDLE fenceEvent;
    fenceEvent = CreateEvent(0, FALSE, FALSE, 0);
    return fenceEvent;
}

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd)
{
    WNDCLASS windowClass = {};

    windowClass.style = CS_VREDRAW | CS_HREDRAW;
    windowClass.lpfnWndProc = &mainWindowCallback;
    windowClass.hInstance = hInstance;
    windowClass.lpszClassName = L"Cool Window Class";

    if (!RegisterClass(&windowClass)) { return -1; }

    HWND windowHandle = CreateWindowEx(0, windowClass.lpszClassName,
                    L"TANKS!",
                    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                    CW_USEDEFAULT,
                    CW_USEDEFAULT,
                    CW_USEDEFAULT,
                    CW_USEDEFAULT,
                    0, 0, hInstance, 0);

    if (!windowHandle) { return -1; }
    
    EnableDebugLayer();

    ComPtr<ID3D12Resource> backBuffers[NUM_FRAMES];

    ComPtr<IDXGIAdapter4> adapter = GetAdapter(false);
    ComPtr<ID3D12Device2> device = CreateDevice(adapter);
    ComPtr<ID3D12CommandQueue> cmdQueue = CreateCommandQueue(device, D3D12_COMMAND_LIST_TYPE_DIRECT);

    bool tearingSupported = CheckTearingSupported();

    ComPtr<IDXGISwapChain4> swapChain = CreateSwapChain(windowHandle, cmdQueue, 1920, 1080, 3);

    RUNNING = true;

    while (RUNNING)
    {
        MSG msg;
        BOOL msgResult = GetMessage(&msg, 0, 0, 0);
        if (msgResult > 0)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            break;
        }

    }

    return 0;
}