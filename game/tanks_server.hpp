#ifndef TANKS_SERVER_H
#define TANKS_SERVER_H

#include "../core.h"
#include "tanks.hpp"
#include "tanks_math.hpp"
#include <limits.h>

#define TICK_RATE 60
#define TICK_DURATION (1.0 / TICK_RATE)

typedef struct {
	bool active;
	u32 connectionID;
	u16 playerID;
	u16 health;
	vec2 position;
	f32 turretRot;
	f32 rotation;
	f32 lastFireTime;
	TankStyle style;
	Collider2D collider;
	u8 input;
	char displayName[32];
} Tank;

typedef struct {
	bool serverActive;
    double time;
	Tank tanks[8];
	u32 playerCount;
	Arena tempArena;
	PlatformAPI platform;
	double last_tick;
} ServerState;

const vec2 RR_SPAWN_POSITIONS[4] = {{-0.5f, 0.5f}, {0.5f, 0.5f}, {-0.5f, -0.5f}, {0.5f, -0.5f}};

void ServerStart(ServerState * state, u16 port, u16 maxPlayers);
void ServerStop(ServerState * state);
void ServerUpdate(ServerState * state, GameInput * input);

#endif // TANKS_SERVER_H
