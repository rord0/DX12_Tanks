#include "includes.h"

inline void AssertIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        #ifdef _MSC_VER
            __debugbreak();
        #elif defined(__clang__) || defined(__GNUC__) || defined(__MINGW32__)
            __builtin_trap();
        #endif
    }
}

void EnableDebugLayer()
{
    #if defined(_DEBUG)
        ComPtr<ID3D12Debug> debugInterface;
        AssertIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
        debugInterface->EnableDebugLayer();
    #endif
}

ComPtr<IDXGIAdapter4> GetAdapter(bool useWARP)
{
    ComPtr<IDXGIFactory4> factory;
    UINT createFactoryFlags = 0;
    #ifdef _DEBUG
        createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
    #endif

    AssertIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&factory)));

    ComPtr<IDXGIAdapter4> adapter4;

    if (useWARP)
    {
        ComPtr<IDXGIAdapter> warpAdaper;
        AssertIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdaper)));
        AssertIfFailed(warpAdaper.As(&adapter4));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;

        // Try and select high performance adapter using factory6 interface.
        ComPtr<IDXGIFactory6> factory6;
        if (SUCCEEDED(factory->QueryInterface(IID_PPV_ARGS(&factory6))))
        {
            UINT adapterIndex = 0;
            while (SUCCEEDED(factory6->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&hardwareAdapter))))
            {
                DXGI_ADAPTER_DESC1 desc;
                hardwareAdapter->GetDesc1(&desc);

                // Skip if software renderer.
                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                    continue;

                // Check for Direct 3D 12 support.
                if (SUCCEEDED(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
                    break;

                adapterIndex++;
            }
        }

        // Select first hardware adapter.
        if (hardwareAdapter.Get() == nullptr)
        {
            UINT adapterIndex = 0;
            while (SUCCEEDED(factory->EnumAdapters1(adapterIndex, &hardwareAdapter)))
            {
                DXGI_ADAPTER_DESC1 desc;
                hardwareAdapter->GetDesc1(&desc);

                // Skip if software renderer.
                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                    continue;

                // Check for Direct 3D 12 support.
                if (SUCCEEDED(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
                    break;

                adapterIndex++;
            }
        }
    }

    return adapter4;
}

ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4> adapter)
{
    ComPtr<ID3D12Device2> device;
    AssertIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));

#ifdef _DEBUG
    ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(device.As(&infoQueue)))
    {
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR     , TRUE);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING   , TRUE);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);

        // Suppress Messages based on their severity level.
        D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };

        // Suppress individial messages based on their ID.
        D3D12_MESSAGE_ID denyIDs[] = {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
            // D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
            // D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,  
        };

        D3D12_INFO_QUEUE_FILTER infoFilter = {};
        infoFilter.DenyList.NumSeverities = _countof(severities);
        infoFilter.DenyList.pSeverityList = severities;
        infoFilter.DenyList.NumIDs = _countof(denyIDs);
        infoFilter.DenyList.pIDList = denyIDs;

        AssertIfFailed(infoQueue->PushStorageFilter(&infoFilter));
    }
#endif

    return device;
}

ComPtr<ID3D12CommandQueue> CreateCommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
{
    ComPtr<ID3D12CommandQueue> cmdQueue;

    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = type;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;

    AssertIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&cmdQueue)));
    return cmdQueue;
}

bool CheckTearingSupported()
{
    BOOL allowTearing = FALSE;

    ComPtr<IDXGIFactory4> factory4;

    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
    {
        ComPtr<IDXGIFactory5> factory5;
        if (SUCCEEDED(factory4.As(&factory5)))
        {
            if (FAILED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
            {
                allowTearing = false;
            }
        }
    }

    return allowTearing == TRUE;
}

ComPtr<IDXGISwapChain4> CreateSwapChain(HWND hWnd, ComPtr<ID3D12CommandQueue> cmdQueue, u32 width, u32 height, u32 bufferCount)
{
    ComPtr<IDXGISwapChain4> swapChain4;
    ComPtr<IDXGIFactory4> factory4;
    UINT createFactoryFlags = 0;

    #ifdef _DEBUG
        createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
    #endif

    AssertIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&factory4)));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};

    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;      // Controls whether the swap chain is created for stereoscopic rendering.
    swapChainDesc.SampleDesc = {1, 0}; // Sample count = 0, Quality = 0.
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = bufferCount;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = CheckTearingSupported() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    ComPtr<IDXGISwapChain1> swapChain1;

    AssertIfFailed(factory4->CreateSwapChainForHwnd(cmdQueue.Get(), hWnd, &swapChainDesc, nullptr, nullptr, &swapChain1));

    // Disable Alt + Enter fullscreen toggle feature.
    AssertIfFailed(factory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

    AssertIfFailed(swapChain1.As(&swapChain4));

    return swapChain4;
}

ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ComPtr<ID3D12Device> device, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 numDescriptors)
{
    ComPtr<ID3D12DescriptorHeap> descriptorHeap;

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = numDescriptors;
    desc.Type = type;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    AssertIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));
    return descriptorHeap;
}

void UpdateRenderTargetViews(ComPtr<ID3D12Device2> device, ComPtr<IDXGISwapChain4> swapChain, ComPtr<ID3D12DescriptorHeap> descHeap, ComPtr<ID3D12Resource> * backBuffers, u32 numFrames)
{
    UINT rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvDescHandle = descHeap->GetCPUDescriptorHandleForHeapStart();

    for (int i = 0; i < numFrames; i++)
    {
        ComPtr<ID3D12Resource> backBuffer;
        AssertIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

        // NOTE: Passing nullptr for pDesc will created a default RTV based on the back buffer resource.
        device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvDescHandle);

        backBuffers[i] = backBuffer;
        rtvDescHandle.ptr += rtvDescriptorSize;
    }
}

ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
{
    ComPtr<ID3D12CommandAllocator> cmdAllocator;
    AssertIfFailed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&cmdAllocator)));
    return cmdAllocator;
}

ComPtr<ID3D12GraphicsCommandList> CreateCommandList(ComPtr<ID3D12Device2> device, ComPtr<ID3D12CommandAllocator> cmdAllocator, D3D12_COMMAND_LIST_TYPE type)
{
    ComPtr<ID3D12GraphicsCommandList> cmdList;
    AssertIfFailed(device->CreateCommandList(0, type, cmdAllocator.Get(), nullptr, IID_PPV_ARGS(&cmdList)));
    AssertIfFailed(cmdList->Close());
    return cmdList;
}

ComPtr<ID3D12Fence> CreateFence(ComPtr<ID3D12Device2> device)
{
    ComPtr<ID3D12Fence> fence;
    AssertIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
    return fence;
}

u64 SignalCommandQueue(ComPtr<ID3D12CommandQueue> cmdQueue, ComPtr<ID3D12Fence> fence, u64 * fenceValue)
{
    *fenceValue += 1;
    u64 fenceSignalValue = *fenceValue; // Signal value that the fence will be set too.
    AssertIfFailed(cmdQueue->Signal(fence.Get(), fenceSignalValue));
    return fenceSignalValue;
}

void WaitForFenceValue(ComPtr<ID3D12Fence> fence, u64 fenceValue, HANDLE fenceEvent)
{
    if (fence->GetCompletedValue() < fenceValue)
    {
        AssertIfFailed(fence->SetEventOnCompletion(fenceValue, fenceEvent));
        WaitForSingleObject(fenceEvent, INFINITE);
    }
}

inline D3D12_RESOURCE_BARRIER CreateTransitionBarrier(ID3D12Resource * pResource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

    barrier.Transition.pResource = pResource;
    barrier.Transition.StateBefore = stateBefore;
    barrier.Transition.StateAfter = stateAfter;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    return barrier;
}

void Flush(ComPtr<ID3D12CommandQueue> cmdQueue, ComPtr<ID3D12Fence> fence,  u64 * fenceValue, HANDLE fenceEvent)
{
    u64 fenceSignalValue = SignalCommandQueue(cmdQueue, fence, fenceValue);
    WaitForFenceValue(fence, fenceSignalValue, fenceEvent);
}

HANDLE CreateFenceEventHandle()
{
    HANDLE fenceEvent;
    fenceEvent = CreateEvent(0, FALSE, FALSE, 0);
    return fenceEvent;
}

void ExecuteCommandList(ComPtr<ID3D12CommandQueue> cmdQueue, ID3D12GraphicsCommandList * pCMDList)
{
    AssertIfFailed(pCMDList->Close());
    ID3D12CommandList * const cmdLists[] = { pCMDList };
    cmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
}

void CreateBufferResource(ComPtr<ID3D12Device2> device, ID3D12Resource ** pDestinationResource, ID3D12Resource ** pIntermediateResource, size_t bufferSize)
{

    D3D12_HEAP_PROPERTIES uploadHeapProperties = {};
    uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    uploadHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    uploadHeapProperties.CreationNodeMask = 1;
    uploadHeapProperties.VisibleNodeMask = 1;

    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProperties.CreationNodeMask = 1;
    heapProperties.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Width = bufferSize;
    resourceDesc.Height = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;


    AssertIfFailed(
        device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(pIntermediateResource))
    );
    AssertIfFailed(
        device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(pDestinationResource))
    );
}

ComPtr<ID3D12RootSignature> CreateRootSignature(ComPtr<ID3D12Device2> device)
{
    ComPtr<ID3D12RootSignature> rootSignature;

    // Check for highest support root signature version.
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS|
        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

    D3D12_ROOT_PARAMETER1 rootParameters[1] = {};

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionRootSignatureDesc = {};
    
    if (featureData.HighestVersion == D3D_ROOT_SIGNATURE_VERSION_1_1)
    {
        versionRootSignatureDesc.Desc_1_1.Flags = rootSignatureFlags;
        versionRootSignatureDesc.Desc_1_1.NumParameters = 0;
        versionRootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
        versionRootSignatureDesc.Desc_1_1.pParameters = nullptr;
        versionRootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;
    }
    else
    {
        versionRootSignatureDesc.Desc_1_0.Flags = rootSignatureFlags;
        versionRootSignatureDesc.Desc_1_0.NumParameters = 0;
        versionRootSignatureDesc.Desc_1_0.NumStaticSamplers = 0;
        versionRootSignatureDesc.Desc_1_0.pParameters = nullptr;
        versionRootSignatureDesc.Desc_1_0.pStaticSamplers = nullptr;
    }
    versionRootSignatureDesc.Version = featureData.HighestVersion;

    ID3DBlob * rootSignatureBlob = nullptr;
    ID3DBlob * rootSignatureErrorBlob = nullptr;

    AssertIfFailed(D3D12SerializeVersionedRootSignature(&versionRootSignatureDesc, &rootSignatureBlob, &rootSignatureErrorBlob));
    AssertIfFailed(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));

    rootSignatureBlob->Release();
    if (rootSignatureErrorBlob) rootSignatureErrorBlob->Release();

    return rootSignature;
}

void UpdateBufferResource(ComPtr<ID3D12Resource> uploadBuffer, ComPtr<ID3D12Resource> buffer, RendererState & state, size_t bufferSize, const void * bufferData)
{
    void * uploadBufferAddress;
    D3D12_RANGE uploadRange;
    uploadRange.Begin = 0;
    uploadRange.End = bufferSize - 1;
    HRESULT result = uploadBuffer->Map(0, &uploadRange, &uploadBufferAddress);
    memcpy(uploadBufferAddress, bufferData, bufferSize);
    uploadBuffer->Unmap(0, &uploadRange);

    state.cmdList->Reset(state.cmdAllocators[0].Get(), nullptr);
    state.cmdList->CopyBufferRegion(buffer.Get(), 0, uploadBuffer.Get(), 0, bufferSize);
    ExecuteCommandList(state.cmdQueue, state.cmdList.Get());
    SignalCommandQueue(state.cmdQueue, state.fence, &state.fenceValue);
}

RendererState InitializeRenderer(HWND windowHandle, bool useWARP, bool enableVSync, u32 width, u32 height)
{
    RendererState state = {};
    state.adapter = GetAdapter(useWARP);
    state.device = CreateDevice(state.adapter);
    state.tearingSupported = CheckTearingSupported();
    state.cmdQueue = CreateCommandQueue(state.device, D3D12_COMMAND_LIST_TYPE_DIRECT);
    state.swapChain = CreateSwapChain(windowHandle, state.cmdQueue, width, height, NUM_FRAMES);
    state.rtvDescHeap = CreateDescriptorHeap(state.device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, NUM_FRAMES);
    state.rtvDescSize = state.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    state.dsvDescHeap = CreateDescriptorHeap(state.device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
    state.backBufferIndex = state.swapChain->GetCurrentBackBufferIndex();
    state.viewport.TopLeftX = 0;
    state.viewport.TopLeftY = 0;
    state.viewport.Width = width;
    state.viewport.Height = height;
    state.viewport.MaxDepth = 0.0f;
    state.viewport.MinDepth = 1.0f;
    state.scissorRect.left = 0;
    state.scissorRect.top = 0;
    state.scissorRect.right = width;
    state.scissorRect.bottom = height;

    for (int i = 0; i < NUM_FRAMES; i++)
    {
        state.cmdAllocators[i] = CreateCommandAllocator(state.device, D3D12_COMMAND_LIST_TYPE_DIRECT);
    }

    state.cmdList = CreateCommandList(state.device, state.cmdAllocators[state.backBufferIndex], D3D12_COMMAND_LIST_TYPE_DIRECT);
    state.fence = CreateFence(state.device);
    state.fenceEvent = CreateFenceEventHandle();
    state.vSyncEnabled = enableVSync;

    return state;
}