#include "renderer_dx12.h"

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

void CreateBufferResource(ComPtr<ID3D12Device2> device, ID3D12Resource ** pDestinationResource, size_t bufferSize)
{
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
        device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(pDestinationResource))
    );
}

void CreateUploadBufferResource(ComPtr<ID3D12Device2> device, ID3D12Resource ** ppResource, size_t bufferSize)
{
    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
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
        device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(ppResource))
    );
}

void CreateTextureResource(ComPtr<ID3D12Device2> device, ID3D12Resource ** pDestinationResource, u64 width, u64 height, size_t bufferSize)
{
    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProperties.CreationNodeMask = 1;
    heapProperties.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = width;
    resourceDesc.Height = height;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; 
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    
    AssertIfFailed(
        device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(pDestinationResource))
    );
}

ID3DBlob * CompileShader(void * src, size_t size, const char * name, const char * entry, const char * target)
{
    ID3DBlob * shaderBlob = nullptr;
    ID3DBlob * errorBlob = nullptr;
	#ifdef _DEBUG
		UINT flags1 = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
	#else
		UINT flags1 = D3DCOMPILE_OPTIMIZATION_LEVEL3;
	#endif

    if (FAILED(D3DCompile(src, size, name, NULL, NULL, entry, target, flags1, 0, &shaderBlob, &errorBlob)))
    {
        if (errorBlob)
        {
            OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
            errorBlob->Release();
        }
    };

    return shaderBlob;
}

D3D12_GRAPHICS_PIPELINE_STATE_DESC 
createQuadPipelineStateDesc(ID3D12RootSignature * rootSignature,
                            const D3D12_INPUT_ELEMENT_DESC * pInputElementDescs,
                            u32 numDescs,
                            D3D12_SHADER_BYTECODE vsByteCode,
                            D3D12_SHADER_BYTECODE psByteCode) 
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
  
    desc.pRootSignature = rootSignature;
    desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    desc.InputLayout.pInputElementDescs = pInputElementDescs;
    desc.InputLayout.NumElements = numDescs;

    desc.VS = vsByteCode;
    desc.PS = psByteCode;

    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // TODO: set this to back.
    desc.RasterizerState.FrontCounterClockwise = FALSE;
    desc.RasterizerState.DepthBias = 0;
    desc.RasterizerState.DepthBiasClamp = 0.0f;
    desc.RasterizerState.DepthClipEnable = FALSE;
    desc.RasterizerState.MultisampleEnable = FALSE;
    desc.RasterizerState.AntialiasedLineEnable = FALSE;
    desc.RasterizerState.ForcedSampleCount = 0;
    desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    desc.StreamOutput.NumEntries = 0;
    desc.StreamOutput.NumStrides = 0;
    desc.StreamOutput.pBufferStrides = nullptr;
    desc.StreamOutput.pSODeclaration = nullptr;
    desc.StreamOutput.RasterizedStream = 0;

    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

    desc.BlendState.AlphaToCoverageEnable = FALSE;
    desc.BlendState.IndependentBlendEnable = FALSE;

    desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    desc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
    desc.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;

    desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;

    desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

    desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.StencilEnable = FALSE;
    desc.SampleMask = 0xFFFFFFFF;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0; 

    desc.NodeMask = 0;
    desc.CachedPSO.CachedBlobSizeInBytes = 0;
    desc.CachedPSO.pCachedBlob = nullptr;
    desc.NumRenderTargets = 1;
    desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    return desc;
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
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionRootSignatureDesc = {};

    versionRootSignatureDesc.Version = featureData.HighestVersion;

    D3D12_DESCRIPTOR_RANGE1 descRange1 = {};
    descRange1.NumDescriptors = 32;
    descRange1.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descRange1.BaseShaderRegister = 0;
    descRange1.RegisterSpace = 0;
    descRange1.OffsetInDescriptorsFromTableStart = 0;
    descRange1.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;

    D3D12_STATIC_SAMPLER_DESC staticSamplerDesc = {};
    staticSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    staticSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    staticSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    staticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    staticSamplerDesc.MaxAnisotropy = 0;
    staticSamplerDesc.MipLODBias = 0;
    staticSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplerDesc.MinLOD = 0.0f;
    staticSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplerDesc.ShaderRegister = 0;
    staticSamplerDesc.RegisterSpace = 0;
    staticSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_PARAMETER1 rootParameters1_1[2] = {};
    D3D12_ROOT_PARAMETER rootParameters1_0[2] = {};

    if (featureData.HighestVersion >= D3D_ROOT_SIGNATURE_VERSION_1_1)
    {
        versionRootSignatureDesc.Desc_1_1.Flags = rootSignatureFlags;
        versionRootSignatureDesc.Desc_1_1.NumParameters =2;
        versionRootSignatureDesc.Desc_1_1.pParameters = rootParameters1_1;
        versionRootSignatureDesc.Desc_1_1.NumStaticSamplers = 1;
        versionRootSignatureDesc.Desc_1_1.pStaticSamplers = &staticSamplerDesc;

        rootParameters1_1[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParameters1_1[0].Constants.Num32BitValues = 16;
        rootParameters1_1[0].Constants.RegisterSpace = 0;
        rootParameters1_1[0].Constants.ShaderRegister = 0;
        rootParameters1_1[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        rootParameters1_1[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters1_1[1].DescriptorTable.NumDescriptorRanges = 1;
        rootParameters1_1[1].DescriptorTable.pDescriptorRanges = &descRange1;
        rootParameters1_1[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    }
    else
    {
        versionRootSignatureDesc.Desc_1_0.Flags = rootSignatureFlags;
        versionRootSignatureDesc.Desc_1_0.NumParameters = 1;
        versionRootSignatureDesc.Desc_1_0.NumStaticSamplers = 0;
        versionRootSignatureDesc.Desc_1_0.pParameters = rootParameters1_0;
        versionRootSignatureDesc.Desc_1_0.pStaticSamplers = nullptr;

        rootParameters1_0[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParameters1_0[0].Constants.Num32BitValues = 16;
        rootParameters1_0[0].Constants.RegisterSpace = 0;
        rootParameters1_0[0].Constants.ShaderRegister = 0;
        rootParameters1_0[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    }

    ID3DBlob * rootSignatureBlob = nullptr;
    ID3DBlob * rootSignatureErrorBlob = nullptr;

    if (FAILED(D3D12SerializeVersionedRootSignature(&versionRootSignatureDesc, &rootSignatureBlob, &rootSignatureErrorBlob)))
    {
        if (rootSignatureErrorBlob)
        {
            OutputDebugStringA(static_cast<char*>(rootSignatureErrorBlob->GetBufferPointer()));
            rootSignatureErrorBlob->Release();
        }
    }
    AssertIfFailed(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
    rootSignatureBlob->Release();

    return rootSignature;
}

void UpdateBufferResource(ComPtr<ID3D12Resource> uploadBuffer, GPUAllocation uploadAlloc, ComPtr<ID3D12Resource> buffer, RendererState & state, size_t bufferSize, const void * bufferData)
{
    // Copy data into upload buffer
    memcpy(uploadAlloc.pCPU, bufferData, bufferSize);

    // Copy from upload heap to default heap
    state.cmdList->Reset(state.cmdAllocators[0].Get(), nullptr);
    state.cmdList->CopyBufferRegion(buffer.Get(), 0, uploadBuffer.Get(), uploadAlloc.offset, bufferSize);

    ExecuteCommandList(state.cmdQueue, state.cmdList.Get());
    int fenceValue = SignalCommandQueue(state.cmdQueue, state.fence, &state.fenceValue);
    WaitForFenceValue(state.fence, fenceValue, state.fenceEvent);
}

void DynamicUpdateBufferResource(ID3D12Resource * uploadBuffer, ID3D12Resource * buffer, ID3D12GraphicsCommandList * cmdList, size_t bufferSize, const void * bufferData)
{
    // Upload Buffer to CPU
    void * uploadBufferAddress;
    D3D12_RANGE uploadRange;
    uploadRange.Begin = 0;
    uploadRange.End = bufferSize - 1;
    HRESULT result = uploadBuffer->Map(0, &uploadRange, &uploadBufferAddress);
    AssertIfFailed(result);
    memcpy(uploadBufferAddress, bufferData, bufferSize);
    uploadBuffer->Unmap(0, &uploadRange);

    cmdList->CopyBufferRegion(buffer, 0, uploadBuffer, 0, bufferSize);
    D3D12_RESOURCE_BARRIER barrier = CreateTransitionBarrier(buffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    cmdList->ResourceBarrier(1, &barrier);
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

UploadArena UploadArenaAlloc(ComPtr<ID3D12Device2> device, size_t size)
{
    UploadArena arena = {};
    arena.size = size;
    CreateUploadBufferResource(device, &arena.resource, size);
    arena.pGPU = arena.resource->GetGPUVirtualAddress();
    HRESULT mapResult = arena.resource->Map(0, nullptr, &arena.pCPU);
    AssertIfFailed(mapResult);
    arena.index = 0;
    return arena;
}

void UploadArenaClear(UploadArena * arena)
{
    arena->index = 0;
}

void UploadArenaRelease(UploadArena * arena)
{
    arena->resource->Unmap(0, nullptr);
    arena->resource->Release();
    arena->resource = nullptr;
    arena->pCPU = nullptr;
    arena->pGPU = D3D12_GPU_VIRTUAL_ADDRESS(0);
    arena->size = 0;
}

inline size_t AlignPow2(size_t size, size_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

GPUAllocation UploadArenaPush(UploadArena * arena, size_t size, size_t alignment)
{
    GPUAllocation allocation = {};

    size_t alignedSize = AlignPow2(size, alignment);
    size_t alignedOffset = AlignPow2(arena->index, alignment);

    // Check if arena has space for aligned memory
    if (alignedOffset + alignedSize <= arena->size)
    {
        allocation.pCPU = (u8*)arena->pCPU + alignedOffset;
        allocation.pGPU = arena->pGPU + alignedOffset;
        allocation.offset = alignedOffset;

        arena->index += alignedSize;
    }
    return allocation;
}

void UploadArenaPop(UploadArena * arena, size_t size)
{
    if (size <= arena->index)
    {
        arena->index -= size;
    }
    else
    {
        // TODO(rordon): warning that user popped too much off stack.
        arena->index = 0;
    }
}

void UpdateTextureResource(RendererState & state, ComPtr<ID3D12Resource> textureResource, const ImageData * img)
{ 

    // Get aligned resource size in bytes.
    D3D12_RESOURCE_DESC desc = textureResource->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
    UINT numRows;
    UINT64 srcRowPitch;
    UINT64 totalBytes;
    state.device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &numRows, &srcRowPitch, &totalBytes);

    // Create a staging buffer for texture data.
    ID3D12Resource * uploadResource;
    CreateUploadBufferResource(state.device, &uploadResource, totalBytes);
    D3D12_GPU_VIRTUAL_ADDRESS pGPU = uploadResource->GetGPUVirtualAddress();
    void * pCPU = nullptr;
    HRESULT mapResult = uploadResource->Map(0, nullptr, &pCPU);
    AssertIfFailed(mapResult);

    for (int y = 0; y < numRows; y++)
    {
        u8 * destPadded = (u8*)pCPU + (footprint.Footprint.RowPitch * y);
        u8 * srcPacked =  img->memory +  (srcRowPitch * y);
        memcpy(destPadded, srcPacked, srcRowPitch);
    }

    state.cmdList->Reset(state.cmdAllocators[0].Get(), nullptr);

    D3D12_BOX textureSizeBox = {};
    textureSizeBox.left = 0;
    textureSizeBox.right = img->width;
    textureSizeBox.top = 0;
    textureSizeBox.bottom = img->height;
    textureSizeBox.front = 0;
    textureSizeBox.back = 1;

    D3D12_TEXTURE_COPY_LOCATION textureSrc;
    textureSrc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    textureSrc.pResource = uploadResource;
    textureSrc.SubresourceIndex = 0;
    textureSrc.PlacedFootprint.Offset = 0;
    textureSrc.PlacedFootprint.Footprint = footprint.Footprint;

    D3D12_TEXTURE_COPY_LOCATION textureDest = {};
    textureDest.pResource = textureResource.Get();
    textureDest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    textureDest.SubresourceIndex = 0;

    state.cmdList->CopyTextureRegion(&textureDest, 0, 0, 0, &textureSrc, &textureSizeBox);

    D3D12_RESOURCE_BARRIER b = CreateTransitionBarrier(textureResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    state.cmdList->ResourceBarrier(1, &b);

    ExecuteCommandList(state.cmdQueue, state.cmdList.Get());
    int fenceValue = SignalCommandQueue(state.cmdQueue, state.fence, &state.fenceValue);
    WaitForFenceValue(state.fence, fenceValue, state.fenceEvent);

    uploadResource->Unmap(0, nullptr);
    uploadResource->Release();
}

RendererState DX12_InitializeRenderer(HWND windowHandle, bool useWARP, bool enableVSync, u32 width, u32 height)
{
    EnableDebugLayer();

    RendererState state = {};
    state.adapter = GetAdapter(useWARP);
    state.device = CreateDevice(state.adapter);
    state.tearingSupported = CheckTearingSupported();
    state.cmdQueue = CreateCommandQueue(state.device, D3D12_COMMAND_LIST_TYPE_DIRECT);
    state.swapChain = CreateSwapChain(windowHandle, state.cmdQueue, width, height, NUM_FRAMES);
    state.rtvDescHeap = CreateDescriptorHeap(state.device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, NUM_FRAMES);
    state.rtvDescSize = state.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    state.srvDescSize = state.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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

    UpdateRenderTargetViews(state.device, state.swapChain, state.rtvDescHeap, state.backBuffers, NUM_FRAMES);
    return state;
}
