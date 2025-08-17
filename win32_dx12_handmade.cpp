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

void Render(ComPtr<ID3D12CommandAllocator> * cmdAllocators, u64 currentBackBufferIndex, ComPtr<ID3D12GraphicsCommandList> cmdList, ComPtr<ID3D12Resource> * backBuffers, ID3D12DescriptorHeap * descHeap, UINT rtvDescSize)
{
    ComPtr<ID3D12CommandAllocator> cmdAllocator = cmdAllocators[currentBackBufferIndex];
    ComPtr<ID3D12Resource> backBuffer = backBuffers[currentBackBufferIndex];

    cmdAllocator->Reset();
    cmdList->Reset(cmdAllocator.Get(), nullptr);

    // Begin Frame: Transition RTV to render target state
    D3D12_RESOURCE_BARRIER barrierTarget = CreateTransitionBarrier(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmdList->ResourceBarrier(1, &barrierTarget);

    // Clear Render Target
    D3D12_CPU_DESCRIPTOR_HANDLE rtvDescHandle = descHeap->GetCPUDescriptorHandleForHeapStart();
    rtvDescHandle.ptr += (currentBackBufferIndex * rtvDescSize);
    float clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
    // TODO(rordon): set render target
    cmdList->ClearRenderTargetView(rtvDescHandle, clearColor, 0, nullptr);

    // End Frame: Transition RTV to present state.
    D3D12_RESOURCE_BARRIER barrierPresent = CreateTransitionBarrier(backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    cmdList->ResourceBarrier(1, &barrierTarget);
    AssertIfFailed(cmdList->Close());

    ID3D12CommandList * const cmdLists[] = { cmdList.Get() };
    cmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

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
    ComPtr<ID3D12DescriptorHeap> rtvDescHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, NUM_FRAMES);
    UINT rtvDescSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();
    UpdateRenderTargetViews(device, swapChain, rtvDescHeap, NUM_FRAMES);
    for (int i = 0; i < NUM_FRAMES; i++)
    {
        cmdAllocators[i] = CreateCommandAllocator(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
    }
    ComPtr<ID3D12GraphicsCommandList> cmdList = CreateCommandList(device, cmdAllocators[backBufferIndex], D3D12_COMMAND_LIST_TYPE_DIRECT);

    RUNNING = true;

    // Profiling
    LARGE_INTEGER perfFrequencyResult;
    u64 perfFrequency;
    QueryPerformanceFrequency(&perfFrequencyResult);
    perfFrequency = perfFrequencyResult.QuadPart;
    LARGE_INTEGER lastCounter;
    //----------------------------

    while (RUNNING)
    {
        QueryPerformanceCounter(&lastCounter);

        MSG msg;
        BOOL msgResult = GetMessage(&msg, 0, 0, 0);
        if (msgResult > 0)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            //Render(ComPtr<ID3D12CommandAllocator> * cmdAllocators, index, ComPtr<ID3D12GraphicsCommandList> cmdList, ComPtr<ID3D12Resource> * backBuffers);
        }
        else
        {
            break;
        }

        // Profiling
        LARGE_INTEGER endCounter;
        QueryPerformanceCounter(&endCounter);
        u64 counterElapsed = endCounter.QuadPart - lastCounter.QuadPart;
        u64 msPerFrame = (1000 * counterElapsed) / perfFrequency;
        lastCounter = endCounter;
        // ---------------------------------
    }

    return 0;
}