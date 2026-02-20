#ifndef TANKS_CLIENT_H
#define TANKS_CLIENT_H

#include "../core.h"
#include "tanks_math.hpp"
#include "tanks_server.hpp"
#include "tanks.hpp"

#include <cassert>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdio>

typedef struct {
    u32 x;
    u32 y;
    u32 width;
    u32 height;
} AtlasEntry;

typedef struct {
	u32 sheetWidth;
	u32 sheetHeight;
	u32 numRows;
	u32 numCols;
	u32 numFrames;
    u32 textureHandle;
} SpriteSheet;

typedef struct {
	b32 active;
	vec2 position;
	vec2 direction;
	vec2 scale;
	float timeAlive;
} Particle;

typedef struct {
	f32 startLifetime;
	u32 maxParticles;
	Particle * particles;
} ParticleEmitter;

typedef struct
{
	u16 playerID;
	u16 health;
	vec2 position;
	b32 active;
	f32 rotation;
	f32 turretRot;
	f32 turretOffset;
	f32 healthLerp;
	TankStyle style;
} TankGFX;

typedef struct
{
    void * memory;
    size_t size;
    size_t index;
} Arena; 

typedef struct {
    double time;
    vec3 cameraPos;
	Arena permArena;
    u32 extraTextureHandle;
    u32 tankAtlasHandle;
	AtlasEntry * tankAtlasEntries;
	SpriteSheet fireEffectSheet;
	SpriteSheet explosionVFXSheet;
	ParticleEmitter turretFireEmitter;
	ParticleEmitter explosionEmitter;
	TankGFX tanks[8]; 
	bool connected;
	bool helloSent;
} GameState;

void ClientStart(GameState * state, GameMemory * gameMemory);
void ClientUpdate(GameState * state, GameMemory * gameMemory, GameInput * input, RendererPushBuffer * renderCommands);

#endif // TANKS_CLIENT_H
