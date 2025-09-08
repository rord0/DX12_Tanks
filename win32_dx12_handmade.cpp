#include "includes.h"
#include "render.cpp"
#include <climits>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

bool USE_WARP = false;
u32 CLIENT_WIDTH = 800;
u32 CLIENT_HEIGHT = 600;

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

        state.viewport.TopLeftX = 0;
        state.viewport.TopLeftY = 0;
        state.viewport.Width = CLIENT_WIDTH;
        state.viewport.Height = CLIENT_HEIGHT;
        state.viewport.MaxDepth = D3D12_MAX_DEPTH;
        state.viewport.MinDepth = D3D12_MIN_DEPTH;
        state.scissorRect.left = 0;
        state.scissorRect.top = 0;
        state.scissorRect.right = CLIENT_WIDTH;
        state.scissorRect.bottom = CLIENT_HEIGHT;
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

void DEBUG_PlatformFreeFileMemory(void ** memory)
{
    if (*memory)
    {
        VirtualFree(*memory, 0, MEM_RELEASE);
        *memory = NULL;
    }
}

DEBUG_FileResult DEBUG_PlatformReadEntireFile(const char * filenameASCII)
{
    DEBUG_FileResult result = {NULL, 0};
    HANDLE fileHandle = CreateFileA(filenameASCII, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);

    if (fileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER fileSize;
        if (GetFileSizeEx(fileHandle, &fileSize))
        {
            result.data = VirtualAlloc(0, fileSize.QuadPart, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            result.size = fileSize.QuadPart;
            if (result.data)
            {
                if (fileSize.QuadPart <= UINT_MAX) { /* TODO: assert here. */ }
                DWORD bytesRead = 0;
                if (ReadFile(fileHandle, result.data, fileSize.QuadPart, &bytesRead, 0) && (fileSize.QuadPart == bytesRead))
                {
                    // NOTE: File read successfully.
                }
                else
                {
                    DEBUG_PlatformFreeFileMemory(&result.data);
                }
            }
        }
    }

    CloseHandle(fileHandle);
    return result;
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
    state.cmdList->OMSetRenderTargets(1, &rtvDescHandle, FALSE, 0);
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
                    WS_OVERLAPPEDWINDOW,
                    windowX,
                    windowY,
                    windowWidth,
                    windowHeight,
                    0, 0, hInstance, 0);

    if (!windowHandle) { return -1; }

    // Shader Compilation

    DEBUG_FileResult vertexShaderSrc = DEBUG_PlatformReadEntireFile("../vertex.hlsl");
    DEBUG_FileResult pixelShaderSrc = DEBUG_PlatformReadEntireFile("../pixel.hlsl");
    DEBUG_FileResult testPNG = DEBUG_PlatformReadEntireFile("../gdeasy.png");
    int imgX;
    int imgY;
    int numComponents;
    stbi_uc * testBitmap = stbi_load_from_memory((stbi_uc*)testPNG.data, testPNG.size, &imgX, &imgY, &numComponents, 0);
    size_t bitmapSize = imgX * imgY * numComponents;

    ID3DBlob * vertexShaderBlob = nullptr;
    ID3DBlob * vertexShaderErrorBlob = nullptr;

    ID3DBlob * pixelShaderBlob = nullptr;
    ID3DBlob * pixelShaderErrorBlob = nullptr;

    if (FAILED(D3DCompile(vertexShaderSrc.data, vertexShaderSrc.size, "vertex.hlsl", NULL, NULL,"main", "vs_5_1", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &vertexShaderBlob, &vertexShaderErrorBlob)))
    {
        if (vertexShaderErrorBlob)
        {
            OutputDebugStringA(static_cast<char*>(vertexShaderErrorBlob->GetBufferPointer()));
        }
    };
    if (FAILED(D3DCompile(pixelShaderSrc.data, pixelShaderSrc.size, "pixel.hlsl", NULL, NULL, "main", "ps_5_1", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &pixelShaderBlob, &pixelShaderErrorBlob)))
    {
        if (pixelShaderErrorBlob)
        {
            OutputDebugStringA(static_cast<char*>(pixelShaderErrorBlob->GetBufferPointer()));
        }
    };

    D3D12_SHADER_BYTECODE vertexShaderBytecode = {vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize()};
    D3D12_SHADER_BYTECODE pixelShaderBytecode = {pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize()};

    //                          DirectX12
    EnableDebugLayer();

    VertexPosUV quadVertices[4] = {{{-0.25,  0.25, 0.0}, {0.0, 0.0}},    // Top Left     (blk)
                                   {{ 0.25,  0.25, 0.0}, {1.0, 0.0}},    // Top Right    (red)
                                   {{-0.25, -0.25, 0.0}, {0.0, 1.0}},    // Bottom Left  (grn)
                                   {{ 0.25, -0.25, 0.0}, {1.0, 1.0}}};   // Bottom Right (ylw)

    InstanceData2D instanceData[4] = {{{-0.5f, 0.0f, 0.0f}, {1.0f, 1.0f}, 0.0f},
                                      {{ 0.0f, 0.5f, 0.0f}, {1.0f, 1.0f}, 90.0f},
                                      {{ 0.0f,-0.5f, 0.0f}, {1.0f, 1.0f}, 180.0f},
                                      {{ 0.5f, 0.0f, 0.0f}, {1.0f, 1.0f}, 240.0f}};

    u16 quadIndices[6] = {0, 1, 2, 1, 3, 2};

    RENDERER_STATE = InitializeRenderer(windowHandle, USE_WARP, false, CLIENT_WIDTH, CLIENT_HEIGHT);
    UpdateRenderTargetViews(RENDERER_STATE.device, RENDERER_STATE.swapChain, RENDERER_STATE.rtvDescHeap, RENDERER_STATE.backBuffers, NUM_FRAMES);

    ComPtr<ID3D12Resource> vertexBuffer;
    ComPtr<ID3D12Resource> vertexUploadBuffer;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};

    ComPtr<ID3D12Resource> instanceBuffer;
    ComPtr<ID3D12Resource> instanceUploadBuffer;
    D3D12_VERTEX_BUFFER_VIEW instanceBufferView = {};

    ComPtr<ID3D12Resource> indexBuffer;
    ComPtr<ID3D12Resource> indexUploadBuffer;
    D3D12_INDEX_BUFFER_VIEW indexBufferView = {};

    
    ComPtr<ID3D12Resource> textureBuffer;
    ComPtr<ID3D12Resource> textureUploadBuffer;

    CreateBufferResource(RENDERER_STATE.device, &vertexBuffer, &vertexUploadBuffer, sizeof(quadVertices));
    CreateBufferResource(RENDERER_STATE.device, &indexBuffer, &indexUploadBuffer, sizeof(quadIndices));
    CreateBufferResource(RENDERER_STATE.device, &instanceBuffer, &instanceUploadBuffer, sizeof(instanceData));
    CreateTextureResource(RENDERER_STATE.device, &textureBuffer, &textureUploadBuffer, imgX, imgY, numComponents * imgX * imgY);


    vertexBuffer->SetName(L"Vertex Buffer");
    vertexUploadBuffer->SetName(L"Vertex Upload Buffer");
    
    indexBuffer->SetName(L"Index Buffer");
    indexUploadBuffer->SetName(L"Index Upload Buffer");

    instanceBuffer->SetName(L"Instance Buffer");
    instanceUploadBuffer->SetName(L"Instance Upload Buffer");
    textureBuffer->SetName(L"Texture Buffer");
    textureUploadBuffer->SetName(L"Texture Upload Buffer");

    UpdateBufferResource(vertexUploadBuffer, vertexBuffer, RENDERER_STATE, sizeof(quadVertices), &quadVertices);
    UpdateBufferResource(indexUploadBuffer, indexBuffer, RENDERER_STATE, sizeof(quadIndices), &quadIndices);
    UpdateBufferResource(instanceUploadBuffer, instanceBuffer, RENDERER_STATE, sizeof(instanceData), &instanceData);
    UpdateTextureResource(textureUploadBuffer, textureBuffer, RENDERER_STATE, imgX, imgY,bitmapSize, testBitmap);

    ComPtr<ID3D12Resource> depthBuffer;       // Depth Buffer
    ComPtr<ID3D12DescriptorHeap> dsvHeapDesc; // Depth Stencil View Heap Desciptor

    // Root Signature
    ComPtr<ID3D12RootSignature> rootSignature = CreateRootSignature(RENDERER_STATE.device);

    // Pipeline state object
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
            { "Position",         0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            { "UV",               0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            { "InstancePosition", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
            { "InstanceSize",     0, DXGI_FORMAT_R32G32_FLOAT,    1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
            { "InstanceRotZ",     0, DXGI_FORMAT_R32_FLOAT,       1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = {};
  
    pipelineStateDesc.pRootSignature = rootSignature.Get();
    pipelineStateDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    pipelineStateDesc.InputLayout.pInputElementDescs = inputElementDescs;
    pipelineStateDesc.InputLayout.NumElements = 5;

    pipelineStateDesc.VS = vertexShaderBytecode;
    pipelineStateDesc.PS = pixelShaderBytecode;

    pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // TODO: set this to back.
    pipelineStateDesc.RasterizerState.FrontCounterClockwise = FALSE;
    pipelineStateDesc.RasterizerState.DepthBias = 0;
    pipelineStateDesc.RasterizerState.DepthBiasClamp = 0.0f;
    pipelineStateDesc.RasterizerState.DepthClipEnable = FALSE;
    pipelineStateDesc.RasterizerState.MultisampleEnable = FALSE;
    pipelineStateDesc.RasterizerState.AntialiasedLineEnable = FALSE;
    pipelineStateDesc.RasterizerState.ForcedSampleCount = 0;
    pipelineStateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    pipelineStateDesc.StreamOutput.NumEntries = 0;
    pipelineStateDesc.StreamOutput.NumStrides = 0;
    pipelineStateDesc.StreamOutput.pBufferStrides = nullptr;
    pipelineStateDesc.StreamOutput.pSODeclaration = nullptr;
    pipelineStateDesc.StreamOutput.RasterizedStream = 0;

    pipelineStateDesc.NumRenderTargets = 1;
    pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pipelineStateDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

    pipelineStateDesc.BlendState.AlphaToCoverageEnable = FALSE;
    pipelineStateDesc.BlendState.IndependentBlendEnable = FALSE;
    pipelineStateDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
    pipelineStateDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
    pipelineStateDesc.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
    pipelineStateDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    pipelineStateDesc.DepthStencilState.DepthEnable = FALSE;
    pipelineStateDesc.DepthStencilState.StencilEnable = FALSE;
    pipelineStateDesc.SampleMask = 0xFFFFFFFF;
    pipelineStateDesc.SampleDesc.Count = 1;
    pipelineStateDesc.SampleDesc.Quality = 0; 

    pipelineStateDesc.NodeMask = 0;
    pipelineStateDesc.CachedPSO.CachedBlobSizeInBytes = 0;
    pipelineStateDesc.CachedPSO.pCachedBlob = nullptr;
    pipelineStateDesc.NumRenderTargets = 1;
    pipelineStateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    ComPtr<ID3D12PipelineState> pipelineState;
    AssertIfFailed(RENDERER_STATE.device->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&pipelineState)));

    // Input Assembler
    vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexBufferView.SizeInBytes = sizeof(quadVertices);
    vertexBufferView.StrideInBytes = sizeof(VertexPosUV);

    indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
    indexBufferView.SizeInBytes = sizeof(quadIndices);
    indexBufferView.Format = DXGI_FORMAT_R16_UINT; 

    instanceBufferView.BufferLocation = instanceBuffer->GetGPUVirtualAddress();
    instanceBufferView.SizeInBytes = sizeof(instanceData);
    instanceBufferView.StrideInBytes = sizeof(InstanceData2D);

    float modelMatrix[4][4];
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

    float clearColor[4] = {0.2f, 0.2f, 0.3f, 1.0f};

    double time = 0.0f;
    RUNNING = true;
    ShowWindow(windowHandle, SW_SHOW);
    while (RUNNING)
    {
        QueryPerformanceCounter(&lastCounter);

        win32ProcessPendingMessages(windowHandle);

        ColorRGBA color = GetHSVSpectrumColor(time);

        BeginFrame(RENDERER_STATE, clearColor);

        RENDERER_STATE.cmdList->SetPipelineState(pipelineState.Get());

        RENDERER_STATE.cmdList->SetGraphicsRootSignature(rootSignature.Get());
        RENDERER_STATE.cmdList->SetGraphicsRoot32BitConstants(0, 16, &modelMatrix, 0);

        RENDERER_STATE.cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        RENDERER_STATE.cmdList->IASetVertexBuffers(0, 1, &vertexBufferView);
        RENDERER_STATE.cmdList->IASetVertexBuffers(1, 1, &instanceBufferView);
        RENDERER_STATE.cmdList->IASetIndexBuffer(&indexBufferView);

        RENDERER_STATE.cmdList->RSSetViewports(1, &RENDERER_STATE.viewport);
        RENDERER_STATE.cmdList->RSSetScissorRects(1, &RENDERER_STATE.scissorRect);

        RENDERER_STATE.cmdList->DrawIndexedInstanced(6, 4, 0, 0, 0);

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