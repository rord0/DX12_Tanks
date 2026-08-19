#include "../core.h"
#include "../render_entry.h"

#include "tanks.hpp"
#include "imgui.h"
#include "tanks_client.hpp"
#include "tanks_server.hpp"
#include "util.hpp"
#include <algorithm>
#include <cstddef>

RendererPushBuffer * DEBUG_RENDER_CMDS;

// Unity Build

// DearImGui
#include "imgui.cpp"
#include "imgui_demo.cpp"
#include "imgui_draw.cpp"
#include "imgui_tables.cpp"
#include "imgui_widgets.cpp"

#include "render_commands.cpp"
#include "tanks_math.cpp"
#include "util.cpp"
#include "transforms.cpp"
#include "collision.cpp"
#include "tanks_client.cpp"
#include "tanks_server.cpp"
#include "prefabs.cpp"

#define ARRAY_IMPLEMENTATION
#include "../array.h"
#define ARENA_IMPLEMENTATION
#include "arena.hpp"
#define RING_QUEUE_IMPLEMENTATION
#include "ring_queue.hpp"
#define HASHMAP_IMPLEMENTATION
#include "hashmap.hpp"

#define EXPORT extern "C" __declspec(dllexport)

EXPORT GAME_START_FUNCTION(start)
{
	Arena permanantMemoryArena = ArenaInit((u8*)gameMemory->permStorage, 	  gameMemory->permStorageSize);
	Arena temporaryMemoryArena = ArenaInit((u8*)gameMemory->transientStorage, gameMemory->transStorageSize);

	// Distrute permanant storage evenly between server and client.
    GameState * clientState   =   (GameState*)ArenaPush(&permanantMemoryArena, sizeof(GameState));
	ServerState * serverState = (ServerState*)ArenaPush(&permanantMemoryArena, sizeof(ServerState));

	size_t maxPermArenaSize = ArenaGetRemainingSize(&permanantMemoryArena) / 2;
	clientState->permArena = ArenaInit(ArenaPush(&permanantMemoryArena, maxPermArenaSize), maxPermArenaSize);
	serverState->permArena = ArenaInit(ArenaPush(&permanantMemoryArena, maxPermArenaSize), maxPermArenaSize);

	// Distribute temporary storage evenly between server and client.
	size_t maxTempArenaSize = ArenaGetRemainingSize(&temporaryMemoryArena) / 2;
	clientState->frameArena = ArenaInit(ArenaPush(&temporaryMemoryArena, maxTempArenaSize), maxTempArenaSize);
	serverState->tempArena  = ArenaInit(ArenaPush(&temporaryMemoryArena, maxTempArenaSize), maxTempArenaSize);

	copy_c_str(clientState->displayName, "Player", 32);
	const char * ip = "172.0.0.1";

	serverState->platform = gameMemory->platform;

	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--host") == 0 && serverState->serverActive == false)
		{
			ServerStart(serverState, 7777, MAX_PLAYERS);
		}
		else if (strncmp(argv[i], "--name=", 7) == 0)
		{
			copy_c_str(clientState->displayName, &argv[i][7], 32);
		}

		if ((strncmp(argv[i], "--ipv6=", 7) == 0) && argv[i][7] != '\0')
		{
		    ip = &argv[i][7];
		}
	}

	ClientStart(clientState, gameMemory);
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

EXPORT INIT_DEAR_IMGUI_FUNCTION(init_dear_imgui)
{
    ImGui::SetAllocatorFunctions(allocFunc, freeFunc, allocUserData);
    ImGui::SetCurrentContext(ctx);
}
