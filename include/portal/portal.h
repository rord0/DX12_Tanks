#ifndef PORTAL_H
#define PORTAL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
	extern "C" {
#endif

#ifdef _WIN32
    typedef uint64_t SOCKET_HANDLE;
#else
    typedef int SOCKET_HANDLE;
#endif

#define PORTAL_MAX_PACKET_SIZE  1024
#define PACKET_QUEUE_START_SIZE 512
#define HEARTBEAT_SEND_RATE 0.5
#define PORTAL_CLIENT_PORT_ANY 0
#define SEQUENCE_BUFFER_SIZE 1024
#define PORTAL_SERVER_MAX_CLIENTS 8
#define SERVER_MAX_CONNECTION_REQUESTS 8

#define PORTAL_API extern

typedef enum {ADDRESS_INVALID, ADDRESS_IPV4, ADDRESS_IPV6} addressType;

typedef struct {
    addressType type;
    union { uint8_t ipv4[4]; uint16_t ipv6[8]; } data;
    unsigned short port;
} PortalAddress;

typedef enum {
    SOCKET_TYPE_IPV4,
    SOCKET_TYPE_IPV6
} PortalSocketType;

typedef enum
{
    PACKET_CONNECT,
    PACKET_REJECT,
    PACKET_CHALLENGE,
    PACKET_RESPONSE,
    PACKET_HEARTBEAT,
    PACKET_PAYLOAD,
    PACKET_DISCONNECT,
    PT_PACKET_ACK,
    NUM_PACKET_TYPES
} packetType;

typedef enum
{
    CONNECTION_REJECTED_SERVER_FULL,
    CONNECTION_REJECTED_ALREADY_CONNECTED,
    CONNECTION_REJECTED_TOO_MANY_REQUESTS
} connectedRejectedReason;

typedef enum
{
    PORTAL_SEND_UNRELIABLE,
    PORTAL_SEND_RELIABLE
} PortalPacketSendMode;

typedef enum
{
    SERVER_CONNECTION_TIMED_OUT,
    SERVER_KICKED,
    PORTAL_SERVER_STOPPED,
    PORTAL_DISCONNECT_REQUESTED
} PortalDisconnectReason;

typedef enum
{
    PORTAL_EVENT_CLIENT_CONNECT,
    PORTAL_EVENT_CLIENT_DISCONNECT
} PortalEventType;

typedef struct {
	PortalEventType type;
	uint32_t id;
} PortalEvent;

typedef enum {
    PORTAL_CLIENT_STATE_DISCONNECTED,
    PORTAL_CLIENT_STATE_REQUESTING_CONNECTION,
    PORTAL_CLIENT_STATE_SENDING_CHALLENGE_RESPONSE,
    PORTAL_CLIENT_STATE_CONNECTED,
} PortalClientState;

typedef struct
{
    uint16_t peerID;
    uint8_t channel;
    uint16_t size;
    void * data;
} PortalPacket;

typedef struct
{
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint32_t capacity;
	size_t   elementSize;
    void * memory;
} PortalQueue;

typedef struct
{
    unsigned int elementSize;
    unsigned int count;
    unsigned int capacity;
    void * data;
} uArray;

typedef enum
{
    PACKET_SENT,
    PACKET_RESEND,
    PACKET_ACKED
} sentPacketState;

typedef struct
{
    uint16_t seqNum;
    double timeSent;
    uint16_t size;
    void * userData;
} sentPacket;

typedef struct
{
    uint16_t localSeq;
    uint16_t remoteSeq;

    uint32_t remoteSeqBuffer[SEQUENCE_BUFFER_SIZE];
    uint8_t remoteACKBuffer[SEQUENCE_BUFFER_SIZE];

    uint32_t localSeqBuffer[SEQUENCE_BUFFER_SIZE];
    uint8_t localACKBuffer[SEQUENCE_BUFFER_SIZE];
} sequencer;

typedef struct
{
    sequencer seq;                      // Sequencer to keep track of which packets have been ACKed.
    uArray sentPackets;                 // Contains packet data for reliable packets sent.
    double time;                        // The current time in seconds.
    double lastSendTime;                // The time which the last reliable packet was sent.
    uint32_t packetCountSinceLastACK;   // The number of packets receieved since the last time a message containing ACK data was sent.
} reliableChannel;

typedef struct
{
    uint16_t seqNum;
    uint16_t ack;
    uint32_t ackBits;
} reliableHeaderData;

typedef struct {
    int isConnected;
    double lastPacketReceiveTime;
    double lastPacketSendTime;
    PortalAddress clientAddress;
    uint64_t clientSalt;
    uint64_t challengeSalt;
    reliableChannel * reliableChannel;
} serverClientSlot;

typedef struct {
    PortalAddress clientAddress;
    uint64_t clientSalt;
    uint64_t serverSalt;
    double requestTime;
} pendingConnection;

typedef struct {
    double time;
    int maxClients;
    int numClientsConnected;
    serverClientSlot clientSlots[PORTAL_SERVER_MAX_CLIENTS];
    int pendingConnectionsCount;
    pendingConnection pendingConnections[SERVER_MAX_CONNECTION_REQUESTS];
    PortalAddress serverAddress;
    SOCKET_HANDLE serverSocket;
    int started;
    PortalQueue receiveQueue;
	PortalQueue eventQueue;
} PortalServer;

typedef struct {
    SOCKET_HANDLE clientSocket;
    uint64_t clientSalt;
    uint64_t serverSalt;
    uint32_t clientIndex;
    double time;
    double lastPacketSendTime;
    double lastPacketReceiveTime;
    PortalAddress serverAddress;
    PortalClientState state;
    PortalQueue receiveQueue;
    reliableChannel * rChannel;
} PortalClient;

// Portal public API functions:
PORTAL_API  int PortalInit(void * (*mallocFunction)(size_t size));
PORTAL_API void PortalShutdown();
PORTAL_API void PortalSleep(uint32_t ms);
PORTAL_API PortalAddress PortalStrToIPv4Address(const char * addrStr, unsigned short port);
PORTAL_API PortalAddress PortalStrToIPv6Address(const char * addrStr, unsigned short port);

// Server API
PORTAL_API PortalServer PortalCreateServer(PortalSocketType socketType, unsigned short port, int maxConnections);
PORTAL_API void PortalServerUpdate(PortalServer * server, double time);
PORTAL_API void PortalServerSend(PortalServer * server, void * data, uint32_t size, PortalPacketSendMode sendMode, uint32_t clientIndex);
PORTAL_API void PortalServerSendAll(PortalServer * server, void * data, uint32_t size, PortalPacketSendMode sendMode);
PORTAL_API  int PortalServerReceive(PortalServer * server, PortalPacket * outPacket);
PORTAL_API  int PortalServerGetEvent(PortalServer * server, PortalEvent * outEvent);
PORTAL_API void PortalServerDestroy(PortalServer * server);

// Client API
PORTAL_API PortalClient PortalCreateClient(PortalSocketType socketType, unsigned short port);
PORTAL_API void PortalClientConnect(PortalClient * client, PortalAddress serverAddress);
PORTAL_API void PortalClientUpdate(PortalClient * client, double time);
PORTAL_API void PortalClientSend(PortalClient * client, void * data, unsigned int size, PortalPacketSendMode sendMode);
PORTAL_API  int PortalClientReceive(PortalClient * client, PortalPacket * outPacket);
PORTAL_API void PortalClientDestroy(PortalClient * client);

#ifdef __cplusplus
}
#endif

#endif // PORTAL_H
