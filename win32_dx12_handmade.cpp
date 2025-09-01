#include "includes.h"
#include "render.cpp"

bool USE_WARP = false;
u32 CLIENT_WIDTH = 1280;
u32 CLIENT_HEIGHT = 720;

bool RUNNING = false;
RendererState RENDERER_STATE = {};

void Resize(u32 width, u32 height, RendererState & state)
{
    if (CLIENT_WIDTH != width || CLIENT_HEIGHT != height)
    {
        CLIENT_WIDTH = std::max(1u, width);
        CLIENT_HEIGHT = std::max(1u, height);
        Flush(state.cmdQueue, state.fence, &state.fenceValue, state.fenceEvent);
        for (int i = 0; i < NUM_FRAMES; i++)
        {
            state.backBuffers[i].Reset();
            state.fenceValues[i] = state.fenceValues[state.backBufferIndex];
        }

        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        AssertIfFailed(state.swapChain->GetDesc(&swapChainDesc));
        AssertIfFailed(state.swapChain->ResizeBuffers(NUM_FRAMES, CLIENT_WIDTH, CLIENT_HEIGHT, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

        state.backBufferIndex = state.swapChain->GetCurrentBackBufferIndex();

        UpdateRenderTargetViews(state.device, state.swapChain, state.rtvDescHeap, state.backBuffers, NUM_FRAMES);
    }
}

typedef struct 
{
    float r, g, b, a;
} ColorRGBA;

ColorRGBA HSVtoRGBA(float h, float s, float v)
{
    float c = v * s;
    float x = c * (1 - fabs(fmod(h / 60.0f, 2) - 1));
    float m = v - c;

    float r, g, b;
    if (h < 60)      { r = c; g = x; b = 0; }
    else if (h < 120){ r = x; g = c; b = 0; }
    else if (h < 180){ r = 0; g = c; b = x; }
    else if (h < 240){ r = 0; g = x; b = c; }
    else if (h < 300){ r = x; g = 0; b = c; }
    else             { r = c; g = 0; b = x; }

    return { r + m, g + m, b + m, 1.0f};
}

ColorRGBA GetHSVSpectrumColor(float time, float speed = 1.0f)
{
    float hue = fmod(time * speed * 60.0f, 360.0f);
    return HSVtoRGBA(hue, 1.0f, 1.0f);
}

void win32ProcessPendingMessages(HWND windowHandle)
{
    MSG msg = {};
    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
    {
        switch (msg.message)
        {
            default:
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                break;
        }
    }
}

LRESULT mainWindowCallback(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (msg)
    {
        case WM_SIZE:
        {
            OutputDebugStringA("WM_SIZE\n");
            RECT clientRect = {};
            GetClientRect(window, &clientRect);
            int width = clientRect.right - clientRect.left;
            int height = clientRect.bottom - clientRect.top;
            Resize(width, height, RENDERER_STATE);
        } break;
        case WM_PAINT:
        {
                PAINTSTRUCT ps;
                BeginPaint(window, &ps);
                // NOTE: Do nothing here (Rendering happens in the render loop).
                EndPaint(window, &ps);
        }break;
        case WM_DESTROY:
            OutputDebugStringA("WM_DESTROY\n");
            break;
        case WM_CLOSE:
            OutputDebugStringA("WM_CLOSE\n");
            RUNNING = false;
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

void BeginFrame(RendererState& state, float clearColor[4])
{
    ComPtr<ID3D12CommandAllocator> cmdAllocator = state.cmdAllocators[state.backBufferIndex];
    ComPtr<ID3D12Resource> backBuffer = state.backBuffers[state.backBufferIndex];
    cmdAllocator->Reset();
    state.cmdList->Reset(cmdAllocator.Get(), nullptr);

    // Transition Render Target View to render target state.
    D3D12_RESOURCE_BARRIER barrier = CreateTransitionBarrier(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    state.cmdList->ResourceBarrier(1, &barrier);

    // Clear Render Target
    D3D12_CPU_DESCRIPTOR_HANDLE rtvDescHandle = state.rtvDescHeap->GetCPUDescriptorHandleForHeapStart();
    rtvDescHandle.ptr += (state.backBufferIndex * state.rtvDescSize);
    state.cmdList->ClearRenderTargetView(rtvDescHandle, clearColor, 0, nullptr);
    // Clear Depth Buffer
}

void EndFrame(RendererState& state)
{
    // End Frame: Transition RTV to present state.
    ComPtr<ID3D12Resource> backBuffer = state.backBuffers[state.backBufferIndex];
    D3D12_RESOURCE_BARRIER barrier = CreateTransitionBarrier(backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    state.cmdList->ResourceBarrier(1, &barrier);

    // Close and Execute Command list
    ExecuteCommandList(state.cmdQueue, state.cmdList.Get());
    
    // Present Frame
    UINT syncInterval = state.vSyncEnabled ? 1 : 0;
    UINT presentFlags = state.tearingSupported && !state.vSyncEnabled ? DXGI_PRESENT_ALLOW_TEARING : 0;
    AssertIfFailed(state.swapChain->Present(syncInterval, presentFlags));

    state.fenceValues[state.backBufferIndex] = SignalCommandQueue(state.cmdQueue, state.fence, &state.fenceValue);
    state.backBufferIndex = state.swapChain->GetCurrentBackBufferIndex();
    WaitForFenceValue(state.fence, state.fenceValue, state.fenceEvent); 
}

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd)
{
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
                    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                    windowX,
                    windowY,
                    windowWidth,
                    windowHeight,
                    0, 0, hInstance, 0);

    if (!windowHandle) { return -1; }
    //                          DirectX12
    EnableDebugLayer();

    VertexPosColor vertices[3] = {{{-0.25, -0.25, 0.0}, {1.0, 0.0, 0.0}},
                                  {{0.25, -0.25, 0.0}, {0.0, 1.0, 0.0}},
                                  {{0.0, 0.25, 0.0}, {0.0, 0.0, 1.0}}};
    WORD indices[3] = {0,1,2};

    RENDERER_STATE = InitializeRenderer(windowHandle, USE_WARP, false, CLIENT_WIDTH, CLIENT_HEIGHT);
    UpdateRenderTargetViews(RENDERER_STATE.device, RENDERER_STATE.swapChain, RENDERER_STATE.rtvDescHeap, RENDERER_STATE.backBuffers, NUM_FRAMES);

    ComPtr<ID3D12Resource> vertexBuffer;
    ComPtr<ID3D12Resource> vertexUploadBuffer;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

    ComPtr<ID3D12Resource> indexBuffer;
    D3D12_INDEX_BUFFER_VIEW indexBufferView;

    CreateBufferResource(RENDERER_STATE.device, &vertexBuffer, &vertexUploadBuffer, sizeof(VertexPosColor) * 3);
    vertexBuffer->SetName(L"Vertex Buffer");
    vertexUploadBuffer->SetName(L"Vertex Upload Buffer");
    UpdateBufferResource(vertexUploadBuffer, vertexBuffer, RENDERER_STATE, sizeof(VertexPosColor) * 3, &vertices);

    ComPtr<ID3D12Resource> depthBuffer;
    ComPtr<ID3D12DescriptorHeap> dsvHeapDesc; // Depth Stencil View Heap Desciptor

    // Root Signature
    ComPtr<ID3D12RootSignature> rootSigature;

    // Pipeline state object
    ComPtr<ID3D12PipelineState> pipelineState;

    D3D12_VIEWPORT viewport;
    D3D12_RECT scissorRect;

    float modelMatirx[4][4];
    float viewMatrix[4][4];
    float projectionMatrix[4][4];

    float cameraFov;


    bool contentLoaded = false;
    // -----------------------------------

    //             Profiling
    LARGE_INTEGER perfFrequencyResult;
    long long perfFrequency;
    QueryPerformanceFrequency(&perfFrequencyResult);
    perfFrequency = perfFrequencyResult.QuadPart;
    LARGE_INTEGER lastCounter;
    // ----------------------------

    float clearColor[4] = {0.4f, 0.6f, 0.9f, 1.0f};

    double time = 0.0f;
    RUNNING = true;
    while (RUNNING)
    {
        QueryPerformanceCounter(&lastCounter);

        win32ProcessPendingMessages(windowHandle);

        ColorRGBA color = GetHSVSpectrumColor(time);
        clearColor[0]= color.r;
        clearColor[1] = color.g;
        clearColor[2] = color.b;

        BeginFrame(RENDERER_STATE, clearColor);
        EndFrame(RENDERER_STATE);
        // Profiling
        LARGE_INTEGER endCounter;
        QueryPerformanceCounter(&endCounter);
        int64_t counterElapsed = endCounter.QuadPart - lastCounter.QuadPart;
        u32 msPerFrame = (1000 * counterElapsed) / perfFrequency;
        double deltaTime = (double)(counterElapsed) / (double)perfFrequency;
        time += deltaTime;

        u32 FPS = perfFrequency / counterElapsed;
        lastCounter = endCounter;
        
        char buffer[256];
        snprintf(buffer, 256, "MS/Frame: %dms FPS: %d Time: %lf", msPerFrame, FPS, time);
        SetWindowTextA(windowHandle, buffer);
        // ---------------------------------
    }

    return 0;
}