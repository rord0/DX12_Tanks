#include "portal.h"
#include "arena.h"
#include "core.h"

typedef struct {
	PortalServer server;
	PortalClient client;
	NetworkEvent * serverEvents;
	NetworkEvent * clientEvents;
	clientState lastClientState;
	Arena framePackets;
} NetworkState;

NetworkState NETWORK_STATE = {0};

PLATFORM_START_SERVER(PlatformStartServer)
{
	NETWORK_STATE.server = PortalCreateServer(SOCKET_TYPE_IPV6, port, maxConnections);
	if (NETWORK_STATE.server.started) { return true; }
	return false;
}

PLATFORM_START_CLIENT(PlatformStartClient)
{
	NETWORK_STATE.client = PortalCreateClient(SOCKET_TYPE_IPV6, 0);
	PortalClientConnect(&NETWORK_STATE.client, PortalStrToIPv6Address(addressStr, serverPort));
	return true;
}

PLATFORM_CLIENT_SEND(PlatformClientSend)
{
	PortalClientSend(&NETWORK_STATE.client, data, size, (PortalPacketSendMode)sendMode);
}

void InitializeNetworking()
{
	PortalInit(NULL);

	NETWORK_STATE.serverEvents = (NetworkEvent*)PlatformAlloc(sizeof(NetworkEvent) * 1024);
	NETWORK_STATE.clientEvents = (NetworkEvent*)PlatformAlloc(sizeof(NetworkEvent) * 1024);
	NETWORK_STATE.framePackets = ArenaAlloc(KB(16));
	NETWORK_STATE.lastClientState = CLIENT_DISCONNECTED;
}

void NetworkingUpdate(GameInput * input, double time)
{
	PortalServerUpdate(&NETWORK_STATE.server, time);
	PortalClientUpdate(&NETWORK_STATE.client, time);
	ArenaClear(&NETWORK_STATE.framePackets);

	u32 clientEventCount = 0;
	u32 serverEventCount = 0;
	if (NETWORK_STATE.lastClientState != CLIENT_CONNECTED && NETWORK_STATE.client.state == CLIENT_CONNECTED)
	{
		// add connected event to thingy.
		NetworkEvent event = {NET_EVENT_CLIENT_CONNECTED, 0, NULL};
		NETWORK_STATE.clientEvents[clientEventCount] = event;
		clientEventCount++;
	}
	NETWORK_STATE.lastClientState = NETWORK_STATE.client.state;

	if (NETWORK_STATE.client.state == CLIENT_CONNECTED)
	{
		PortalPacket packet;
		while(PortalClientReceive(&NETWORK_STATE.client, &packet))
		{
			NetworkPacket * netPacket = (NetworkPacket*)ArenaPush(&NETWORK_STATE.framePackets, sizeof(NetworkPacket));
			void * data = ArenaPush(&NETWORK_STATE.framePackets, packet.size);
			netPacket->size = packet.size;
			netPacket->data = data;
			netPacket->id = packet.peerID;
			memcpy(data, packet.data, packet.size);
			NetworkEvent event = {NET_EVENT_PACKET, packet.peerID, netPacket};
			NETWORK_STATE.clientEvents[clientEventCount++] = event;
		}
	}

	if (NETWORK_STATE.server.started)
	{
		PortalEvent serverEvent;
		while(PortalServerGetEvent(&NETWORK_STATE.server, &serverEvent))
		{
			switch (serverEvent.type) {
				case PORTAL_EVENT_CLIENT_CONNECT:
				{
					NetworkEvent event = {NET_EVENT_CLIENT_CONNECTED, serverEvent.id, NULL};
					NETWORK_STATE.serverEvents[serverEventCount++] = event;
				} break;

				case PORTAL_EVENT_CLIENT_DISCONNECT:
				{
					NetworkEvent event = {NET_EVENT_CLIENT_DISCONNECTED, serverEvent.id, NULL};
					NETWORK_STATE.serverEvents[serverEventCount++] = event;
				} break;
			}
		}

		PortalPacket packet = {0};
		while (NETWORK_STATE.server.started && PortalServerReceive(&NETWORK_STATE.server, &packet))
		{
			NetworkPacket * netPacket = (NetworkPacket*)ArenaPush(&NETWORK_STATE.framePackets, sizeof(NetworkPacket));
			void * data = ArenaPush(&NETWORK_STATE.framePackets, packet.size);
			netPacket->size = packet.size;
			netPacket->data = data;
			netPacket->id = packet.peerID;
			memcpy(data, packet.data, packet.size);
			NetworkEvent event = {NET_EVENT_PACKET, packet.peerID, netPacket};
			NETWORK_STATE.serverEvents[serverEventCount++] = event;
		}
	}

	input->clientEventCount = clientEventCount;
	input->clientEvents = NETWORK_STATE.clientEvents;
	input->serverEventCount = serverEventCount;
	input->serverEvents = NETWORK_STATE.serverEvents;
}
