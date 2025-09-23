#include "includes.h"
#include "render.cpp"
#include "render_entry.h"
#include <climits>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define PI 3.14159265358979323846

#define GAME_CODE_DLL "tanksgame.dll"
#define RESOURCES_PATH "../res/"

bool USE_WARP = false;
const u32 CLIENT_WIDTH = 1200;
const u32 CLIENT_HEIGHT = 800;
float ASPECT = (float)CLIENT_WIDTH / (float)CLIENT_HEIGHT;

bool RUNNING = false;
bool OS_RESIZING = false;
RendererState RENDERER_STATE = {};

void Resize(u32 width, u32 height, RendererState & state)
{
    width  < 1 ? 1 : width;
    height < 1 ? 1 : height;

    ASPECT = (float)width / (float)height;
    Flush(state.cmdQueue, state.fence, &state.fenceValue, state.fenceEvent);
    for (int i = 0; i < NUM_FRAMES; i++)
    {
        state.backBuffers[i].Reset();
        state.fenceValues[i] = state.fenceValues[state.backBufferIndex];
    }

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    AssertIfFailed(state.swapChain->GetDesc(&swapChainDesc));
    AssertIfFailed(state.swapChain->ResizeBuffers(NUM_FRAMES, width, height, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

    state.viewport.TopLeftX = 0;
    state.viewport.TopLeftY = 0;
    state.viewport.Width = width;
    state.viewport.Height = height;
    state.viewport.MaxDepth = D3D12_MAX_DEPTH;
    state.viewport.MinDepth = D3D12_MIN_DEPTH;
    state.scissorRect.left = 0;
    state.scissorRect.top = 0;
    state.scissorRect.right = width;
    state.scissorRect.bottom = height;
    state.backBufferIndex = state.swapChain->GetCurrentBackBufferIndex();

    UpdateRenderTargetViews(state.device, state.swapChain, state.rtvDescHeap, state.backBuffers, NUM_FRAMES);
}

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

ImageData LoadImageFromFile(const char * filename)
{
    ImageData data = {};
    DEBUG_FileResult fileData = DEBUG_PlatformReadEntireFile(filename);
    if (fileData.data)
    {
        stbi_uc * pBitmap = stbi_load_from_memory((stbi_uc*)fileData.data, fileData.size, &data.width, &data.height, &data.numComponents, 4);
        if (pBitmap)
        {
            // NOTE(rordon): numComponts is forced to 4 in stb.
            data.numComponents = 4;
            data.size = data.width * data.height * data.numComponents;
            data.memory = pBitmap;
        }
        DEBUG_PlatformFreeFileMemory(&fileData.data);
    } 

    return data;
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

void ProcessRenderPushBuffer(RendererPushBuffer * pb, InstanceBuffer * instanceBuf, DebugGeoInstanceData * rectInstanceData, u32 * rectCount, u32 maxRects)
{
    //NOTE(rordon): all the code is doing here is taking a struct with variable size instance data
    //              then copying it into another buffer... Maybe there's a way to do this without writing code for each render command.
    size_t entryOffset = 0;
    while (entryOffset < pb->index)
    {
        RenderEntryHeader * header = (RenderEntryHeader*)(pb->memory + entryOffset);
        switch (header->type)
        {
            case RENDER_ENTRY_TYPE_CLEAR:
                entryOffset += sizeof(RenderEntryClear);
                break;
            case RENDER_ENTRY_TYPE_DEBUG_RECTANGLE:
            {
                RenderEntryDebugRectangle * entry = (RenderEntryDebugRectangle*)header;
                if (*rectCount < maxRects)
                {
                    rectInstanceData[*rectCount] = entry->instanceData;
                    *rectCount += 1;
                }
                entryOffset += sizeof(RenderEntryDebugRectangle);
            }
                break;
            case RENDER_ENTRY_TYPE_DEBUG_CIRCLE:
                entryOffset += sizeof(RenderEntryDebugCircle);
                break;
            case RENDER_ENTRY_TYPE_TEXTURED_QUAD:
            {
                RenderEntryTexturedQuad * entry = (RenderEntryTexturedQuad*)header;
                if (instanceBuf->instanceCount < instanceBuf->maxInstances)
                {
                    instanceBuf->data[instanceBuf->instanceCount++] = entry->instanceData;
                }
                entryOffset += sizeof(RenderEntryTexturedQuad);
            }   break;
            default:
                // Crashout.
                break;
        } 
    }

    pb->entryCount = 0;
    pb->index = 0;
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

ID3DBlob * CompileShaderFromFile(const char * filepath, const char * name, const char * entry, const char * target)
{
    DEBUG_FileResult shaderSrc = DEBUG_PlatformReadEntireFile(filepath);
    ID3DBlob * blob = nullptr;
    if (shaderSrc.data)
    {

        blob = CompileShader(shaderSrc.data, shaderSrc.size, name, entry, target);
        DEBUG_PlatformFreeFileMemory(&shaderSrc.data);
    }
    else
    {
        // TODO(rordon): warning here...
    }
    return blob;
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

    //////////////////////
    // Shader Compilation

    ID3DBlob * vertexShaderBlob = CompileShaderFromFile(RESOURCES_PATH"shaders/vertex.hlsl", "vertex.hlsl", "main", "vs_5_1");
    ID3DBlob * pixelShaderBlob = CompileShaderFromFile(RESOURCES_PATH"shaders/pixel.hlsl", "pixel.hlsl", "main", "ps_5_1");

    ID3DBlob * rectangleVertexShaderBlob = CompileShaderFromFile(RESOURCES_PATH"shaders/rectangle_vertex.hlsl", "rectangle_vertex.hlsl", "main", "vs_5_1");
    ID3DBlob * rectanglePixelShaderBlob = CompileShaderFromFile(RESOURCES_PATH"shaders/rectangle_pixel.hlsl", "rectangle_pixel.hlsl", "main", "ps_5_1");

    D3D12_SHADER_BYTECODE vertexShaderBytecode = {vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize()};
    D3D12_SHADER_BYTECODE pixelShaderBytecode = {pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize()};

    D3D12_SHADER_BYTECODE rectVertexShaderBytecode = {rectangleVertexShaderBlob->GetBufferPointer(), rectangleVertexShaderBlob->GetBufferSize()};
    D3D12_SHADER_BYTECODE rectPixelShaderBytecode = {rectanglePixelShaderBlob->GetBufferPointer(), rectanglePixelShaderBlob->GetBufferSize()};

    ImageData gdEasyData = LoadImageFromFile("../gd_easy.png");
    ImageData gdNormalData = LoadImageFromFile("../gd_normal.png");
    ImageData gdHardData = LoadImageFromFile("../gd_hard.png");
    ImageData gdHarderData = LoadImageFromFile("../gd_harder.png");

    EnableDebugLayer();

    VertexPosUV quadVertices[4] = {{{-0.25,  0.25, 0.0}, {0.0, 0.0}},    // Top Left     (blk)
                                   {{ 0.25,  0.25, 0.0}, {1.0, 0.0}},    // Top Right    (red)
                                   {{-0.25, -0.25, 0.0}, {0.0, 1.0}},    // Bottom Left  (grn)
                                   {{ 0.25, -0.25, 0.0}, {1.0, 1.0}}};   // Bottom Right (ylw)
    u16 quadIndices[6] = {0, 1, 2, 1, 3, 2};

    InstanceData2D dynamicInstanceData[32] = {};
    InstanceBuffer instancePushBuffer = {dynamicInstanceData, 32, 0};

    DebugGeoInstanceData rectangleInstanceData[32] = {};
    u32 rectInstanceCount = 0;
    u32 maxRectInstances = sizeof(rectangleInstanceData)/sizeof(DebugGeoInstanceData);

    RENDERER_STATE = InitializeRenderer(windowHandle, USE_WARP, false, CLIENT_WIDTH, CLIENT_HEIGHT);
    UpdateRenderTargetViews(RENDERER_STATE.device, RENDERER_STATE.swapChain, RENDERER_STATE.rtvDescHeap, RENDERER_STATE.backBuffers, NUM_FRAMES);

    ComPtr<ID3D12Resource> vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};

    ComPtr<ID3D12Resource> indexBuffer;
    D3D12_INDEX_BUFFER_VIEW indexBufferView = {};

    ComPtr<ID3D12Resource> instanceBuffer;
    ComPtr<ID3D12Resource> instanceUploadBuffer;
    D3D12_VERTEX_BUFFER_VIEW instanceBufferView = {};

    D3D12_VERTEX_BUFFER_VIEW rectInstanceBufferView = {};

    ComPtr<ID3D12Resource> textureBuffer;
    ComPtr<ID3D12Resource> textureP2Buffer;
    ComPtr<ID3D12Resource> textureP3Buffer;
    ComPtr<ID3D12Resource> textureP4Buffer;

    UploadArena arena = UploadArenaAlloc(RENDERER_STATE.device, MB(10));

    GPUAllocation vertexUpload = UploadArenaPush(&arena, sizeof(quadVertices), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    GPUAllocation indexUpload = UploadArenaPush(&arena, sizeof(quadIndices), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    CreateBufferResource(RENDERER_STATE.device,  &vertexBuffer, sizeof(quadVertices));
    CreateBufferResource(RENDERER_STATE.device,  &indexBuffer, sizeof(quadIndices));

    UpdateBufferResource(arena.resource, vertexUpload, vertexBuffer, RENDERER_STATE, sizeof(quadVertices), &quadVertices);
    UpdateBufferResource(arena.resource, indexUpload, indexBuffer, RENDERER_STATE, sizeof(quadIndices), &quadIndices);

    CreateBufferResource(RENDERER_STATE.device,  &instanceBuffer, sizeof(dynamicInstanceData));
    CreateUploadBufferResource(RENDERER_STATE.device,  &instanceUploadBuffer, sizeof(dynamicInstanceData));

    CreateTextureResource(RENDERER_STATE.device, &textureBuffer, gdEasyData.width, gdEasyData.height, gdEasyData.size);
    CreateTextureResource(RENDERER_STATE.device, &textureP2Buffer, gdNormalData.width, gdNormalData.height, gdNormalData.size);
    CreateTextureResource(RENDERER_STATE.device, &textureP3Buffer, gdHardData.width, gdHardData.height, gdHardData.size);
    CreateTextureResource(RENDERER_STATE.device, &textureP4Buffer, gdHarderData.width, gdHarderData.height, gdHarderData.size);


    UpdateTextureResource(RENDERER_STATE, &arena, textureBuffer,   gdEasyData);
    UpdateTextureResource(RENDERER_STATE, &arena, textureP2Buffer, gdNormalData);
    UpdateTextureResource(RENDERER_STATE, &arena, textureP3Buffer, gdHardData);
    UpdateTextureResource(RENDERER_STATE, &arena, textureP4Buffer, gdHarderData);

    UploadArenaRelease(&arena);

    vertexBuffer->SetName(L"Vertex Buffer");
    indexBuffer->SetName(L"Index Buffer");

    instanceBuffer->SetName(L"Instance Buffer");
    instanceUploadBuffer->SetName(L"Instance Upload Buffer");
    textureBuffer->SetName(L"Texture Buffer 1");

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

    D3D12_INPUT_ELEMENT_DESC debugGeoElementDescs[] = {
            { "Position",         0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            { "UV",               0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            { "InstancePosition", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
            { "InstanceSize",     0, DXGI_FORMAT_R32G32_FLOAT,    1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
            { "InstanceColor",    0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
            { "InstanceRotZ",     0, DXGI_FORMAT_R32_FLOAT,       1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
            { "InstanceFill",     0, DXGI_FORMAT_R32_FLOAT,       1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = createQuadPipelineStateDesc(rootSignature.Get(), inputElementDescs, 5, vertexShaderBytecode, pixelShaderBytecode);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC debugGeoPSODesc = createQuadPipelineStateDesc(rootSignature.Get(), debugGeoElementDescs, _countof(debugGeoElementDescs), rectVertexShaderBytecode, rectPixelShaderBytecode);
  
    ComPtr<ID3D12PipelineState> pipelineState;
    AssertIfFailed(RENDERER_STATE.device->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&pipelineState)));

    ComPtr<ID3D12PipelineState> rectPipelineState;
    AssertIfFailed(RENDERER_STATE.device->CreateGraphicsPipelineState(&debugGeoPSODesc, IID_PPV_ARGS(&rectPipelineState)));

    // Input Assembler
    vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexBufferView.SizeInBytes = sizeof(quadVertices);
    vertexBufferView.StrideInBytes = sizeof(VertexPosUV);

    indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
    indexBufferView.SizeInBytes = sizeof(quadIndices);
    indexBufferView.Format = DXGI_FORMAT_R16_UINT; 

    instanceBufferView.BufferLocation = instanceBuffer->GetGPUVirtualAddress();
    instanceBufferView.SizeInBytes = sizeof(dynamicInstanceData);
    instanceBufferView.StrideInBytes = sizeof(InstanceData2D);

    rectInstanceBufferView.SizeInBytes = sizeof(rectangleInstanceData);
    rectInstanceBufferView.StrideInBytes = sizeof(DebugGeoInstanceData);

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    srvHeapDesc.NodeMask = 0;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = 8;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.PlaneSlice = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    ComPtr<ID3D12DescriptorHeap> srvHeap;
    AssertIfFailed(RENDERER_STATE.device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap)));

    D3D12_CPU_DESCRIPTOR_HANDLE textDescHandle = srvHeap->GetCPUDescriptorHandleForHeapStart();
    UINT srvHandleIncrementSize = RENDERER_STATE.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    RENDERER_STATE.device->CreateShaderResourceView(textureBuffer.Get(), &srvDesc, textDescHandle);
    textDescHandle.ptr += srvHandleIncrementSize;
    RENDERER_STATE.device->CreateShaderResourceView(textureP2Buffer.Get(), &srvDesc, textDescHandle);
    textDescHandle.ptr += srvHandleIncrementSize;
    RENDERER_STATE.device->CreateShaderResourceView(textureP3Buffer.Get(), &srvDesc, textDescHandle);
    textDescHandle.ptr += srvHandleIncrementSize;
    RENDERER_STATE.device->CreateShaderResourceView(textureP4Buffer.Get(), &srvDesc, textDescHandle);

    UploadArena frameUploadArena = UploadArenaAlloc(RENDERER_STATE.device, KB(4));

    bool contentLoaded = false;
    ShowWindow(windowHandle, SW_SHOW);

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
    gameMemory.permStorage = VirtualAlloc(0, MB(2), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    gameMemory.permStorageSize = MB(2);
    gameMemory.transientStorage = VirtualAlloc(0, MB(1), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    gameMemory.transStorageSize = MB(1);

    InputState inputState = {};

    gameCode.Start(&gameMemory);

    //----------------------
    // Main Loop           |
    // --------------------
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
            Resize(resolution.x, resolution.y, RENDERER_STATE);
            prevResolution = resolution;
        }
        GameState * gameState = (GameState*)gameMemory.permStorage;
        gameState->tempInput.y = (float)inputState.W.isDown + -(float)(inputState.S.isDown); 
        gameState->tempInput.x = (float)inputState.D.isDown + -(float)(inputState.A.isDown); 

        gameCode.Update(&gameMemory, deltaTime);

        mat4 projectionMatrix = orthographicProjection(ASPECT, -ASPECT, 1.0f, -1.0f, -0.01f, 100.0f);

        ProcessRenderPushBuffer(&gameState->renderPB, &instancePushBuffer, rectangleInstanceData, &rectInstanceCount, maxRectInstances);
        
        // Render
        BeginFrame(RENDERER_STATE, (float*)&gameState->clearColor);

        // Upload Instance Data
        UploadArenaClear(&frameUploadArena);
        GPUAllocation dynAlloc = UploadArenaPush(&frameUploadArena, sizeof(dynamicInstanceData), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        memcpy(dynAlloc.pCPU, dynamicInstanceData, sizeof(dynamicInstanceData));
        instanceBufferView.BufferLocation = dynAlloc.pGPU;
        GPUAllocation rectAlloc = UploadArenaPush(&frameUploadArena, sizeof(rectangleInstanceData), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        memcpy(rectAlloc.pCPU, rectangleInstanceData, sizeof(rectangleInstanceData));
        rectInstanceBufferView.BufferLocation = rectAlloc.pGPU;
        //DynamicUpdateBufferResource(instanceUploadBuffer.Get(), instanceBuffer.Get(), RENDERER_STATE.cmdList.Get(), sizeof(dynamicInstanceData), dynamicInstanceData);
        // TODO: clear frame arena then copy instance buffers into it and recreate buffer views.
        instancePushBuffer.instanceCount = 0;

        // NOTE(rordon): shouldn't change much....
        RENDERER_STATE.cmdList->SetGraphicsRootSignature(rootSignature.Get());
        RENDERER_STATE.cmdList->SetGraphicsRoot32BitConstants(0, 16, &projectionMatrix.m, 0);
        ID3D12DescriptorHeap * heaps[] = { srvHeap.Get() };
        RENDERER_STATE.cmdList->SetDescriptorHeaps(1, heaps);
        RENDERER_STATE.cmdList->RSSetViewports(1, &RENDERER_STATE.viewport);
        RENDERER_STATE.cmdList->RSSetScissorRects(1, &RENDERER_STATE.scissorRect);
        RENDERER_STATE.cmdList->IASetVertexBuffers(0, 1, &vertexBufferView);
        RENDERER_STATE.cmdList->IASetIndexBuffer(&indexBufferView);

        // TODO(rordon): Render rectangles
        // TODO(rordon): Render circles 
        // TODO(rordon): Render lines??? 
        // TODO(rordon): Render Textured Quads. 

        // Get the frame upload arena, allocate enough for the instance data, create view for instance data, bind it
        RENDERER_STATE.cmdList->SetPipelineState(pipelineState.Get());
        RENDERER_STATE.cmdList->SetGraphicsRootDescriptorTable(1, srvHeap->GetGPUDescriptorHandleForHeapStart());

        RENDERER_STATE.cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        RENDERER_STATE.cmdList->IASetVertexBuffers(1, 1, &instanceBufferView);

        D3D12_GPU_DESCRIPTOR_HANDLE handy = srvHeap->GetGPUDescriptorHandleForHeapStart();
        for (int i = 0; i < 4; i++)
        {
            RENDERER_STATE.cmdList->SetGraphicsRootDescriptorTable(1, handy);
            RENDERER_STATE.cmdList->DrawIndexedInstanced(6, 1, 0, 0, i);
            handy.ptr += srvHandleIncrementSize;
        }

        RENDERER_STATE.cmdList->SetPipelineState(rectPipelineState.Get());
        //RENDERER_STATE.cmdList->SetGraphicsRootDescriptorTable(1, srvHeap->GetGPUDescriptorHandleForHeapStart());

        RENDERER_STATE.cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        RENDERER_STATE.cmdList->IASetVertexBuffers(1, 1, &rectInstanceBufferView);
        RENDERER_STATE.cmdList->DrawIndexedInstanced(6, rectInstanceCount, 0, 0, 0);

        EndFrame(RENDERER_STATE);
        
        rectInstanceCount = 0;
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