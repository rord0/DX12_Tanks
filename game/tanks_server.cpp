#include "collision.hpp"
#include "serialize.hpp"
#include "tanks.hpp"
#include <cassert>
#include <cstddef>
#include <cstring>
#include <fstream>
#include "tanks_server.hpp"
#include "tanks_client.hpp"
#include "tanks_math.hpp"
#include "transforms.hpp"
#include "util.hpp"
#include "prefabs.hpp"

PlayerConnectData GetPlayerConnectData(ServerState * state, u32 playerID)
{
	PlayerConnectData data = {0};

	data.playerID = state->tanks[playerID].playerID;
	data.style = state->tanks[playerID].style;
	copy_c_str(data.displayName, state->tanks[playerID].displayName, sizeof(data.displayName));

	return data;
}

Tank * ServerGetPlayer(ServerState * state, u32 connectionID)
{
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (state->tanks[i].connectionID == connectionID)
		{
			return &state->tanks[i];
		}
	}
	return NULL;
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

void ServerSendPlayerFiredMessage(ServerState * state, u16 playerID, vec2 hitPos)
{
	ScratchArena temp(&state->tempArena);

	Tank * player = &state->tanks[playerID];
	PlayerFiredPacket packet = {0};
	packet.playerID = playerID;
	packet.hitPosition = hitPos;

	WriteStream stream = {(u8*)ArenaPush(temp.arena, 1024), 1024};
	if (!packet.serialize(stream)) { return; }

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (state->tanks[i].active)
		{
			state->platform.platformServerSend(stream.buffer, stream.pos, 1, state->tanks[i].connectionID);
		}
	}
}

void ServerSendRoundOverMessage(ServerState * state, bool roundOver, u16 winningPlayerID)
{
	ScratchArena temp(&state->tempArena);

	RoundOverPacket packet = {0};
	packet.roundOver = roundOver;
	packet.winningPlayerID = winningPlayerID;

	WriteStream stream = {(u8*)ArenaPush(temp.arena, 1024), 1024};
	if (!packet.serialize(stream)) { return; }

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (state->tanks[i].active)
		{
			state->platform.platformServerSend(stream.buffer, stream.pos, 1, state->tanks[i].connectionID);
		}
	}

}

void ServerSendUpdateMessage(ServerState * state)
{
	ScratchArena temp(&state->tempArena);

	UpdatePacket packet;
	packet.count = state->playerCount;
	packet.roundTimer = (f32)state->roundTimer;
	packet.playerData = (PlayerUpdateData*)ArenaPush(temp.arena, sizeof(PlayerUpdateData) * state->playerCount);

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		Tank * player = &state->tanks[i];
		Transform2D * transform = state->transforms->GetTransform(player->transformIndex);
		if (player->active)
		{
			PlayerUpdateData * playerData = packet.playerData + i;
			playerData->playerID  = player->playerID;
			playerData->health    = player->health;
			playerData->pos       = transform->position;
			playerData->rotation  = transform->rotation;
			playerData->turretRot = player->turretRot;
			playerData->kills	  = player->kills;
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

void ServerBroadcastDisconnectMessage(ServerState * state, u32 playerID)
{
	ScratchArena temp(&state->tempArena);

	DisconnectPacket packet = {0};
	packet.playerID = playerID;

	WriteStream stream = {(u8*)ArenaPush(temp.arena, KB(1)), KB(1)};
	if (!packet.serialize(stream)) { return; }

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (state->tanks[i].active)
		{
			state->platform.platformServerSend(stream.buffer, stream.pos, 1, state->tanks[i].connectionID);
		}
	}
}

vec2 FindSpawnPosition(ServerState * state)
{
	vec2 safestSpawn = RR_SPAWN_POSITIONS[0];
	f32 nearestPlayerDist = -1.0f;
	for (int i = 0; i < sizeof(RR_SPAWN_POSITIONS)/sizeof(vec2); i++)
	{
		vec2 currentSpawnPos = RR_SPAWN_POSITIONS[i];
		f32 currentNearestPlayerDist = std::numeric_limits<f32>::max();
		for (int p = 0; p < MAX_PLAYERS; p++)
		{
			Tank * player = &state->tanks[p];
			if (player->active)
			{
				Transform2D * transform = state->transforms->GetTransform(player->transformIndex);
				f32 dist = vec2Dist(transform->position, currentSpawnPos);
				if (dist < currentNearestPlayerDist) { currentNearestPlayerDist = dist; }
			}
		}

		if (currentNearestPlayerDist > nearestPlayerDist)
		{
			safestSpawn = currentSpawnPos;
			nearestPlayerDist = currentNearestPlayerDist;
		}
	}

	return safestSpawn;
}


void ServerStart(ServerState * state, u16 port, u16 maxPlayers)
{
	state->platform.platformStartServer(7777, maxPlayers);
	state->serverActive = true;
	state->transforms = (TransformHierarchy*)ArenaPush(&state->permArena, sizeof(TransformHierarchy));
	state->instances = ArrayInit(sizeof(PrefabInstance), 256, ArenaPush(&state->permArena, sizeof(PrefabInstance) * 256));
	state->collision = InitCollisionSystem2D(&state->permArena, 256, 16);
	state->roundTimer = ROUND_TIME;
	state->roundOver = false;


	Array prefabData = ParsePrefabInstancesCSV("prefab_instances.csv", &state->tempArena);
	for (int i = 0; i < prefabData.count; i++)
	{
		PrefabInstanceData data = *((PrefabInstanceData*)prefabData.elements + i);
		PrefabInstance instance = CreatePrefabInstance(data, &state->collision, state->transforms);
		ArrayPush(&state->instances, &instance);
	}
}

void ServerStop(ServerState * state)
{
	if (!state->serverActive) { return; }
	
	state->serverActive = false;
	state->playerCount = 0;
	memset(state->tanks,0, sizeof(state->tanks));
	state->platform.stopServer();
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
	player->playerID = playerIndex;
	player->connectionID = connID;
	player->style = packet->style;
	player->health = TANK_MAX_HEALTH;
	memcpy(player->displayName, packet->displayName, 32);
	player->displayName[31] = '\0';

	player->transformIndex = state->transforms->AddTransform({FindSpawnPosition(state), {1.0f, 1.0f}, 0.0f});
	u32 colliderTransform = state->transforms->AddTransform({{0.0f, 0.0f}, {0.75f/2, 0.65f/2}, 0.0f}, player->transformIndex);
	player->colliderID = state->collision.AddCollider(COLLIDER_RECTANGLE, colliderTransform, false);


	player->active = true;
	state->playerCount++;
	ServerSendWelcomeMessage(state, playerIndex);
	ServerBroadcastConnectMessage(state, playerIndex);
}

void ServerProcessInputPacket(ServerState * state, InputPacket * packet, u32 connID)
{
	Tank * player = ServerGetPlayer(state, connID);
	if (!player) { return; }

	player->input = packet->input;
	player->turretRot = packet->turretRot;
}

void ServerHandleDisconnect(ServerState * state, u32 connID)
{
	Tank * player = ServerGetPlayer(state, connID);
	if (!player) { return; }

	player->active = false;
	state->playerCount--;
	state->collision.RemoveCollider(player->colliderID);
	ServerBroadcastDisconnectMessage(state, player->playerID);
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
		case PACKET_TYPE_INPUT:
		{
			InputPacket inputPkt;
			if (inputPkt.serialize(stream))
			{
				ServerProcessInputPacket(state, &inputPkt, packet->id);
			}
		}
		default: break;
	}
}

void TankDamage(Tank * tank, u16 amount)
{
	i32 health = (i32)tank->health;
	health -= amount;
	if (health < 0)
	{
		health = 0;
	}
	tank->health = (u16)health;
}

void TankShoot(Tank * tank, ServerState * state)
{
	Transform2D * transform = state->transforms->GetTransform(tank->transformIndex);

	vec2 turretDir = vec2{cosf(tank->turretRot), sinf(tank->turretRot)};
	vec2 tankDir   = vec2{cosf(transform->rotation),  sinf(transform->rotation)};
	vec2 turretCenter = transform->position + (tankDir * -0.03f);
	vec2 turretPos = turretCenter - (turretDir * 0.075f); // Offset backwards
											//
	vec2 lineStart = turretCenter;
	vec2 lineEnd =  turretCenter + (-turretDir * TURRET_RANGE);

	vec2 hitPos = lineEnd;
	u32 hitColliderID = 0;
	if (state->collision.RaycastHit(lineStart, lineEnd, &hitPos, tank->colliderID, &hitColliderID))
	{
		for (int i = 0; i < MAX_PLAYERS; i++)
		{
			Tank * otherTank = &state->tanks[i];
			if (otherTank->colliderID == hitColliderID)
			{
				TankDamage(otherTank, 10);
				if (otherTank->health == 0)
				{
					tank->kills++;
				}
			}
		}
	}

	ServerSendPlayerFiredMessage(state, tank->playerID, hitPos);
	tank->lastFireTime = state->time;
}

void UpdateTank(Tank * tank, ServerState * state)
{
	Transform2D * transform = state->transforms->GetTransform(tank->transformIndex);
	if (tank->health == 0)
	{
		transform->SetPosition(FindSpawnPosition(state));
		tank->health = TANK_MAX_HEALTH;
	}

	vec2i inputAxis = {0, 0};
	bool shootPressed = ((tank->input >> 4) & 1);
	if ((tank->input >> 0) & 1) { inputAxis.y += 1; } // UP
	if ((tank->input >> 1) & 1) { inputAxis.x -= 1; } // LEFT
	if ((tank->input >> 2) & 1) { inputAxis.y -= 1; } // DOWN
	if ((tank->input >> 3) & 1) { inputAxis.x += 1; } // RIGHT
	
	if (state->roundOver) { return; }

	transform->SetRotation(transform->rotation - (inputAxis.x * TANK_ROTATION_SPEED * TICK_DURATION));
	vec2 tankForward = vec2Rotate({-1.0f, 0.0f}, transform->rotation);
	transform->SetPosition(transform->position + (tankForward * TANK_MOVEMENT_SPEED * inputAxis.y * TICK_DURATION));

	if (transform->position.x > WORLD_EXTENTS.x) { transform->SetPosition({WORLD_EXTENTS.x, transform->position.y}); }
	if (transform->position.x < WORLD_EXTENTS.y) { transform->SetPosition({WORLD_EXTENTS.y, transform->position.y}); }
	if (transform->position.y > WORLD_EXTENTS.z) { transform->SetPosition({transform->position.x, WORLD_EXTENTS.z}); }
	if (transform->position.y < WORLD_EXTENTS.w) { transform->SetPosition({transform->position.x, WORLD_EXTENTS.w}); }

	if (shootPressed && state->time > tank->lastFireTime + TANK_FIRE_RATE)
	{
		TankShoot(tank, state);
	}
}

u16 FindWinningPlayer(ServerState * state)
{
	u16 maxScore = 0;
	u16 playerID = 0;
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		Tank * tank = &state->tanks[i];
		if (!tank->active) continue;
		if (tank->kills > maxScore)
		{
			maxScore = tank->kills;
			playerID = tank->playerID;
		}
	}

	return playerID;
}

void ResetPlayers(ServerState * state)
{
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		Tank * tank = &state->tanks[i];
		if (!tank->active) continue;
		tank->kills = 0;
		tank->health = TANK_MAX_HEALTH;
		state->transforms->GetTransform(tank->transformIndex)->SetPosition(FindSpawnPosition(state));
	}
}

void ServerTick(ServerState * state, GameInput * input)
{
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

	if (state->playerCount > 0)
	{
		state->roundTimer -= TICK_DURATION;
		if (state->roundTimer <= 0)
		{
			if (!state->roundOver)
			{
				state->roundOver = true;
				state->roundTimer = 15.0f;
				ServerSendRoundOverMessage(state, true, FindWinningPlayer(state));
			}
			else
			{
				ResetPlayers(state);
				state->roundTimer = ROUND_TIME; 
				state->roundOver = false;
				ServerSendRoundOverMessage(state, false, 0);
			}
		}
	}

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		Tank * tank = &state->tanks[i];
		if (!tank->active) continue;
		UpdateTank(tank, state);
	}

	state->transforms->UpdateTransforms();
	state->collision.ResolveCollisions(state->transforms);

	ServerSendUpdateMessage(state);
}

void DEBUG_DrawCollider(RendererPushBuffer * renderCMDs, Collider2D * collider, TransformHierarchy * transforms)
{
	Transform2D * t = transforms->GetTransform(collider->transformIndex);
	vec3 color = collider->colliding ? vec3{1,0,0} : vec3{0,1,0};
	if (collider->type == COLLIDER_RECTANGLE)
	{
		vec2 verts[4];
		GetRectWorldVertices(t, verts);

		for (int i = 0; i < 4; i++)
		{
			RendererPushLine(renderCMDs, verts[i], verts[(i + 1) % 4], {color.x, color.y, color.z, 1.0f}, 0.004f, 6);
			RendererPushCircle(renderCMDs, verts[i], 0.0, {0.05f, 0.05f}, color, 1.0f, 5);
		}
	}
	else if (collider->type == COLLIDER_CIRCLE)
	{
		RendererPushCircle(renderCMDs, t->position, t->rotation, t->scale, {0,1,0}, 0.1f, 5);
	}
}

void DEBUG_DrawColliders(RendererPushBuffer * renderCMDs, CollisionSystem2D * collision, TransformHierarchy * transforms)
{
	for (int i = 0; i < collision->dynamicColliders.count; i++)
	{
		Collider2D * collider = &((Collider2D*)collision->dynamicColliders.elements)[i];
		DEBUG_DrawCollider(renderCMDs, collider, transforms);
	}
	for (int i = 0; i < collision->staticColliders.count; i++)
	{
		Collider2D * collider = &((Collider2D*)collision->staticColliders.elements)[i];
		DEBUG_DrawCollider(renderCMDs, collider, transforms);
	}
}

void ServerUpdate(ServerState * state, GameInput * input)
{
	if (!state->serverActive) { return; }

	state->time += input->deltaTime;

	if (DEBUG_RENDER_CMDS != NULL)
	{
		DEBUG_DrawColliders(DEBUG_RENDER_CMDS, &state->collision, state->transforms);
		for (int i = 0; i < _countof(RR_SPAWN_POSITIONS); i++)
		{

			RendererPushCircle(DEBUG_RENDER_CMDS, RR_SPAWN_POSITIONS[i], 0.0, {0.1f, 0.1f}, {1,0.2,0}, 1.0f, 1);
		}
	}

	double elapsed = state->time - state->last_tick;
	while (elapsed >= TICK_DURATION)
	{
		ServerTick(state, input);
		state->last_tick += TICK_DURATION;
		elapsed -= TICK_DURATION;
	}
}
