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
#include <stdio.h>

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

#endif // INCLUDES_H