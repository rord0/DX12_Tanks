#ifndef CORE_H
#define CORE_H

// Shared definitions between 'engine' and game code.

#include <cstddef>
#include <cstdint>
#include <stdint.h>
#include <math.h>
#include <string.h>

typedef uint8_t   u8; // 8-bit unsigned int
typedef uint16_t u16; // 16-bit unsigned int
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t  i32;
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

vec4 operator*(mat4 m, vec4 V)
{
	return vec4{
			(V.x * m.m[0][0]) + (V.y * m.m[0][1]) + (V.z * m.m[0][2]) + (V.w * m.m[0][3]),
			(V.x * m.m[1][0]) + (V.y * m.m[1][1]) + (V.z * m.m[1][2]) + (V.w * m.m[1][3]),
			(V.x * m.m[2][0]) + (V.y * m.m[2][1]) + (V.z * m.m[2][2]) + (V.w * m.m[2][3]),
			(V.x * m.m[3][0]) + (V.y * m.m[3][1]) + (V.z * m.m[3][2]) + (V.w * m.m[3][3])};
}


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
vec2 operator/(vec2 V, f32 c)  { return {V.x / c, V.y / c}; }

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

struct vec2i {
    union
    {
        int elements[2];
        struct { int x, y; };
        struct { int u, v; };
    };
};

typedef struct {
    vec3 position;
    vec2 scale;
    float rotation;
	float alpha;
} InstanceData2D;

typedef struct {
    vec3 position;
    vec2 scale;
    float rotation;
    float alpha;
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
    vec4 color;
    float width;
} LineInstanceData;

typedef struct {
    f32 x0, y0, x1, y1;
    f32 u0, v0, u1, v1;
	vec4 color;
    u32 textureIndex;
	f32 pad0, pad1, pad2;
} GlyphInstanceData;

typedef struct {
	vec2 position;
	vec2 size;
	vec4 fillColor;
	vec4 strokeColor;
	float cornerRadius;
	f32 pad0, pad1, pad2;
} SDFRectInstanceData;

typedef struct {
    void * data;
    u64 size;
} DEBUG_FileResult;

typedef enum {
	NET_EVENT_CLIENT_CONNECTED,
	NET_EVENT_CLIENT_DISCONNECTED,
	NET_EVENT_PACKET,
} NetworkEventType;

typedef struct {
	size_t size;
	u32 id;
	void * data;
} NetworkPacket;

typedef struct {
	NetworkEventType type;
	u32 connID;
	NetworkPacket * packet; 
} NetworkEvent;

#include "render_entry.h"
#include "array.h"

////////////////////
// Function Typedefs
#define PLATFORM_LOAD_TEXTURE(name) u32 name(const char * textureName)
typedef PLATFORM_LOAD_TEXTURE(PlatformLoadTextureFunction);

#define PLATFORM_LOAD_FONT_ATLAS(name) i32 name(const char * atlasPath, const char * metadataPath)
typedef PLATFORM_LOAD_FONT_ATLAS(PlatformLoadFontAtlasFn);

#define PLATFORM_MEASURE_TEXT(name) vec2 name(i32 fontID, const char * text, u32 len, f32 scale)
typedef PLATFORM_MEASURE_TEXT(PlatformMeasureTextFn);

#define PLATFORM_LOAD_FILE(name) DEBUG_FileResult name(const char * filepath)
typedef PLATFORM_LOAD_FILE(PlatformLoadFileFunction);

#define PLATFORM_FREE_FILE(name) void name(void ** memory)
typedef PLATFORM_FREE_FILE(PlatformFreeFileFunction);

#define PLATFORM_START_SERVER(name) bool name(uint16_t port, uint16_t maxConnections)
typedef PLATFORM_START_SERVER(PlatformStartServerFunction);

#define PLATFORM_START_CLIENT(name) bool name(const char * addressStr, uint16_t serverPort)
typedef PLATFORM_START_CLIENT(PlatformStartClientFn);

#define PLATFORM_CLIENT_SEND(name) void name(void * data, size_t size, uint16_t sendMode)
typedef PLATFORM_CLIENT_SEND(PlatformClientSendFn);

#define PLATFORM_SERVER_SEND(name) void name(void * data, size_t size, uint16_t sendMode, u32 clientIndex)
typedef PLATFORM_SERVER_SEND(PlatformServerSendFn);

#define PLATFORM_SERVER_GET_EVENT(name) bool name(NetworkEvent ** outEvent)
typedef PLATFORM_SERVER_GET_EVENT(PlatformServerGetEventFn);

typedef struct {
    PlatformLoadTextureFunction * platformLoadTexture;
    PlatformLoadFileFunction * platformLoadFile;
    PlatformFreeFileFunction * platformFreeFile;
    PlatformStartServerFunction * platformStartServer;
    PlatformStartClientFn * platformStartClient;
	PlatformClientSendFn * platformClientSend;
	PlatformServerSendFn * platformServerSend;
	PlatformServerGetEventFn * serverGetEvent;
	PlatformLoadFontAtlasFn * loadFont;
	PlatformMeasureTextFn * measureText;
} PlatformAPI;

typedef struct {
    void * permStorage;
    u64 permStorageSize;
    void * transientStorage;
    u64 transStorageSize;
	PlatformAPI platform;
} GameMemory;

typedef struct
{
    bool isDown;
    bool wasDown;
} KeyInput;

typedef struct {
	KeyInput WASD[4];
	KeyInput ARROWS[4];
	bool isMousePressed;
	bool isEnterPressed;
	bool isSpacePressed;
	double deltaTime;
	vec2i viewportSize;
	vec2i mousePosVP;
	// ---- Networking ----
	NetworkEvent * clientEvents;
	u32 clientEventCount;
} GameInput;

#define GAME_START_FUNCTION(name) void name(GameMemory * gameMemory, int argc, char ** argv)
typedef GAME_START_FUNCTION(GameStartFunction);

#define GAME_UPDATE_FUNCTION(name) void name(GameMemory * gameMemory, GameInput * input, RendererPushBuffer * renderCommands, RendererPushBuffer * uiRenderCMDs)
typedef GAME_UPDATE_FUNCTION(GameUpdateFunction);

#endif // CORE_H
