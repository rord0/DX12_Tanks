#ifndef TANKS_SERVER_H
#define TANKS_SERVER_H

#include "../core.h"
#include "tanks.hpp"
#include "tanks_math.hpp"
#include <limits.h>

typedef struct {
	bool active;
	u16 playerID;
	u16 health;
	vec2 position;
	f32 turretRot;
	f32 rotation;
	f32 lastFireTime;
	TankStyle style;
	Collider2D collider;
	u8 input;
} Tank;

typedef struct {
    double time;
	Tank tanks[8];
} ServerState;

void ServerStart(ServerState * state);
void ServerUpdate(ServerState * state, GameInput * input);

#endif // TANKS_SERVER_H
