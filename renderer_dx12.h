#ifndef RENDERER_DX12_H
#define RENDERER_DX12_H

#include "core.h"

#include <wrl.h>
using namespace Microsoft::WRL;

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

//#include <d3dx12.h>

//#define _DEBUG

const static UINT NUM_FRAMES = 2;

typedef struct {
    ComPtr<IDXGIAdapter4> adapter;
    ComPtr<ID3D12Device2> device;
    ComPtr<ID3D12CommandQueue> cmdQueue;
    ComPtr<IDXGISwapChain4> swapChain;
    ComPtr<ID3D12Resource> backBuffers[NUM_FRAMES];
    ComPtr<ID3D12CommandAllocator> cmdAllocators[NUM_FRAMES];
    ComPtr<ID3D12GraphicsCommandList> cmdList;
    ComPtr<ID3D12Fence> fence;
    ComPtr<ID3D12DescriptorHeap> rtvDescHeap;
    UINT rtvDescSize;
    UINT srvDescSize;
    ComPtr<ID3D12DescriptorHeap> dsvDescHeap;
    // Syncronization
    u64 fenceValue;
    u64 fenceValues[NUM_FRAMES];
    HANDLE fenceEvent;
    UINT backBufferIndex;
    // Viewport
    D3D12_RECT scissorRect;
    D3D12_VIEWPORT viewport;
    bool tearingSupported;
    bool vSyncEnabled;
} RendererState;

typedef struct 
{
    u32 instanceID;
    u32 count;
    u32 offset;
} DrawInstanceCMD;

typedef struct 
{
    ID3D12Resource * resource;
    size_t size;
    size_t index;
    void * pCPU;
    D3D12_GPU_VIRTUAL_ADDRESS pGPU;
} UploadArena;

typedef struct
{
    void * pCPU;
    D3D12_GPU_VIRTUAL_ADDRESS pGPU;
    u64 offset;
} GPUAllocation;

typedef struct {
    // Shaders
    ID3DBlob * textureVertexShaderBlob;
    ID3DBlob * texturePixelShaderBlob;

    ID3DBlob * rectangleVertexShaderBlob;
    ID3DBlob * rectanglePixelShaderBlob;

    ID3DBlob * lineVertexShaderBlob;
    ID3DBlob * linePixelShaderBlob;

    // Root Signatures
    ComPtr<ID3D12RootSignature> rootSignature;

    // Pipeline State Objects
    ComPtr<ID3D12PipelineState> texturePSO;
    ComPtr<ID3D12PipelineState> rectPSO;
    ComPtr<ID3D12PipelineState> linePSO;

    // Allocators
    UploadArena frameUploadArena;

    // Resources
    ComPtr<ID3D12Resource> textureVertexBuffer;
    ComPtr<ID3D12Resource> textureIndexBuffer;
    D3D12_VERTEX_BUFFER_VIEW textureVertexBufferView;
    D3D12_INDEX_BUFFER_VIEW textureIndexBufferView;

    ComPtr<ID3D12Resource> lineVertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW lineVertexBufferView;

    ComPtr<ID3D12Resource> lineIndexBuffer;
    D3D12_INDEX_BUFFER_VIEW lineIndexBufferView;
    u16 lineIndexCount;

    D3D12_VERTEX_BUFFER_VIEW textureInstanceBufferView;
    D3D12_VERTEX_BUFFER_VIEW rectInstanceBufferView;
    D3D12_VERTEX_BUFFER_VIEW lineInstanceBufferView;


    // TODO(get rid of this abomination): create a renderer arena and make this a SLL.
    ComPtr<ID3D12DescriptorHeap> textureSRVHeap;
    u32 textureCount;
    u32 maxTexures;
    ComPtr<ID3D12Resource> textureResources[32];

    // Instance Data Arrays
    Array quadInstances;
    Array rectangleInstances;
    Array lineInstances;
    Array subTextureInstances;
    Array instanceDrawCMDs;

    // Matrices
    mat4 projection;
} RendererResourcesDX12;

// GLOBAL VARIABLES X_X
RendererState RENDERER_STATE = {};
RendererResourcesDX12 RENDERER_PIPELINE = {};
const bool USE_WARP = false;

TextureInstanceData quadInstanceData[32] = {};
DebugGeoInstanceData rectangleInstanceData[32] = {};
LineInstanceData lineInstanceData[32] = {};
SubTextureInstanceData subTextureInstanceData[32] = {};
DrawInstanceCMD INSTANCE_DRAW_CMDS[64] = {};

#endif // RENDERER_DX12_H