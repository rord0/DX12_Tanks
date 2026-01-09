#ifndef CORE_H
#define CORE_H

// Shared definitions between 'engine' and game code.

#include <stdint.h>
#include <math.h>
#include <string.h>

typedef uint8_t   u8; // 8-bit unsigned int
typedef uint16_t u16; // 16-bit unsigned int
typedef uint32_t u32;
typedef uint64_t u64;
typedef uint32_t b32; // 32-bit Boolean
typedef float f32;    // 32-bit Float

#define KB(x) (x * 1024)
#define MB(x) (KB(x) * 1024)
#define GB(x) (MB(x) * 1024)

#define RESOURCES_PATH "../res/"
#define PI 3.14159265358979323846

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

struct vec2 {
    union
    {
        float elements[2];
        struct { float x, y; };
        struct { float u, v; };
    };
};

vec2 operator+(vec2 A, vec2 B) { return {A.x + B.x, A.y + B.y}; }

vec2 operator-(vec2 A, vec2 B) { return {A.x - B.x, A.y - B.y}; }
vec2 operator-(vec2 A)         { return {-A.x, -A.y}; }

vec2 operator*(f32 c, vec2 V)  { return {c * V.x, c * V.y}; }
vec2 operator*(vec2 V, f32 c)  { return c * V; }

vec2 & operator*=(vec2 & V, f32 c)
{
    V = c * V;
    return V;
}

vec2 & operator+=(vec2 & A, vec2 B)
{
    A = A + B;
    return A;
}

vec2 & operator-=(vec2 & A, vec2 B)
{
    A = A - B;
    return A;
}

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
    float rotation;
    u32 textureIndex;
} TextureInstanceData;

typedef struct {
    vec3 position;
    vec2 scale;
    float rotation;
    u32 textureIndex;
    vec4 uvTransform;
} SubTextureInstanceData;

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
} LineInstanceData;

typedef struct {
    void * data;
    u64 size;
} DEBUG_FileResult;

#include "render_entry.h"
#include "array.h"

////////////////////
// Function Typedefs
#define PLATFORM_LOAD_TEXTURE(name) u32 name(const char * textureName)
typedef PLATFORM_LOAD_TEXTURE(PlatformLoadTextureFunction);

#define PLATFORM_LOAD_FILE(name) DEBUG_FileResult name(const char * filepath)
typedef PLATFORM_LOAD_FILE(PlatformLoadFileFunction);

#define PLATFORM_FREE_FILE(name) void name(void ** memory)
typedef PLATFORM_FREE_FILE(PlatformFreeFileFunction);

typedef struct {
    void * permStorage;
    u64 permStorageSize;
    void * transientStorage;
    u64 transStorageSize;
    PlatformLoadTextureFunction * platformLoadTexture;
    PlatformLoadFileFunction * platformLoadFile;
    PlatformFreeFileFunction * platformFreeFile;
} GameMemory;

typedef struct {
	vec2 tempInput;
	bool isMousePressed;
	double deltaTime;
} GameInput;

#define GAME_START_FUNCTION(name) void name(GameMemory * gameMemory)
typedef GAME_START_FUNCTION(GameStartFunction);

#define GAME_UPDATE_FUNCTION(name) void name(GameMemory * gameMemory, GameInput * input, RendererPushBuffer * renderCommands)
typedef GAME_UPDATE_FUNCTION(GameUpdateFunction);

#endif // CORE_H
