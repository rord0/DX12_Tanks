#include "../core.h"
#include "../render_entry.h"

#include "tanks.hpp"
#include "tanks_client.hpp"
#include "tanks_server.hpp"
#include "util.hpp"

RendererPushBuffer * DEBUG_RENDER_CMDS;

// Unity Build
#include "render_commands.cpp"
#include "tanks_math.cpp"
#include "util.cpp"
#include "tanks_client.cpp"
#include "tanks_server.cpp"
#define ARENA_IMPLEMENTATION
#include "arena.hpp"

#define EXPORT extern "C" __declspec(dllexport)

EXPORT GAME_START_FUNCTION(start)
{
	ServerState * serverState = (ServerState*)((u8*)gameMemory->permStorage + sizeof(GameState));
    GameState * state = (GameState*)gameMemory->permStorage;
	copy_c_str(state->displayName, "Player", 32);
	const char * ip = "::1";

	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--host") == 0 && serverState->serverActive == false)
		{
			serverState->tempArena = ArenaInit((u8*)gameMemory->transientStorage, MB(1));
			serverState->platform = gameMemory->platform;
			ServerStart(serverState, 7777, 8);
		}
		else if (strncmp(argv[i], "--name=", 7) == 0)
		{
			copy_c_str(state->displayName, &argv[i][7], 32);
		}

		if ((strncmp(argv[i], "--ipv6=", 7) == 0) && argv[i][7] != '\0')
		{
		    ip = &argv[i][7];
		}
	}

	state->frameArena = ArenaInit((u8*)gameMemory->transientStorage + MB(1), MB(1));
	ClientStart(state, gameMemory);

	gameMemory->platform.platformStartClient(ip, 7777);
}

EXPORT GAME_UPDATE_FUNCTION(update)
{
	DEBUG_RENDER_CMDS = renderCommands;
    GameState * state = (GameState*)gameMemory->permStorage;
	ServerState * serverState = (ServerState*)((u8*)gameMemory->permStorage + sizeof(GameState));

	ServerUpdate(serverState, input);
	// DEBUG_SyncTanks(serverState->tanks, state->tanks);
	ClientUpdate(state, gameMemory, input, renderCommands, uiRenderCMDs);
}
