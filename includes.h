#ifndef INCLUDES_H
#define INCLUDES_H

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define UNICODE

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h> 
#include <Xinput.h>
#include <stdint.h>
#include <mmeapi.h>
#include <dsound.h>
#include <algorithm>
#include <unordered_map>

#include "core.h"

typedef struct
{
    HMODULE DLL;
    GameUpdateFunction * Update;
    GameStartFunction * Start;
    bool isValid;
    FILETIME lastWriteTime;
} Win32GameCode;

typedef struct {
    vec3 position;
    vec2 UV;
} VertexPosUV;

typedef struct {
    int width;
    int height;
    int numComponents;
    int size;
    u8 * memory;
} ImageData;

#endif // INCLUDES_H
