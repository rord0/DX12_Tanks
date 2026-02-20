#ifndef TANKS_H 
#define TANKS_H

#include "../core.h"
#include "serialize.hpp"

#define MAX_PLAYERS 8
#define TANK_MAX_HEALTH 100
#define TANK_ROTATION_SPEED 1.8f
#define TANK_MOVEMENT_SPEED 0.5f
#define TANK_FIRE_RATE 0.8f

typedef struct {
    u8 trackType;
    u8 bodyType;
    u8 turretType;
    u8 colorID;
} TankStyle;

typedef enum {
	PACKET_TYPE_HELLO,
	PACKET_TYPE_WELCOME,
	PACKET_TYPE_INPUT,
	PACKET_TYPE_UPDATE,
	PACKET_TYPE_FIRED
} PacketType;

typedef struct ClientHelloPacket {
	TankStyle style;
	char displayName[32];

	template<typename Stream>
	bool serialize(Stream & stream)
	{
		SerializeU8(stream, style.bodyType);
		SerializeU8(stream, style.trackType);
		SerializeU8(stream, style.turretType);
		SerializeU8(stream, style.colorID);
		SerializeCStr(stream, displayName, 32);
		return true;
	}
} HelloPacket;

#endif // TANKS_H
