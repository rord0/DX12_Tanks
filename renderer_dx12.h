#ifndef RENDERER_DX12_H
#define RENDERER_DX12_H

#include "core.h"
#include "arena.h"

#include <cstddef>
#include <iterator>
#include <wrl.h>
using namespace Microsoft::WRL;

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

//#include <d3dx12.h>

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
	mat4 VP;
	mat4 projection;
} SetProjCMD;

enum class DrawCommandType {
	DRAW_COMMAND_INSTANCE,
	DRAW_COMMAND_SET_PROJ,
};

typedef struct 
{
	DrawCommandType type;
	union
	{
		SetProjCMD setProj;
		DrawInstanceCMD instance;
	};
} DrawCMD;

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

typedef struct 
{
    u32 totalInstances;
    u16 currentLayer;
    u32 layerInstanceCount;
} TEMP_Frame_Instance_Counter;

typedef struct 
{
    u32 instanceID;
    ID3D12PipelineState * PSO;

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    D3D12_VERTEX_BUFFER_VIEW instanceBufferView;
    D3D12_INDEX_BUFFER_VIEW indexBufferView;

    u32 indexCountPerInstance;
	size_t instanceSize;

    TEMP_Frame_Instance_Counter frameInstanceCounter;
    Array instanceData;
} InstanceRenderData;

typedef struct RendererResourcesDX12 {
    // Shaders
    ID3DBlob * lineVertexShaderBlob;
    ID3DBlob * linePixelShaderBlob;

    // Root Signatures
    ComPtr<ID3D12RootSignature> rootSignature;

    // Allocators
    UploadArena frameUploadArena;

    // Resources
    ComPtr<ID3D12Resource> textureVertexBuffer;
    ComPtr<ID3D12Resource> textureIndexBuffer;
    D3D12_VERTEX_BUFFER_VIEW textureVertexBufferView;
    D3D12_INDEX_BUFFER_VIEW textureIndexBufferView;

    ComPtr<ID3D12Resource> lineVertexBuffer;
    ComPtr<ID3D12Resource> lineIndexBuffer;
    D3D12_VERTEX_BUFFER_VIEW lineVertexBufferView;
    D3D12_INDEX_BUFFER_VIEW lineIndexBufferView;

    // TODO(get rid of this abomination): create a renderer arena and make this a SLL.
    ComPtr<ID3D12DescriptorHeap> textureSRVHeap;
    u32 textureCount;
    u32 maxTexures;
    ComPtr<ID3D12Resource> textureResources[32];

	Arena permanentArena;
    Arena instanceDataArena;
    Array drawCMDs;

    InstanceRenderData IRD[9];
    static constexpr size_t instanceCount = _countof(IRD);
	vec4 clearColor;
    // Matrices
    mat4 projection;
} RendererResourcesDX12;

// GLOBAL VARIABLES X_X
RendererState RENDERER_STATE = {};
RendererResourcesDX12 RENDERER_PIPELINE = {};
const bool USE_WARP = false;

#endif // RENDERER_DX12_H
