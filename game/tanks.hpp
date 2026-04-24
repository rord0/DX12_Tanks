#ifndef TANKS_H 
#define TANKS_H

#include "../core.h"
#include "serialize.hpp"
#include "arena.hpp"
#include <cstddef>
#include <immintrin.h>

#define MAX_PLAYERS 8
#define TANK_MAX_HEALTH 100
#define TANK_ROTATION_SPEED 1.8f
#define TANK_MOVEMENT_SPEED 0.5f
#define TANK_FIRE_RATE 0.8f

#define SerializeTankStyle(stream, value)   if(!serializeTankStyle(stream, value))   { return false; }
#define SerializePlayerData(stream, value)  if(!serializePlayerData(stream, value))  { return false; }
#define SerializePlayerUpdateData(stream, value) if(!serializePlayerUpdateData(stream, value))  { return false; }

typedef struct {
    u8 trackType;
    u8 bodyType;
    u8 turretType;
    u8 colorID;
} TankStyle;

typedef struct PlayerConnectData_t {
	u16 playerID;
	TankStyle style;
	char displayName[32];
} PlayerConnectData;

typedef struct PlayerUpdateData_t {
	u16 playerID;
	u8 health;
	u8 wasTeleport;
	vec2 pos;
	f32 rotation;
	f32 turretRot;
} PlayerUpdateData;

typedef enum {
	PACKET_TYPE_HELLO = 10,
	PACKET_TYPE_WELCOME,
	PACKET_TYPE_CONNECT,
	PACKET_TYPE_DISCONNECT,
	PACKET_TYPE_INPUT,
	PACKET_TYPE_UPDATE,
	PACKET_TYPE_FIRED
} PacketType;

template<typename Stream>
bool serializeTankStyle(Stream & stream, TankStyle & style)
{
	SerializeU8(stream, style.bodyType);
	SerializeU8(stream, style.trackType);
	SerializeU8(stream, style.turretType);
	SerializeU8(stream, style.colorID);
	return true;
}

template<typename Stream>
bool serializePlayerData(Stream & stream, PlayerConnectData & data)
{
	SerializeU16(stream, data.playerID);
	SerializeTankStyle(stream, data.style);
	SerializeCStr(stream, data.displayName, 32);
	return true;
}

template<typename Stream>
bool serializePlayerUpdateData(Stream & stream, PlayerUpdateData & data)
{
	SerializeU16(stream, data.playerID);
	SerializeU8(stream, data.health);
	SerializeU8(stream, data.wasTeleport);
	SerializeV2(stream, data.pos);
	SerializeF32(stream, data.rotation);
	SerializeF32(stream, data.turretRot);
	return true;
}

typedef struct ClientHelloPacket {
	TankStyle style;
	char displayName[32];

	template<typename Stream>
	bool serialize(Stream & stream)
	{
		u8 type = PACKET_TYPE_HELLO;
		SerializeU8(stream, type);
		SerializeTankStyle(stream, style);
		SerializeCStr(stream, displayName, 32);
		return true;
	}
} HelloPacket;

typedef struct ClientInputPacket {
	u8 input;
	f32 turretRot;
	template<typename Stream>
	bool serialize(Stream & stream)
	{
		u8 type = PACKET_TYPE_INPUT;
		SerializeU8(stream, type);
		SerializeU8(stream, input);
		SerializeF32(stream, turretRot);
		return true;
	}
} InputPacket;

typedef struct ServerWelcomePacket {
	u16 playerID;
	u8 playerCount;
	PlayerConnectData playerData[MAX_PLAYERS];
	template<typename Stream>
	bool serialize(Stream & stream)
	{
		u8 type = PACKET_TYPE_WELCOME;
		SerializeU8(stream, type);
		SerializeU16(stream, playerID);
		SerializeU8(stream, playerCount);
		for (int i = 0; i < playerCount; i++)
		{
			SerializePlayerData(stream, playerData[i]);
		}
		return true;
	}
} WelcomePacket;

typedef struct ServerConnectPacket {
	PlayerConnectData playerData;
	template<typename Stream>
	bool serialize(Stream & stream)
	{
		u8 type = PACKET_TYPE_CONNECT;
		SerializeU8(stream, type);
		SerializePlayerData(stream, playerData)
		return true;
	}
} ConnectPacket;

typedef struct ServerDisconnectPacket {
	u16 playerID;
	template<typename Stream>
	bool serialize(Stream & stream)
	{
		u8 type = PACKET_TYPE_DISCONNECT;
		SerializeU8(stream, type);
		SerializeU16(stream, playerID);
		return true;
	}
} DisconnectPacket;

typedef struct PlayerFiredPacket {
	u16 playerID;
	vec2 hitPosition;
	template<typename Stream>
	bool serialize(Stream & stream)
	{
		u8 type = PACKET_TYPE_FIRED;
		SerializeU8(stream, type);
		SerializeU16(stream, playerID);
		SerializeV2(stream, hitPosition);
		return true;
	}
} PlayerFiredPacket;

typedef struct ServerUpdatePacket {
	u16 count;
	PlayerUpdateData * playerData;
	template<typename Stream>
	bool serialize(Stream & stream, Arena * arena = nullptr)
	{
		u8 type = PACKET_TYPE_UPDATE;
		SerializeU8(stream, type);
		SerializeU16(stream, count);

		if (Stream::IsReading)
		{
			if (arena == nullptr)    { return false; }
			if (count > MAX_PLAYERS) { return false; }
			playerData = (PlayerUpdateData*)ArenaPush(arena, count * sizeof(PlayerUpdateData));
			if (!playerData) { return false; }
		}

		for (int i = 0; i < count; i++)
		{
			SerializePlayerUpdateData(stream, playerData[i]);
		}

		return true;
	}
} UpdatePacket;

#endif // TANKS_H
