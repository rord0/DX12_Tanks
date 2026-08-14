#ifndef TANKS_SERVER_H
#define TANKS_SERVER_H

#include "../core.h"
#include "tanks.hpp"
#include "tanks_client.hpp"
#include "collision.hpp"
#include "tanks_math.hpp"
#include <limits.h>

#define TICK_RATE 60
#define TICK_DURATION (1.0 / TICK_RATE)
#define TURRET_RANGE 2.0f
#define ROUND_TIME 120.0f
#define HILL_TICK_RATE 0.8f

typedef struct {
	bool active;
	u32 connectionID;
	u16 playerID;
	u16 health;
	u16 kills;
	f32 turretRot;
	f32 lastFireTime;
	TankStyle style;
	u32 transformIndex;
	u32 colliderID;
	u8 input;
	char displayName[32];
} Tank;

typedef struct {
	bool serverActive;
    double time;
	Tank tanks[8];
	u32 playerCount;
	Arena tempArena;
	Arena permArena;
	PlatformAPI platform;
	double last_tick;
	double roundTimer;
	bool roundOver;
	TransformHierarchy * transforms;
	Array instances;
	CollisionSystem2D collision;
	u32 hillZoneIndex;
	f32 hillTimeRemaining;
	bool hillMovingSent;
	double nextHillTick;
} ServerState;

typedef struct HillData
{
	vec2 pos;
	f32 radius;
} HillData;

const vec2 RR_SPAWN_POSITIONS[4] = {{-1.0f, 1.0f}, {1.0f, 1.0f}, {-1.0f, -1.0f}, {1.0f, -1.0f}};
const vec4 WORLD_EXTENTS = {6.0f, -2.0f, 2.0f, -2.0f}; // RIGHT, LEFT, UP, DOWN
const HillData HILL_ZONES[] = {{{-0.9, 1.2}, 2.85f}, {{5.0f, 1.2}, 3.0f}};

void ServerStart(ServerState * state, u16 port, u16 maxPlayers);
void ServerStop(ServerState * state);
void ServerUpdate(ServerState * state, GameInput * input);

#endif // TANKS_SERVER_H
