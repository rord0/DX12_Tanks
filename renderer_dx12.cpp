#include "array.h"

#include "includes.h"

#include "renderer_dx12.h"
#include "render_entry.h"

#include "render.cpp"

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


const D3D12_INPUT_ELEMENT_DESC texureInputElementDescs[] = {
        { "Position",         0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        { "UV",               0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        { "InstancePosition", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "InstanceSize",     0, DXGI_FORMAT_R32G32_FLOAT,    1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "InstanceRotZ",     0, DXGI_FORMAT_R32_FLOAT,       1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
};

const D3D12_INPUT_ELEMENT_DESC rectInputElementDescs[] = {
        { "Position",         0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        { "UV",               0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        { "InstancePosition", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "InstanceSize",     0, DXGI_FORMAT_R32G32_FLOAT,    1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "InstanceColor",    0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "InstanceRotZ",     0, DXGI_FORMAT_R32_FLOAT,       1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "InstanceFill",     0, DXGI_FORMAT_R32_FLOAT,       1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
};

const D3D12_INPUT_ELEMENT_DESC lineElementDescs[] = {
        { "Position",      0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        { "StartPos",      0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "EndPos",        0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "InstanceColor", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        { "InstanceWidth", 0, DXGI_FORMAT_R32_FLOAT,       1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1}
};


void InitRendererResources(RendererResourcesDX12 & res, RendererState & state)
{
    ////////////
    // Geometry
    const VertexPosUV quadVertices[4] = {{{-0.25,  0.25, 0.0}, {0.0, 0.0}},    // Top Left     (0,0)---(1,0)
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

    ImageData gdEasyData = LoadImageFromFile(RESOURCES_PATH"gd_easy.png");
    ImageData gdNormalData = LoadImageFromFile(RESOURCES_PATH"gd_normal.png");
    ImageData gdHardData = LoadImageFromFile(RESOURCES_PATH"gd_hard.png");
    ImageData gdHarderData = LoadImageFromFile(RESOURCES_PATH"gd_harder.png");

    DX12_RendererCreateTexture(&gdEasyData, state, res);
    DX12_RendererCreateTexture(&gdNormalData, state, res);
    DX12_RendererCreateTexture(&gdHardData, state, res);
    DX12_RendererCreateTexture(&gdHarderData, state, res);

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

    res.textureInstanceBufferView.BufferLocation = D3D12_GPU_VIRTUAL_ADDRESS(0);
    res.textureInstanceBufferView.SizeInBytes = sizeof(quadInstanceData);
    res.textureInstanceBufferView.StrideInBytes = sizeof(InstanceData2D);

    res.rectInstanceBufferView.SizeInBytes = sizeof(rectangleInstanceData);
    res.rectInstanceBufferView.StrideInBytes = sizeof(DebugGeoInstanceData);

    res.lineInstanceBufferView.SizeInBytes = sizeof(lineInstanceData);
    res.lineInstanceBufferView.StrideInBytes = sizeof(LineInstanceData);

    // Free temp upload arena.

    res.quadInstances = {sizeof(InstanceData2D), sizeof(quadInstanceData), 0, (void*)quadInstanceData};
    res.rectangleInstances = {sizeof(DebugGeoInstanceData), sizeof(rectangleInstanceData)/sizeof(DebugGeoInstanceData), 0, (void*)rectangleInstanceData};
    res.lineInstances = {sizeof(LineInstanceData), sizeof(lineInstanceData)/sizeof(LineInstanceData), 0, (void*)lineInstanceData};
    res.subTextureInstances = {sizeof(SubTextureInstanceData), sizeof(subTextureInstanceData)/sizeof(SubTextureInstanceData), 0, (void*)subTextureInstanceData};
    res.lineIndexCount = lineIndexCount;

    UploadArenaRelease(&tempUploadArena);
}

RendererResourcesDX12 InitRendererPipeline(RendererState & state)
{
    RendererResourcesDX12 res = {};
    //////////////////////
    // Shader Compilation
    res.textureVertexShaderBlob = CompileShaderFromFile(RESOURCES_PATH"shaders/vertex.hlsl", "vertex.hlsl", "main", "vs_5_1");
    res.texturePixelShaderBlob = CompileShaderFromFile(RESOURCES_PATH"shaders/pixel.hlsl", "pixel.hlsl", "main", "ps_5_1");

    res.rectangleVertexShaderBlob = CompileShaderFromFile(RESOURCES_PATH"shaders/rectangle_vertex.hlsl", "rectangle_vertex.hlsl", "main", "vs_5_1");
    res.rectanglePixelShaderBlob = CompileShaderFromFile(RESOURCES_PATH"shaders/rectangle_pixel.hlsl", "rectangle_pixel.hlsl", "main", "ps_5_1");

    res.lineVertexShaderBlob = CompileShaderFromFile(RESOURCES_PATH"shaders/line_vertex.hlsl", "line_vertex.hlsl", "main", "vs_5_1");
    res.linePixelShaderBlob = CompileShaderFromFile(RESOURCES_PATH"shaders/line_pixel.hlsl", "line_pixel.hlsl", "main", "ps_5_1");

    /////////////////
    // Root Signature
    res.rootSignature = CreateRootSignature(state.device);

    //////////////////////////////
    // Create Pipeline State Descs
    D3D12_SHADER_BYTECODE lineVertexShaderBytecode = {res.lineVertexShaderBlob->GetBufferPointer(), res.lineVertexShaderBlob->GetBufferSize()};
    D3D12_SHADER_BYTECODE linePixelShaderBytecode = {res.linePixelShaderBlob->GetBufferPointer(), res.linePixelShaderBlob->GetBufferSize()};

    D3D12_SHADER_BYTECODE textureVertexShaderBytecode = {res.textureVertexShaderBlob->GetBufferPointer(), res.textureVertexShaderBlob->GetBufferSize()};
    D3D12_SHADER_BYTECODE texturePixelShaderBytecode = {res.texturePixelShaderBlob->GetBufferPointer(), res.texturePixelShaderBlob->GetBufferSize()};

    D3D12_SHADER_BYTECODE rectVertexShaderBytecode = {res.rectangleVertexShaderBlob->GetBufferPointer(), res.rectangleVertexShaderBlob->GetBufferSize()};
    D3D12_SHADER_BYTECODE rectPixelShaderBytecode = {res.rectanglePixelShaderBlob->GetBufferPointer(), res.rectanglePixelShaderBlob->GetBufferSize()};

    D3D12_GRAPHICS_PIPELINE_STATE_DESC texturePipelineStateDesc = createQuadPipelineStateDesc(res.rootSignature.Get(), texureInputElementDescs, _countof(texureInputElementDescs), textureVertexShaderBytecode, texturePixelShaderBytecode);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC rectPipelineStateDesc = createQuadPipelineStateDesc(res.rootSignature.Get(), rectInputElementDescs, _countof(rectInputElementDescs), rectVertexShaderBytecode, rectPixelShaderBytecode);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC linePipelineStateDesc = createQuadPipelineStateDesc(res.rootSignature.Get(), lineElementDescs, _countof(lineElementDescs), lineVertexShaderBytecode, linePixelShaderBytecode);

    AssertIfFailed(state.device->CreateGraphicsPipelineState(&texturePipelineStateDesc, IID_PPV_ARGS(&res.texturePSO)));
    AssertIfFailed(state.device->CreateGraphicsPipelineState(&rectPipelineStateDesc, IID_PPV_ARGS(&res.rectPSO)));
    AssertIfFailed(state.device->CreateGraphicsPipelineState(&linePipelineStateDesc, IID_PPV_ARGS(&res.linePSO)));

    /////////////////////
    // Create Allocators
    res.frameUploadArena = UploadArenaAlloc(state.device, KB(4));
    res.frameUploadArena.resource->SetName(L"Frame Upload Resource");


    ////////////////////
    // Create Resources
    InitRendererResources(res, state);

    return res;
}

float DX12_RendererResizeFrameBuffers(u32 width, u32 height, RendererState & state)
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

    return (float)width / (float)height;
}


void DX12_BeginFrame(float clearColor[4], RendererState & state)
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

void DX12_Render(RendererState & state, RendererResourcesDX12 & res)
{
    // Upload Instance Data
    UploadArenaClear(&res.frameUploadArena);

    GPUAllocation dynAlloc = UploadArenaPush(&res.frameUploadArena, sizeof(quadInstanceData), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    memcpy(dynAlloc.pCPU, quadInstanceData, sizeof(quadInstanceData));
    res.textureInstanceBufferView.BufferLocation = dynAlloc.pGPU;

    GPUAllocation rectAlloc = UploadArenaPush(&res.frameUploadArena, sizeof(rectangleInstanceData), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    memcpy(rectAlloc.pCPU, rectangleInstanceData, sizeof(rectangleInstanceData));
    res.rectInstanceBufferView.BufferLocation = rectAlloc.pGPU;

    GPUAllocation lineAlloc = UploadArenaPush(&res.frameUploadArena, sizeof(lineInstanceData), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    memcpy(lineAlloc.pCPU, lineInstanceData, sizeof(lineInstanceData));
    res.lineInstanceBufferView.BufferLocation = lineAlloc.pGPU;

    // NOTE(rordon): shouldn't change much....
    state.cmdList->SetGraphicsRootSignature(res.rootSignature.Get());
    state.cmdList->SetGraphicsRoot32BitConstants(0, 16, &res.projection.m, 0);
    ID3D12DescriptorHeap * heaps[] = { res.textureSRVHeap.Get() };
    state.cmdList->SetDescriptorHeaps(1, heaps);
    state.cmdList->RSSetViewports(1, &state.viewport);
    state.cmdList->RSSetScissorRects(1, &state.scissorRect);
    state.cmdList->IASetVertexBuffers(0, 1, &res.textureVertexBufferView);
    state.cmdList->IASetIndexBuffer(&res.textureIndexBufferView);

    // TODO(rordon): Render rectangles
    // TODO(rordon): Render circles 
    // TODO(rordon): Render lines??? 
    // TODO(rordon): Render Textured Quads. 

    // Get the frame upload arena, allocate enough for the instance data, create view for instance data, bind it
    state.cmdList->SetPipelineState(res.texturePSO.Get());
    state.cmdList->SetGraphicsRootDescriptorTable(1, res.textureSRVHeap->GetGPUDescriptorHandleForHeapStart());
    state.cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    state.cmdList->IASetVertexBuffers(1, 1, &res.textureInstanceBufferView);

    D3D12_GPU_DESCRIPTOR_HANDLE textureHeapStart = res.textureSRVHeap->GetGPUDescriptorHandleForHeapStart();
    for (int i = 1; i < 5; i++)
    {
        D3D12_GPU_DESCRIPTOR_HANDLE textureDesc = textureHeapStart; 
        textureDesc.ptr = textureHeapStart.ptr + state.srvDescSize * i;
        state.cmdList->SetGraphicsRootDescriptorTable(1, textureDesc);
        state.cmdList->DrawIndexedInstanced(6, 1, 0, 0, i - 1);
    }

    // Draw Rectangle Instances
    state.cmdList->SetPipelineState(res.rectPSO.Get());
    state.cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    state.cmdList->IASetVertexBuffers(1, 1, &res.rectInstanceBufferView);
    state.cmdList->DrawIndexedInstanced(6, res.rectangleInstances.count, 0, 0, 0);

    // Draw Line Instances
    // Iterate over sorted draws
    state.cmdList->SetPipelineState(res.linePSO.Get());
    state.cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    state.cmdList->IASetVertexBuffers(0, 1, &res.lineVertexBufferView);
    state.cmdList->IASetVertexBuffers(1, 1, &res.lineInstanceBufferView);
    state.cmdList->IASetIndexBuffer(&res.lineIndexBufferView);
    state.cmdList->DrawIndexedInstanced(res.lineIndexCount, res.lineInstances.count, 0, 0, 0);

    res.rectangleInstances.count = 0;
    res.quadInstances.count = 0;
    res.lineInstances.count = 0;
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

void DX12_RendererProcessPushBuffer(RendererPushBuffer * pb, Array * quadInstances, Array * rectInstances, Array * lineInstances, Array * subTextureInstances)
{
    size_t entryOffset = 0;
    while (entryOffset < pb->index)
    {
        RenderEntryHeader * header = (RenderEntryHeader*)(pb->memory + entryOffset);
        switch (header->type)
        {
            case RENDER_ENTRY_TYPE_CLEAR:
            {
                entryOffset += sizeof(RenderEntryClear);
            } break;

            case RENDER_ENTRY_TYPE_DEBUG_RECTANGLE:
            {
                RenderEntryDebugRectangle * entry = (RenderEntryDebugRectangle*)header;
                ArrayPush(rectInstances, &entry->instanceData);
                entryOffset += sizeof(RenderEntryDebugRectangle);
            } break;

            case RENDER_ENTRY_TYPE_DEBUG_CIRCLE:
            {
                entryOffset += sizeof(RenderEntryDebugCircle);
            } break;

            case RENDER_ENTRY_TYPE_TEXTURED_QUAD:
            {
                RenderEntryTexturedQuad * entry = (RenderEntryTexturedQuad*)header;
                ArrayPush(quadInstances, &entry->instanceData);
                entryOffset += sizeof(RenderEntryTexturedQuad);
            } break;

            case RENDER_ENTRY_TYPE_LINE:
            {
                RenderEntryLine * entry = (RenderEntryLine*)header;
                ArrayPush(lineInstances, &entry->instanceData);
                entryOffset += sizeof(RenderEntryLine);
            } break;

            case RENDER_ENTRY_TYPE_SUB_TEXTURE:
            {
                RenderEntrySubTexture * entry = (RenderEntrySubTexture*)header;
                ArrayPush(subTextureInstances, &entry->instanceData);
                entryOffset += sizeof(RenderEntrySubTexture);
            }
            default:
            {
                // TODO: crashout here...
            } break;
        } 
    }

    pb->entryCount = 0;
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
    RENDERER_STATE = DX12_InitializeRenderer(windowHandle, USE_WARP, enableVSync, width, height);
    RENDERER_PIPELINE = InitRendererPipeline(RENDERER_STATE);
}

float RendererResizeFramebuffers(u32 width, u32 height)
{
    return DX12_RendererResizeFrameBuffers(width, height, RENDERER_STATE);
}

int RendererCreateTexture(const ImageData * image)
{
    return DX12_RendererCreateTexture(image, RENDERER_STATE, RENDERER_PIPELINE);
}

void RendererProcessPushBuffer(RendererPushBuffer * pb)
{
    DX12_RendererProcessPushBuffer(pb, &RENDERER_PIPELINE.quadInstances, &RENDERER_PIPELINE.rectangleInstances, &RENDERER_PIPELINE.lineInstances, &RENDERER_PIPELINE.subTextureInstances);
}

void BeginFrame(float clearColor[4], mat4 projectionMatrix)
{
    DX12_BeginFrame(clearColor,  RENDERER_STATE);
    RENDERER_PIPELINE.projection = projectionMatrix;
}

void EndFrame()
{
    DX12_EndFrame(RENDERER_STATE);
}

void Render()
{
    DX12_Render(RENDERER_STATE, RENDERER_PIPELINE);
}