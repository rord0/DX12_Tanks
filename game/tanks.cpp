#include "../core.h"
#include "../render_entry.h"

#include "tanks.hpp"
#include "tanks_client.hpp"
#include "tanks_server.hpp"

RendererPushBuffer * DEBUG_RENDER_CMDS;

// Unity Build
#include "render_commands.cpp"
#include "tanks_math.cpp"
#include "tanks_client.cpp"
#include "tanks_server.cpp"

#define EXPORT extern "C" __declspec(dllexport)

EXPORT GAME_START_FUNCTION(start)
{
	ServerState * serverState = (ServerState*)((u8*)gameMemory->permStorage + sizeof(GameState));
	ServerStart(serverState);

    GameState * state = (GameState*)gameMemory->permStorage;
	ClientStart(state, gameMemory);

	gameMemory->platformStartServer(7777, 8);
	gameMemory->platformStartClient("::1", 7777);
}

EXPORT GAME_UPDATE_FUNCTION(update)
{
	DEBUG_RENDER_CMDS = renderCommands;
    GameState * state = (GameState*)gameMemory->permStorage;
	ServerState * serverState = (ServerState*)((u8*)gameMemory->permStorage + sizeof(GameState));

	ServerUpdate(serverState, input);

	DEBUG_SyncTanks(serverState->tanks, state->tanks);

	ClientUpdate(state, gameMemory, input, renderCommands);
}
