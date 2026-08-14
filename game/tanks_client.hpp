#ifndef TANKS_CLIENT_H
#define TANKS_CLIENT_H

#include "../core.h"
#include "../array.h"
#include "tanks_math.hpp"
#include "tanks_server.hpp"
#include "tanks.hpp"
#include "ui.hpp"
#include "util.hpp"
#include "hashmap.hpp"

#include <cassert>
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
	char displayName[32];
	u32 kills;
	vec2 lastTrackSpawnPos;
} TankGFX;

typedef enum ClientState
{
	CLIENT_STATE_MAIN_MENU,
	CLIENT_STATE_CUSTOMIZE_MENU,
	CLIENT_STATE_JOIN_MENU,
	CLIENT_STATE_CONNECTING,
	CLIENT_STATE_CONNECTED
} ClientState;

typedef enum EditorState {
	EDITOR_STATE_IDLE,
	EDITOR_STATE_DRAGGING,
	EDITOR_STATE_SELECTING
} EditorState;

typedef struct DEBUG_EditorState
{
	EditorState state;
	vec2 selectionStart;
	vec2 mouseWorldLast;
} DEBUG_EditorState;

typedef struct {
    double time;
    double timeSinceLastUpdate;
    vec2 cameraPos;
	Arena permArena;
	Arena frameArena;
    u32 extraTextureHandle;
    u32 shellImpactTextureHandle;
    u32 tankAtlasHandle;
	u32 customizeIconTextureHandle;
	u32 hamburgerIconTextureHandle;
	u32 targetIconTextureHandle;
	u32 desertBackgroundTextureHandle;
	HashMap textureHandles;
    i32 interFontHandle;
	AtlasEntry * tankAtlasEntries;
	SpriteSheet fireEffectSheet;
	SpriteSheet explosionVFXSheet;
	ParticleEmitter turretFireEmitter;
	ParticleEmitter explosionEmitter;
	ParticleEmitter shellTrailEmitter;
	ParticleEmitter impactEmitter;
	ParticleEmitter tankTrackEmitter;
	TankGFX tanks[8]; 
	u16 playerID;
	TankStyle playerStyle;
	bool connected;
	bool helloSent;
	bool welcomeReceived;
	bool optionsMenuOpen;
	bool showBadIPPopup;
	bool showConnFailedPopup;
	vec2 cameraVelocity;
	u32 currentHillIndex;
	f32 cameraZoom;
	f32 roundTimer;
	bool hillMoving;
	bool roundOver;
	pcg32_random_t random;
	UILayout * ui;
	TransformHierarchy * props;
	Array instances;
	Array selectedInstances;
	CollisionSystem2D colliders;
	ClientState clientState;
	DEBUG_EditorState editorState;
	char serverIP[64];
	char displayName[32];
	char winningPlayerName[32];
} GameState;

void ClientStart(GameState * state, GameMemory * gameMemory);
void ClientUpdate(GameState * state, GameMemory * gameMemory, GameInput * input, RendererPushBuffer * renderCommands);

#endif // TANKS_CLIENT_H
