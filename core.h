#ifndef CORE_H
#define CORE_H

// Shared definitions between 'engine' and game code.

#include <stdint.h>
#include <math.h>

typedef uint8_t   u8; // 8-bit unsigned int
typedef uint16_t u16; // 16-bit unsigned int
typedef uint32_t u32;
typedef uint64_t u64;
typedef uint32_t b32; // 32-bit Boolean
typedef float f32;    // 32-bit Float

#define KB(x) (x * 1024)
#define MB(x) (KB(x) * 1024)
#define GB(x) (MB(x) * 1024)

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
    float elements[4];
    struct
    {
        union { float x, r; };
        union { float y, g; };
        union { float z, b; };
        union { float w, a; };
    };
} vec4;

typedef struct 
{
    float m[4][4];
} mat4;

typedef union {
    float elements[2];
    struct
    {
        union { float x, u; };
        union { float y, v; };
    };
} vec2;

typedef union {
    int elements[2];
    struct
    {
        union { int x, u; };
        union { int y, v; };
    };
} vec2i;

typedef struct {
    vec3 position;
    vec2 scale;
    float rotation;
} InstanceData2D;

typedef struct {
    vec3 position;
    vec2 scale;
    vec3 color;
    float rotation;
    float fill;
} DebugGeoInstanceData;

typedef struct {
    vec3 startPos;
    vec3 endPos;
    vec3 color;
    float width;
} DebugLineInstanceData;

typedef struct {
    InstanceData2D * data;
    u32 maxInstances;
    u32 instanceCount;
} InstanceBuffer;


typedef struct {
    void * permStorage;
    u64 permStorageSize;
} GameMemory;

typedef struct {
    double time;
    vec4 clearColor;
    vec3 cameraPos;
    vec2 tempPlayerPos;
    vec2 tempInput;
} GameState;

#define GAME_START_FUNCTION(name) void name(GameMemory * gameMemory)
typedef GAME_START_FUNCTION(GameStartFunction);

#define GAME_UPDATE_FUNCTION(name) void name(GameMemory * gameMemory, InstanceBuffer * instanceBuffer, double deltaTime)
typedef GAME_UPDATE_FUNCTION(GameUpdateFunction);

#endif // CORE_H