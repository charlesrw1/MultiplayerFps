#ifndef NETWORK_H
#define NETWORK_H
#include "Util.h"

enum ServerToClient
{
	SvMessageStart,
	SvMessageSnapshot,
	SvMessageDisconnect,
	SvMessageText,
	SvMessageTick,
};
enum ClientToServer
{
	ClMessageInput,
	ClMessageQuit,
	ClMessageText,
	ClMessageTick,
};

enum SharedMessages
{
	NetMessageEnd = 0xFF,
};

#endif // !NETWORK_H
