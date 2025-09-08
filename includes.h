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
#include <math.h>
#include <algorithm>

typedef uint8_t   u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef uint32_t b32; // 32-bit Boolean

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

typedef union {
    float elements[3];
    struct
    {
        union { float x, r; };
        union { float y, g; };
        union { float z, b; };
    };
} vec3;

typedef union {
    float elements[2];
    struct
    {
        union { float x, u; };
        union { float y, v; };
    };
} vec2;

typedef struct {
    vec3 position;
    vec2 UV;
} VertexPosUV;

typedef struct {
    vec3 position;
    vec2 scale;
    float rotation;
} InstanceData2D;

typedef struct {
    void * data;
    u64 size;
} DEBUG_FileResult;

#endif // INCLUDES_H