#include "tanks.hpp"
#include "tanks_server.hpp"

void DEBUG_DrawCollider(RendererPushBuffer * renderCmds, Collider2D * collider, b32 colliding)
{
	vec3 color = colliding ? vec3{1.0f, 0.0f, 0.0f} : vec3{0.0f, 1.0f, 0.0f};
	if (collider->type == COLLIDER_RECTANGLE)
	{
		DebugGeoInstanceData rect = {vec3{collider->position.x, collider->position.y, 0.0f},
			{collider->size.x, collider->size.y}, color, collider->rotation, 0.05f};
		RendererPushRectangle(renderCmds, rect, 33);
	}
}

void ServerStart(ServerState * state)
{
	state->tanks[3].active = true;
	state->tanks[3].playerID = 67;
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		state->tanks[i].collider.type = COLLIDER_RECTANGLE;
		state->tanks[i].collider.size = {0.75f, 0.65f};
		state->tanks[i].health = TANK_MAX_HEALTH;
	}
}

void ServerProcessHelloPacket(ServerState * state, HelloPacket * packet, u32 connID)
{
	state->tanks[connID].active = true;
	state->tanks[connID].style = packet->style;

	// Send connected message to other clients.
}

void ServerHandlePacket(ServerState * state, NetworkPacket * packet)
{
	PacketType type = (PacketType)(((u8*)packet->data)[0]);
	ReadStream stream((u8*)packet->data, packet->size);

	switch (type)
	{
		case PACKET_TYPE_HELLO:
		{
			HelloPacket helloPkt;
			if (helloPkt.serialize(stream))
			{
				ServerProcessHelloPacket(state, &helloPkt, packet->id);
			}
		} break;
		default: break;
	}
}

void TankDamage(Tank * tank, u16 amount)
{
	if (tank->health > amount)
	{
		tank->health -= amount;
	}
	else
	{
		tank->health = TANK_MAX_HEALTH;
	}
}

void TankShoot(Tank * tank, ServerState * state)
{
	vec2 turretDir = vec2{cosf(tank->turretRot), sinf(tank->turretRot)};
	vec2 tankDir   = vec2{cosf(tank->rotation),  sinf(tank->rotation)};
	vec2 turretCenter = tank->position + (tankDir * -0.03f);
	vec2 turretPos = turretCenter - (turretDir * 0.075f); // Offset backwards
											//
	vec2 lineStart = turretCenter;
	vec2 lineEnd =  turretCenter + (-turretDir*1.5f);

	Tank * hitTank = NULL;
	vec2 hitPos = lineEnd;
	f32 closestDist = std::numeric_limits<float>::max();
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		Tank * otherTank = &state->tanks[i];
		if (otherTank->active && otherTank->playerID != tank->playerID)
		{
			vec2 collisionPoint;
			if (IsLineColliding(otherTank->collider, lineStart, lineEnd, &collisionPoint))
			{
				f32 dist = vec2Dist(collisionPoint, lineStart);
				if (dist < closestDist)
				{
					closestDist = dist;
					hitPos = collisionPoint;
					hitTank = otherTank;
				}
			}
		}
	}
	if (hitTank != NULL)
	{
		TankDamage(hitTank, 10);
	}

	// TankPlayFireEffects(tank->playerID, hitPos, state);
	tank->lastFireTime = state->time;
}

void UpdateTank(Tank * tank, double deltaTime, ServerState * state)
{
	vec2i inputAxis = {0, 0};
	bool shootPressed = ((tank->input >> 4) & 1);
	if ((tank->input >> 0) & 1) { inputAxis.y += 1; } // UP
	if ((tank->input >> 1) & 1) { inputAxis.x -= 1; } // LEFT
	if ((tank->input >> 2) & 1) { inputAxis.y -= 1; } // DOWN
	if ((tank->input >> 3) & 1) { inputAxis.x += 1; } // RIGHT
	
	tank->rotation -= inputAxis.x * TANK_ROTATION_SPEED * deltaTime;
	tank->turretRot = tank->rotation;
	vec2 tankForward = vec2Rotate({-1.0f, 0.0f}, tank->rotation);
	tank->position += tankForward * TANK_MOVEMENT_SPEED * inputAxis.y * deltaTime;

	tank->collider.position = tank->position;
	tank->collider.rotation = tank->rotation;

	if (shootPressed && state->time > tank->lastFireTime + TANK_FIRE_RATE)
	{
		TankShoot(tank, state);
	}
}

void ServerUpdate(ServerState * state, GameInput * input)
{
	state->time += input->deltaTime;

	u8 inputA = 0;
	if (input->WASD[0].isDown) {inputA |= 1 << 0; };
	if (input->WASD[1].isDown) {inputA |= 1 << 1; };
	if (input->WASD[2].isDown) {inputA |= 1 << 2; };
	if (input->WASD[3].isDown) {inputA |= 1 << 3; };
	if (input->isSpacePressed) {inputA |= 1 << 4; };
	u8 inputB = 0;
	if (input->ARROWS[0].isDown) {inputB |= 1 << 0; };
	if (input->ARROWS[1].isDown) {inputB |= 1 << 1; };
	if (input->ARROWS[2].isDown) {inputB |= 1 << 2; };
	if (input->ARROWS[3].isDown) {inputB |= 1 << 3; };
	if (input->isEnterPressed)	 {inputB |= 1 << 4; };

	state->tanks[0].input = inputA;
	state->tanks[3].input = inputB;

	for (int i = 0; i < input->serverEventCount; i++)
	{
		NetworkEvent * event = &input->serverEvents[i];
		switch (event->type) {
			case NET_EVENT_CLIENT_CONNECTED:
				state->tanks[event->connID].active = true;
				state->tanks[event->connID].playerID = event->connID;
				break;
			case NET_EVENT_CLIENT_DISCONNECTED:
				state->tanks[event->connID].active = false;
				break;
			case NET_EVENT_PACKET:
				ServerHandlePacket(state, event->packet);
			break;
		}
	}

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		Tank * tank = &state->tanks[i];
		if (!tank->active) continue;
		UpdateTank(tank, input->deltaTime, state);

		// Handle Tank -> Tank Collision
		for (int j = 0; j < MAX_PLAYERS; j++)
		{
			Tank * otherTank = &state->tanks[j];
			if (otherTank->active == false || tank->playerID == otherTank->playerID) continue;

			CollisionData cd = {0};
			bool colliding = IsColliding(tank->collider, otherTank->collider, &cd);
			if (colliding)
			{
				vec2 correction = cd.normal * (cd.penetration * 0.5f);
				tank->position += correction;
				otherTank->position -= correction;
			}
			DEBUG_DrawCollider(DEBUG_RENDER_CMDS, &tank->collider, colliding);
			DEBUG_DrawCollider(DEBUG_RENDER_CMDS, &otherTank->collider, colliding);
		}
	}
}
