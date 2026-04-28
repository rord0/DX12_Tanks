#include "array.h"
#include "arena.h"

#include "core.h"
#include "includes.h"

#include "renderer_dx12.h"
#include "render_entry.h"
#include "./engine/fonts.hpp"

#include "render.cpp"
#include <algorithm>
#include <cstddef>

int DX12_RendererCreateTexture(const ImageData * image, RendererState & state, RendererResourcesDX12  & res);

void GenerateRoundCapLineGeometry(vec3 * vertexBuffer, u16 * indexBuffer, u32 * vertexCount, u16 * indexCount, u32 resolution)
{
    // TODO: assert that resolution is greater than one.

    size_t geometrySize = (resolution * sizeof(vec3) * 2) + (resolution * 3 * 4);
    
    Array vertices = {sizeof(vec3), 64, 0, vertexBuffer};
    Array indices = {sizeof(u16), 64, 0, indexBuffer};

    vec3 leftCenter = {0.0f, 0.0f, 0.0f};
    ArrayPush(&vertices, &leftCenter);

    // TODO: allocate arena space for geomtry data.

    // Left Semicircle Vertices
    for (int step = 0; step <= resolution; step++)
    {
        const float theta = (PI / 2) + ((step + 0) * PI) / resolution;
        vec3 vertexL = {cosf(theta) * 0.5f, -sinf(theta) * 0.5f, 0.0f};
        ArrayPush(&vertices, &vertexL);
    }

    // Right Semicircle Vertices
    vec3 rightCenter = {0.0f, 0.0f, 1.0f};
    ArrayPush(&vertices, &rightCenter);
    for (int step = 0; step <= resolution; step++)
    {
        const float theta = (PI / 2) + ((step + 0) * PI) / resolution;
        vec3 vertexR = {(-cosf(theta) * 0.5f), sinf(theta) * 0.5f, 1.0f};
        ArrayPush(&vertices, &vertexR);
    }

    // Indices
    if (resolution > 1)
    {
        for (int step = 0; step < resolution; step++)
        {
            u16 l0 = 0; // Center Left Vertex
            u16 l1 = step + 1;
            u16 l2 = step + 2;

            ArrayPush(&indices, &l0);
            ArrayPush(&indices, &l1);
            ArrayPush(&indices, &l2);

            u16 offset = resolution + 2;
            u16 r0 = offset + 0; // Center Right Vertex
            u16 r1 = offset + step + 1;
            u16 r2 = offset + step + 2;

            ArrayPush(&indices, &r0);
            ArrayPush(&indices, &r1);
            ArrayPush(&indices, &r2);
        }
    }
    
    // Middle Triangle Indices
    u16 m0 = 1;
    u16 m1 = resolution + 1;
    u16 m2 = (resolution + 1) + 2;
    u16 m3 = (2*resolution + 1) + 2;

    ArrayPush(&indices, &m0);
    ArrayPush(&indices, &m1);
    ArrayPush(&indices, &m2);

    ArrayPush(&indices, &m2);
    ArrayPush(&indices, &m3);
    ArrayPush(&indices, &m0);

    // Generate Indices
    *vertexCount = vertices.count;
    *indexCount = indices.count;
}

void CreateInstancePipelineState(ID3D12PipelineState ** ppPSO,
                                 ComPtr<ID3D12Device2> device,
                                 ID3D12RootSignature * rootSignature,
                                 const D3D12_INPUT_ELEMENT_DESC * inputElementDescs,
                                 u32 numDescs,
                                 const char * vertexShaderPath,
                                 const char * pixelShaderPath)
{

    ID3DBlob * vertexShaderBlob = CompileShaderFromFile(vertexShaderPath, "vertex.hlsl", "VSmain", "vs_5_1");
    ID3DBlob * pixelShaderBlob  = CompileShaderFromFile(pixelShaderPath, "pixel.hlsl", "PSmain", "ps_5_1");

    D3D12_SHADER_BYTECODE vertexShaderBytecode = {vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize()};
    D3D12_SHADER_BYTECODE  pixelShaderBytecode = {pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize()};

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = createQuadPipelineStateDesc(rootSignature, inputElementDescs, numDescs, vertexShaderBytecode, pixelShaderBytecode);
    AssertIfFailed(device->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(ppPSO)));

    vertexShaderBlob->Release();
    pixelShaderBlob->Release();
}

void InitInstanceRenderData(InstanceRenderData * data,
                                   D3D12_VERTEX_BUFFER_VIEW vertexBufferView,
                                   D3D12_INDEX_BUFFER_VIEW indexBufferView,
                                   u32 indexCountPerInstance,
                                   u32 instanceSize,
                                   u32 maxInstances,
                                   u32 id,
                                   Arena * arena)
{
    data->instanceID = id;

    // Initalize Buffer Views
    data->vertexBufferView = vertexBufferView;
    data->indexBufferView = indexBufferView;
    data->indexCountPerInstance = indexCountPerInstance;

    data->instanceBufferView.StrideInBytes = instanceSize;
    data->instanceBufferView.BufferLocation = D3D12_GPU_VIRTUAL_ADDRESS(0);
    data->instanceBufferView.SizeInBytes = 0;

    data->instanceData = ArrayInit(instanceSize, maxInstances, ArenaPush(arena, instanceSize * maxInstances));
}

RendererResourcesDX12 InitInstancePipelineResources(RendererState & state)
{
    RendererResourcesDX12 res = {};
    //////////////////////
    // Shader Compilation

    /////////////////
    // Root Signature
    res.rootSignature = CreateRootSignature(state.device);

    /////////////////////
    // Create Allocators
    res.frameUploadArena = UploadArenaAlloc(state.device, KB(6));
    res.frameUploadArena.resource->SetName(L"Frame Upload Resource");

    const u64 instanceArenaSize = (sizeof(TextureInstanceData) * 64) + (sizeof(GlyphInstanceData) * 128)+(sizeof(DebugGeoInstanceData) * 128) + (sizeof(LineInstanceData) * 32) + (sizeof(SubTextureInstanceData) * 32);
    res.instanceDataArena = ArenaAlloc(instanceArenaSize);

    ////////////
    // Geometry
    const VertexPosUV quadVertices[4] = {{{-0.25,  0.25, 0.0}, {0.0, 0.0}},    // Top Left      (0,0)---(1,0)
                                         {{ 0.25,  0.25, 0.0}, {1.0, 0.0}},    // Top Right       |    /  |
                                         {{-0.25, -0.25, 0.0}, {0.0, 1.0}},    // Bottom Left     |  /    |
                                         {{ 0.25, -0.25, 0.0}, {1.0, 1.0}}};   // Bottom Right  (0,1)---(1,1)

    const u16 quadIndices[6] = {0, 1, 2, 1, 3, 2};

    vec3 lineVertices[128] = {};
    u16 lineIndices[128] = {};

    u32 lineVertexCount = 0;
    u16 lineIndexCount = 0;
    GenerateRoundCapLineGeometry(lineVertices, lineIndices, &lineVertexCount, &lineIndexCount, 6);

    // Create temporary staging buffer to upload geometry and texture data to GPU.
    UploadArena tempUploadArena = UploadArenaAlloc(state.device, MB(10));

    GPUAllocation vertexUpload = UploadArenaPush(&tempUploadArena, sizeof(quadVertices), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    GPUAllocation indexUpload = UploadArenaPush(&tempUploadArena, sizeof(quadIndices), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    GPUAllocation lineVertexUpload = UploadArenaPush(&tempUploadArena, sizeof(vec3) * lineVertexCount, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    GPUAllocation lineIndexUpload = UploadArenaPush(&tempUploadArena, sizeof(u16) * lineIndexCount, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    CreateBufferResource(state.device,  &res.textureVertexBuffer, sizeof(quadVertices));
    CreateBufferResource(state.device,  &res.textureIndexBuffer, sizeof(quadIndices));

    UpdateBufferResource(tempUploadArena.resource, vertexUpload, res.textureVertexBuffer, state, sizeof(quadVertices), &quadVertices);
    UpdateBufferResource(tempUploadArena.resource, indexUpload, res.textureIndexBuffer, state, sizeof(quadIndices), &quadIndices);

    CreateBufferResource(state.device,  &res.lineVertexBuffer, sizeof(vec3) * lineVertexCount);
    CreateBufferResource(state.device,  &res.lineIndexBuffer, sizeof(u16) * lineIndexCount);

    UpdateBufferResource(tempUploadArena.resource, lineVertexUpload, res.lineVertexBuffer, state, sizeof(vec3) * lineVertexCount, &lineVertices);
    UpdateBufferResource(tempUploadArena.resource, lineIndexUpload, res.lineIndexBuffer, state, sizeof(u16) * lineIndexCount, &lineIndices);

    res.textureVertexBuffer->SetName(L"Textured Quad Vertex Buffer");
    res.textureIndexBuffer->SetName(L"Textured Quad Index Buffer");
    res.lineIndexBuffer->SetName(L"Line Index Buffer");
    res.lineVertexBuffer->SetName(L"Line Vertex Buffer");
    
    UploadArenaClear(&tempUploadArena);

    //////////////////////////////////
    // Create and upload texture data
    res.textureCount = 0;
    res.maxTexures = 32;

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    srvHeapDesc.NodeMask = 0;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = res.maxTexures;
    AssertIfFailed(state.device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&res.textureSRVHeap)));

    D3D12_CPU_DESCRIPTOR_HANDLE textDescHandle = res.textureSRVHeap->GetCPUDescriptorHandleForHeapStart();
    UINT srvHandleIncrementSize = state.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    for (int i = 0; i < res.maxTexures; i++)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC nullDesc = {};

        nullDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        nullDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        nullDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        nullDesc.Texture2D.MipLevels = 1;
        state.device->CreateShaderResourceView(nullptr, &nullDesc, textDescHandle);
        textDescHandle.ptr += srvHandleIncrementSize;
    }

    ImageData invalidTexture = LoadImageFromFile(RESOURCES_PATH"world_eater.jpg");
    ImageData gdEasyData = LoadImageFromFile(RESOURCES_PATH"gd_easy.png");
    ImageData gdNormalData = LoadImageFromFile(RESOURCES_PATH"gd_normal.png");
    ImageData gdHardData = LoadImageFromFile(RESOURCES_PATH"gd_hard.png");
    ImageData gdHarderData = LoadImageFromFile(RESOURCES_PATH"gd_harder.png");

    DX12_RendererCreateTexture(&invalidTexture, state, res);
    DX12_RendererCreateTexture(&gdEasyData, state, res);
    DX12_RendererCreateTexture(&gdNormalData, state, res);
    DX12_RendererCreateTexture(&gdHardData, state, res);
    DX12_RendererCreateTexture(&gdHarderData, state, res);

    FreeImage(&invalidTexture);
    FreeImage(&gdEasyData);
    FreeImage(&gdNormalData);
    FreeImage(&gdHardData);
    FreeImage(&gdHarderData);

    /////////////////////
    // Create Buffer Views
    res.textureVertexBufferView.BufferLocation = res.textureVertexBuffer->GetGPUVirtualAddress();
    res.textureVertexBufferView.SizeInBytes = sizeof(quadVertices);
    res.textureVertexBufferView.StrideInBytes = sizeof(VertexPosUV);

    res.textureIndexBufferView.BufferLocation = res.textureIndexBuffer->GetGPUVirtualAddress();
    res.textureIndexBufferView.SizeInBytes = sizeof(quadIndices);
    res.textureIndexBufferView.Format = DXGI_FORMAT_R16_UINT; 

    res.lineVertexBufferView.BufferLocation = res.lineVertexBuffer->GetGPUVirtualAddress();
    res.lineVertexBufferView.SizeInBytes = sizeof(vec3) * lineVertexCount;
    res.lineVertexBufferView.StrideInBytes = sizeof(vec3);

    res.lineIndexBufferView.BufferLocation = res.lineIndexBuffer->GetGPUVirtualAddress();
    res.lineIndexBufferView.SizeInBytes = sizeof(u16) * lineIndexCount;
    res.lineIndexBufferView.Format = DXGI_FORMAT_R16_UINT; 

    // Free temp upload arena.
    res.instanceDrawCMDs = ArrayInit(sizeof(DrawInstanceCMD), 128, ArenaPush(&res.instanceDataArena, sizeof(DrawInstanceCMD) * 128));

    UploadArenaRelease(&tempUploadArena);

    // Create Textured Quad Instance Data
    const D3D12_INPUT_ELEMENT_DESC texturedQuadInputElementDescs[] = {
            { "Position",          0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            { "UV",                0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            { "InstancePosition",  0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
            { "InstanceSize",      0, DXGI_FORMAT_R32G32_FLOAT,    1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
            { "InstanceRotZ",      0, DXGI_FORMAT_R32_FLOAT,       1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
            { "InstanceAlpha",     0, DXGI_FORMAT_R32_FLOAT,       1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
            { "InstanceTextureID", 0, DXGI_FORMAT_R32_UINT,        1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
    };
    InitInstanceRenderData(&res.IRD[0], res.textureVertexBufferView, res.textureIndexBufferView, 6, sizeof(TextureInstanceData), 32, 0, &res.instanceDataArena);
    CreateInstancePipelineState(&res.IRD[0].PSO, state.device, res.rootSignature.Get(), &texturedQuadInputElementDescs[0], _countof(texturedQuadInputElementDescs), RESOURCES_PATH"shaders/vertex.hlsl", RESOURCES_PATH"shaders/pixel.hlsl");

    // Create Rectangle Instance Data
    const D3D12_INPUT_ELEMENT_DESC rectangleInputElementDescs[] = {
        { "Position",         0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        { "UV",               0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        { "InstancePosition", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "InstanceSize",     0, DXGI_FORMAT_R32G32_FLOAT,    1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "InstanceColor",    0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "InstanceRotZ",     0, DXGI_FORMAT_R32_FLOAT,       1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "InstanceFill",     0, DXGI_FORMAT_R32_FLOAT,       1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
    };
    InitInstanceRenderData(&res.IRD[1], res.textureVertexBufferView, res.textureIndexBufferView, 6, sizeof(DebugGeoInstanceData), 32, 1, &res.instanceDataArena);
    CreateInstancePipelineState(&res.IRD[1].PSO, state.device, res.rootSignature.Get(), &rectangleInputElementDescs[0], _countof(rectangleInputElementDescs), RESOURCES_PATH"shaders/rectangle_vertex.hlsl", RESOURCES_PATH"shaders/rectangle_pixel.hlsl");
    
    // Create Line Instance Data
    const D3D12_INPUT_ELEMENT_DESC lineInputElementDescs[] = {
        { "Position",      0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        { "StartPos",      0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "EndPos",        0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "InstanceColor", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "InstanceWidth", 0, DXGI_FORMAT_R32_FLOAT,       1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1}
    };
    InitInstanceRenderData(&res.IRD[2], res.lineVertexBufferView, res.lineIndexBufferView, lineIndexCount, sizeof(LineInstanceData), 32, 2, &res.instanceDataArena);
    CreateInstancePipelineState(&res.IRD[2].PSO, state.device, res.rootSignature.Get(), &lineInputElementDescs[0], _countof(lineInputElementDescs), RESOURCES_PATH"shaders/line_vertex.hlsl", RESOURCES_PATH"shaders/line_pixel.hlsl");

    // Create Circle Instance Data
    const D3D12_INPUT_ELEMENT_DESC circleInputElementDescs[] = {
        { "Position",         0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        { "UV",               0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        { "InstancePosition", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "InstanceSize",     0, DXGI_FORMAT_R32G32_FLOAT,    1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "InstanceColor",    0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "InstanceRotZ",     0, DXGI_FORMAT_R32_FLOAT,       1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "InstanceFill",     0, DXGI_FORMAT_R32_FLOAT,       1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
    };
    InitInstanceRenderData(&res.IRD[3], res.textureVertexBufferView, res.textureIndexBufferView, 6, sizeof(DebugGeoInstanceData), 32, 3, &res.instanceDataArena);
    CreateInstancePipelineState(&res.IRD[3].PSO, state.device, res.rootSignature.Get(), &circleInputElementDescs[0], _countof(circleInputElementDescs), RESOURCES_PATH"shaders/circle_vertex.hlsl", RESOURCES_PATH"shaders/circle_pixel.hlsl");

    // Create Sub Texture Instance Data
    const D3D12_INPUT_ELEMENT_DESC subTextureInputElementDescs[] = {
        { "Position",            0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        { "UV",                  0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        { "InstancePosition",    0, DXGI_FORMAT_R32G32B32_FLOAT,    1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "InstanceSize",        0, DXGI_FORMAT_R32G32_FLOAT,       1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "InstanceRotZ",        0, DXGI_FORMAT_R32_FLOAT,          1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "InstanceTextureID",   0, DXGI_FORMAT_R32_UINT,           1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "InstanceUVTransform", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
    };
    InitInstanceRenderData(&res.IRD[4], res.textureVertexBufferView, res.textureIndexBufferView, 6, sizeof(SubTextureInstanceData), 32, 4, &res.instanceDataArena);
    CreateInstancePipelineState(&res.IRD[4].PSO, state.device, res.rootSignature.Get(), &subTextureInputElementDescs[0], _countof(subTextureInputElementDescs), RESOURCES_PATH"shaders/subtexture_vertex.hlsl", RESOURCES_PATH"shaders/subtexture_pixel.hlsl");

	// Create Glyph Instance Data 
    const D3D12_INPUT_ELEMENT_DESC glyphInputElementDescs[] = {
        { "Bounds",              0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "UV",    	             0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "Color",	             0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "InstanceTextureID",   0, DXGI_FORMAT_R32_UINT,           1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
    };
    InitInstanceRenderData(&res.IRD[5], res.textureVertexBufferView, res.textureIndexBufferView, 0, sizeof(GlyphInstanceData), 128, 5, &res.instanceDataArena);
    CreateInstancePipelineState(&res.IRD[5].PSO, state.device, res.rootSignature.Get(), &glyphInputElementDescs[0], _countof(glyphInputElementDescs), RESOURCES_PATH"shaders/glyph.hlsl", RESOURCES_PATH"shaders/glyph.hlsl");
    return res;
}

void DX12_RendererResizeFrameBuffers(u32 width, u32 height, RendererState & state)
{
    width  < 1 ? 1 : width;
    height < 1 ? 1 : height;

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


void DX12_BeginFrame(vec4 clearColor, RendererState & state)
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
    state.cmdList->ClearRenderTargetView(rtvDescHandle, (float*)&clearColor, 0, nullptr);
    // Clear Depth Buffer
}

void DX12_Render(RendererState & state, RendererResourcesDX12 & res)
{
    // Upload Instace Buffers to GPU
    UploadArenaClear(&res.frameUploadArena);
    for (int i = 0; i < 6; i++)
    {
        if (res.IRD[i].instanceData.count > 0)
        {
            size_t uploadSize = res.IRD[i].instanceData.elementSize * res.IRD[i].instanceData.count;
            GPUAllocation uploadBuffer = UploadArenaPush(&res.frameUploadArena, uploadSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
            memcpy(uploadBuffer.pCPU, res.IRD[i].instanceData.elements, uploadSize);
            res.IRD[i].instanceBufferView.BufferLocation = uploadBuffer.pGPU;
            res.IRD[i].instanceBufferView.SizeInBytes = uploadSize;
        }
    }

    // NOTE(rordon): shouldn't change much....
    state.cmdList->SetGraphicsRootSignature(res.rootSignature.Get());
    state.cmdList->SetGraphicsRoot32BitConstants(0, 16, &res.projection.m, 0);
    ID3D12DescriptorHeap * heaps[] = { res.textureSRVHeap.Get() };
    state.cmdList->SetDescriptorHeaps(1, heaps);
    state.cmdList->RSSetViewports(1, &state.viewport);
    state.cmdList->RSSetScissorRects(1, &state.scissorRect);
    state.cmdList->IASetVertexBuffers(0, 1, &res.textureVertexBufferView);
    state.cmdList->IASetIndexBuffer(&res.textureIndexBufferView);
    state.cmdList->SetGraphicsRootDescriptorTable(1, res.textureSRVHeap->GetGPUDescriptorHandleForHeapStart());
    state.cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Get the frame upload arena, allocate enough for the instance data, create view for instance data, bind it
    for (int i = 0; i < res.instanceDrawCMDs.count; i++)
    {
        DrawInstanceCMD * cmd = (DrawInstanceCMD*)(res.instanceDrawCMDs.elements) + i;
        switch (cmd->instanceID)
        {
			case 5:
			{
				state.cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
                state.cmdList->SetPipelineState(res.IRD[cmd->instanceID].PSO);
                state.cmdList->IASetVertexBuffers(1, 1, &res.IRD[cmd->instanceID].instanceBufferView);
                state.cmdList->DrawInstanced(4, cmd->count, 0, cmd->offset);
			} break;
            default:
            {
				state.cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                state.cmdList->SetPipelineState(res.IRD[cmd->instanceID].PSO);
                state.cmdList->IASetVertexBuffers(0, 1, &res.IRD[cmd->instanceID].vertexBufferView);
                state.cmdList->IASetVertexBuffers(1, 1, &res.IRD[cmd->instanceID].instanceBufferView);
                state.cmdList->IASetIndexBuffer(&res.IRD[cmd->instanceID].indexBufferView);
                state.cmdList->DrawIndexedInstanced(res.IRD[cmd->instanceID].indexCountPerInstance, cmd->count, 0, 0, cmd->offset);
            } break;
        }
    }

    res.instanceDrawCMDs.count = 0;
}

void DX12_EndFrame(RendererState & state)
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

void InsertionSortRenderEntries(RenderSortEntry * entries, size_t numEntries)
{
    for (int i = 1; i < numEntries; i++)
    {
        RenderSortEntry key = entries[i];
        int j = i - 1;
        while (j >= 0 && entries[j].layer > key.layer)
        {
            entries[j + 1]  = entries[j];
            j = j - 1;
        }
        entries[j + 1] = key;
    }
}

void RendererPushInstance(InstanceRenderData * renderData, Array * drawCMDs, void * instanceData, u16 layer)
{
    ArrayPush(&renderData->instanceData, instanceData);

    if (renderData->frameInstanceCounter.currentLayer != layer)
    {
        if (renderData->frameInstanceCounter.layerInstanceCount > 0)
        {
            DrawInstanceCMD cmd = {renderData->instanceID, renderData->frameInstanceCounter.layerInstanceCount, renderData->frameInstanceCounter.totalInstances};
            ArrayPush(drawCMDs, &cmd);
            renderData->frameInstanceCounter.totalInstances += renderData->frameInstanceCounter.layerInstanceCount;
            renderData->frameInstanceCounter.layerInstanceCount = 0;
        }
        renderData->frameInstanceCounter.currentLayer = layer;
    }
    renderData->frameInstanceCounter.layerInstanceCount++;
}

void RendererFlushInstances(InstanceRenderData * renderData, Array * drawCMDs)
{
    if (renderData->frameInstanceCounter.layerInstanceCount > 0)
    {
        DrawInstanceCMD cmd = {renderData->instanceID, renderData->frameInstanceCounter.layerInstanceCount, renderData->frameInstanceCounter.totalInstances};
        ArrayPush(drawCMDs, &cmd);
        renderData->frameInstanceCounter.totalInstances += renderData->frameInstanceCounter.layerInstanceCount;
        renderData->frameInstanceCounter.layerInstanceCount = 0;
    }
}

void RendererClearInstances(InstanceRenderData * renderData)
{
    renderData->frameInstanceCounter.currentLayer = 0;
    renderData->frameInstanceCounter.layerInstanceCount = 0;
    renderData->frameInstanceCounter.totalInstances = 0;
    renderData->instanceData.count = 0;
}

size_t RenderEntrySizeof(void * entry)
{
	RenderEntryHeader * entryHeader = (RenderEntryHeader*)(entry);
	switch (entryHeader->type)
	{
		case RENDER_ENTRY_TYPE_CLEAR:			return sizeof(RenderEntryClear);
		case RENDER_ENTRY_TYPE_SET_PROJ:		return sizeof(RenderEntrySetProj);
		case RENDER_ENTRY_TYPE_DEBUG_RECTANGLE: return sizeof(RenderEntryDebugRectangle);
		case RENDER_ENTRY_TYPE_DEBUG_CIRCLE:	return sizeof(RenderEntryDebugCircle);
		case RENDER_ENTRY_TYPE_LINE:			return sizeof(RenderEntryLine);
		case RENDER_ENTRY_TYPE_TEXTURED_QUAD:	return sizeof(RenderEntryTexturedQuad);
		case RENDER_ENTRY_TYPE_SUB_TEXTURE:		return sizeof(RenderEntrySubTexture);
		case RENDER_ENTRY_TYPE_TEXT:
		{
			RenderEntryText * textEntry = (RenderEntryText*)entry;
			return sizeof(RenderEntryText) + textEntry->len;
		}
		default: return 0; // TODO: assert here this would be very bad
	}
}

void RendererProcessFrameSetupCMDs(RendererPushBuffer * pb)
{
	size_t offset = 0;
    for (int i = 0; i < pb->entryCount; i++)
    {
		RenderEntryHeader * entryHeader = (RenderEntryHeader*)(pb->memory + offset);
        switch (entryHeader->type)
        {
            case RENDER_ENTRY_TYPE_CLEAR:
            {
				RenderEntryClear * entry = (RenderEntryClear*)entryHeader;
				RENDERER_PIPELINE.clearColor = entry->clearColor;
            } break;
			case RENDER_ENTRY_TYPE_SET_PROJ:
			{
				RenderEntrySetProj * entry = (RenderEntrySetProj*)entryHeader;
				RENDERER_PIPELINE.projection = entry->projection;
			} break;
			default: break; // Skip.
		}
		offset += RenderEntrySizeof((void*)entryHeader);
	}
}

void RendererPushGlyphs(const RenderEntryText * entry, const u8 * text, InstanceRenderData * renderData, Array * drawCMDs, u16 layer)
{
	const FontData * fontData = GetFontAssetData(entry->fontID);
	f32 scale = entry->fontSize / fontData->fontSizePx;
	f32 penX = entry->position.x;
	f32 penY = entry->position.y;

	for (int i = 0; i < entry->len; i++)
	{
		i32 codepoint = (i32)text[i];

		auto it = fontData->glyphs.find(codepoint);
		const GlyphData* glyph = (it == fontData->glyphs.end()) 
			? &fontData->glyphs.at(0)  // fallback
			: &it->second;

		GlyphInstanceData instanceData = {0};
	    instanceData.x0 = penX + glyph->pl * scale;
        instanceData.x1 = penX + glyph->pr * scale;
        instanceData.y0 = penY + glyph->pb * scale;
        instanceData.y1 = penY + glyph->pt * scale;

        instanceData.u0 = glyph->al / fontData->atlasWidth;
        instanceData.u1 = glyph->ar / fontData->atlasWidth;
instanceData.v0 = 1.0f - (glyph->at / fontData->atlasHeight);  // flip top -> v0
instanceData.v1 = 1.0f - (glyph->ab / fontData->atlasHeight);  // flip bottom -> v1
		instanceData.textureIndex = fontData->textureHandle;

        instanceData.color = entry->color;

		RendererPushInstance(renderData, drawCMDs, &instanceData, layer);

        penX += glyph->advance * scale;	
	}
}

void DX12_RendererProcessPushBuffer(RendererPushBuffer * pb, InstanceRenderData * instanceRenderData, u32 instanceRenderDataCount, Array * drawCMDs)
{
	RendererProcessFrameSetupCMDs(pb);
    for (int i = 0; i < instanceRenderDataCount; i++)
    {
        RendererClearInstances(&instanceRenderData[i]);
    }

    InsertionSortRenderEntries(pb->sortEntries, pb->sortEntryCount);

    u16 currentLayer = 0;
    for (int i = 0; i < pb->sortEntryCount; i++)
    {
        RenderSortEntry sortEntry = pb->sortEntries[i];
        if (currentLayer != sortEntry.layer)
        {
            for (int i = 0; i < instanceRenderDataCount; i++)
            {
                RendererFlushInstances(&instanceRenderData[i], drawCMDs);
            }
            currentLayer = sortEntry.layer;
        }
        switch (sortEntry.type)
        {
            case RENDER_ENTRY_TYPE_TEXTURED_QUAD:
            {
                RenderEntryTexturedQuad * entry = (RenderEntryTexturedQuad*)(pb->memory + sortEntry.pushBufferOffset);
                TextureInstanceData instanceData = {};
                instanceData.position = entry->instanceData.position;
                instanceData.rotation = entry->instanceData.rotation;
                instanceData.scale = entry->instanceData.scale;
                instanceData.textureIndex = entry->textureID;
                instanceData.alpha = entry->instanceData.alpha;
                RendererPushInstance(&instanceRenderData[0], drawCMDs, &instanceData, sortEntry.layer);
            } break;

            case RENDER_ENTRY_TYPE_DEBUG_RECTANGLE:
            {
                RenderEntryDebugRectangle * entry = (RenderEntryDebugRectangle*)(pb->memory + sortEntry.pushBufferOffset);
                RendererPushInstance(&instanceRenderData[1], drawCMDs, &entry->instanceData, sortEntry.layer);
            } break;

            case RENDER_ENTRY_TYPE_LINE:
            {
                RenderEntryLine * entry = (RenderEntryLine*)(pb->memory + sortEntry.pushBufferOffset);
                RendererPushInstance(&instanceRenderData[2], drawCMDs, &entry->instanceData, sortEntry.layer);
            } break;
            
            case RENDER_ENTRY_TYPE_DEBUG_CIRCLE:
            {
                RenderEntryDebugCircle * entry = (RenderEntryDebugCircle*)(pb->memory + sortEntry.pushBufferOffset);
                DebugGeoInstanceData instanceData = {};
                instanceData.position = entry->position;
                instanceData.scale    = entry->scale;
                instanceData.fill     = entry->fill;
                instanceData.rotation = entry->rotation;
                instanceData.color    = entry->color;
                RendererPushInstance(&instanceRenderData[3], drawCMDs, &instanceData, sortEntry.layer);
            } break;

            case RENDER_ENTRY_TYPE_SUB_TEXTURE:
            {
                RenderEntrySubTexture * entry = (RenderEntrySubTexture*)(pb->memory + sortEntry.pushBufferOffset);
                SubTextureInstanceData instanceData = {};
                instanceData.position     = entry->position;
                instanceData.rotation     = entry->rotation;
                instanceData.scale        = entry->scale;
                instanceData.uvTransform  = entry->uvTransform;
                instanceData.textureIndex = entry->textureAtlasID;
                RendererPushInstance(&instanceRenderData[4], drawCMDs, &instanceData, sortEntry.layer);
            }break;
			case RENDER_ENTRY_TYPE_TEXT:
			{
				RenderEntryText * entry = (RenderEntryText*)(pb->memory + sortEntry.pushBufferOffset);
				u8 * text = (u8*)entry + sizeof(RenderEntryText);
				RendererPushGlyphs(entry, text, &instanceRenderData[5], drawCMDs, sortEntry.layer);
			} break;
            default:
            {
                // TODO: crashout here...
            } break;
        } 
    }

    for (int i = 0; i < instanceRenderDataCount; i++)
    {
        RendererFlushInstances(&instanceRenderData[i], drawCMDs);
    }

    pb->entryCount = 0;
    pb->sortEntryCount = 0;
    pb->index = 0;
}

int DX12_RendererCreateTexture(const ImageData * image, RendererState & state, RendererResourcesDX12  & res)
{
    u32 textureIndex = res.textureCount;

    CreateTextureResource(state.device, &res.textureResources[textureIndex], image->width, image->height, image->size);
    UpdateTextureResource(state, res.textureResources[textureIndex], image);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.PlaneSlice = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    D3D12_CPU_DESCRIPTOR_HANDLE textDescHandle = res.textureSRVHeap->GetCPUDescriptorHandleForHeapStart();
    UINT srvHandleIncrementSize = state.srvDescSize;
    textDescHandle.ptr += srvHandleIncrementSize * textureIndex;

    state.device->CreateShaderResourceView(res.textureResources[textureIndex].Get(), &srvDesc, textDescHandle);

    res.textureCount++;
    return textureIndex;
}

void InitializeRenderer(HWND windowHandle, bool enableVSync, u32 width, u32 height)
{
    RENDERER_STATE    = DX12_InitializeRenderer(windowHandle, USE_WARP, enableVSync, width, height);
    RENDERER_PIPELINE = InitInstancePipelineResources(RENDERER_STATE);
}

void RendererResizeFramebuffers(u32 width, u32 height)
{
    DX12_RendererResizeFrameBuffers(width, height, RENDERER_STATE);
}

int RendererCreateTexture(const ImageData * image)
{
    return DX12_RendererCreateTexture(image, RENDERER_STATE, RENDERER_PIPELINE);
}

void RendererProcessPushBuffer(RendererPushBuffer * pb)
{
    DX12_RendererProcessPushBuffer(pb, &RENDERER_PIPELINE.IRD[0], 6, &RENDERER_PIPELINE.instanceDrawCMDs);
}

void BeginFrame()
{
    DX12_BeginFrame(RENDERER_PIPELINE.clearColor,  RENDERER_STATE);
}

void EndFrame()
{
    DX12_EndFrame(RENDERER_STATE);
}

void Render()
{
    DX12_Render(RENDERER_STATE, RENDERER_PIPELINE);
}
