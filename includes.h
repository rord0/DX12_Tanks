#ifndef INCLUDES_H
#define INCLUDES_H

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define UNICODE

#include <windows.h>
#include <Xinput.h>
#include <stdint.h>
#include <mmeapi.h>
#include <dsound.h>
#include <algorithm>


#include <wrl.h>
using namespace Microsoft::WRL;

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

//#include <d3dx12.h>

#define _DEBUG

#include "core.h"

typedef struct
{
    HMODULE DLL;
    GameUpdateFunction * Update;
    GameStartFunction * Start;
    bool isValid;
    FILETIME lastWriteTime;
} Win32GameCode;

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
    vec3 position;
    vec2 UV;
} VertexPosUV;

typedef struct {
    void * data;
    u64 size;
} DEBUG_FileResult;

typedef struct {
    int width;
    int height;
    int numComponents;
    int size;
    u8 * memory;
} ImageData;


#endif // INCLUDES_H