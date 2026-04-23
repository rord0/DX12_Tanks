#include "serialize.hpp"
#include "tanks.hpp"
#include <cassert>
#include <cstring>
#include "tanks_server.hpp"
#include "util.hpp"

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

PlayerConnectData GetPlayerConnectData(ServerState * state, u32 playerID)
{
	PlayerConnectData data = {0};

	data.playerID = state->tanks[playerID].playerID;
	data.style = state->tanks[playerID].style;
	copy_c_str(data.displayName, state->tanks[playerID].displayName, sizeof(data.displayName));

	return data;
}

void ServerSendWelcomeMessage(ServerState * state, u32 playerIndex)
{
	ScratchArena temp(&state->tempArena);

	Tank * player = &state->tanks[playerIndex];
	assert(player->active);

	WelcomePacket packet = {0};
	packet.playerID = player->playerID;
	packet.playerCount = 0;
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (state->tanks[i].active)
		{
			packet.playerData[packet.playerCount] = GetPlayerConnectData(state, i);
			packet.playerCount++;
		}
	}

	// TODO: log error msg
	WriteStream stream = {(u8*)ArenaPush(temp.arena, 1024), 1024};
	if (!packet.serialize(stream)) { return; }

	state->platform.platformServerSend(stream.buffer, stream.pos, 1, player->connectionID);
}

void ServerSendUpdateMessage(ServerState * state)
{
	ScratchArena temp(&state->tempArena);

	UpdatePacket packet;
	packet.count = state->playerCount;
	packet.playerData = (PlayerUpdateData*)ArenaPush(temp.arena, sizeof(PlayerUpdateData) * state->playerCount);

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		Tank * player = &state->tanks[i];
		if (player->active)
		{
			PlayerUpdateData * playerData = packet.playerData + i;
			playerData->playerID  = player->playerID;
			playerData->health    = player->health;
			playerData->pos       = player->position;
			playerData->rotation  = player->rotation;
			playerData->turretRot = player->turretRot;
			playerData->wasTeleport = 0;
		}
	}

	WriteStream stream = {(u8*)ArenaPush(temp.arena, KB(1)), KB(1)};
	if (!packet.serialize(stream)) { return; }

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (state->tanks[i].active)
		{
			state->platform.platformServerSend(stream.buffer, stream.pos, 0, state->tanks[i].connectionID);
		}
	}
}

void ServerBroadcastConnectMessage(ServerState * state, u32 playerIndex)
{
	ScratchArena temp(&state->tempArena);

	ConnectPacket packet = {0};
	Tank * player = &state->tanks[playerIndex];
	packet.playerData = GetPlayerConnectData(state, playerIndex);

	WriteStream stream = {(u8*)ArenaPush(temp.arena, KB(1)), KB(1)};
	if (!packet.serialize(stream)) { return; }

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (state->tanks[i].active && i != playerIndex) 
		{
			state->platform.platformServerSend(stream.buffer, stream.pos, 1, state->tanks[i].connectionID);
		}
	}
}

void ServerStart(ServerState * state, u16 port, u16 maxPlayers)
{
	state->platform.platformStartServer(7777, maxPlayers);
	state->serverActive = true;
}

void ServerProcessHelloPacket(ServerState * state, HelloPacket * packet, u32 connID)
{
	// Find empty player index
	int playerIndex = -1; 
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (state->tanks[i].active == false)
		{
			playerIndex = i;
			break;
		}
	}
	// Max number of players reached
	// TODO: warn message: this shouldn't happen...
	if (playerIndex == -1) { return; }

	Tank * player = &state->tanks[playerIndex];
	
	memset(&state->tanks[playerIndex], 0, sizeof(Tank));
	player->active = true;
	player->playerID = playerIndex;
	player->connectionID = connID;
	player->style = packet->style;
	player->health = TANK_MAX_HEALTH;
	player->collider.type = COLLIDER_RECTANGLE;
	player->collider.size = {0.75f, 0.65f};
	memcpy(player->displayName, packet->displayName, 32);
	player->displayName[31] = '\0';
	player->position = RR_SPAWN_POSITIONS[state->playerCount % 4];

	state->playerCount++;
	ServerSendWelcomeMessage(state, playerIndex);
	ServerBroadcastConnectMessage(state, playerIndex);
}

void ServerHandleDisconnect(ServerState * state, u32 connID)
{
	return;
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

void UpdateTank(Tank * tank, ServerState * state)
{
	vec2i inputAxis = {0, 0};
	bool shootPressed = ((tank->input >> 4) & 1);
	if ((tank->input >> 0) & 1) { inputAxis.y += 1; } // UP
	if ((tank->input >> 1) & 1) { inputAxis.x -= 1; } // LEFT
	if ((tank->input >> 2) & 1) { inputAxis.y -= 1; } // DOWN
	if ((tank->input >> 3) & 1) { inputAxis.x += 1; } // RIGHT
	
	tank->rotation -= inputAxis.x * TANK_ROTATION_SPEED * TICK_DURATION;
	tank->turretRot = tank->rotation;
	vec2 tankForward = vec2Rotate({-1.0f, 0.0f}, tank->rotation);
	tank->position += tankForward * TANK_MOVEMENT_SPEED * inputAxis.y * TICK_DURATION;

	tank->collider.position = tank->position;
	tank->collider.rotation = tank->rotation;

	if (shootPressed && state->time > tank->lastFireTime + TANK_FIRE_RATE)
	{
		TankShoot(tank, state);
	}
}

void ServerTick(ServerState * state, GameInput * input)
{
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

	//state->tanks[0].input = inputA;
	//state->tanks[3].input = inputB;

	NetworkEvent * netEvent;
	while (state->platform.serverGetEvent(&netEvent))
	{
		switch (netEvent->type) {
			case NET_EVENT_CLIENT_CONNECTED:
				break;
			case NET_EVENT_CLIENT_DISCONNECTED:
				ServerHandleDisconnect(state, netEvent->connID);
				break;
			case NET_EVENT_PACKET:
				ServerHandlePacket(state, netEvent->packet);
				break;
		}
	}

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		Tank * tank = &state->tanks[i];
		if (!tank->active) continue;
		UpdateTank(tank, state);

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
			//DEBUG_DrawCollider(DEBUG_RENDER_CMDS, &tank->collider, colliding);
			//DEBUG_DrawCollider(DEBUG_RENDER_CMDS, &otherTank->collider, colliding);
		}
	}

	ServerSendUpdateMessage(state);
}

void ServerUpdate(ServerState * state, GameInput * input)
{
	if (!state->serverActive) { return; }

	state->time += input->deltaTime;

	if (DEBUG_RENDER_CMDS != NULL)
	{
		DebugGeoInstanceData rect = {vec3{0.0f, 0.0f, 0.0f}, {7.15f, 4.0f}, {1.0f,0.0f,0.0f}, 0.0f, 0.02f};
		RendererPushRectangle(DEBUG_RENDER_CMDS, rect, 33);
	}

	double elapsed = state->time - state->last_tick;
	while (elapsed >= TICK_DURATION)
	{
		ServerTick(state, input);
		state->last_tick += TICK_DURATION;
		elapsed -= TICK_DURATION;
	}
}
