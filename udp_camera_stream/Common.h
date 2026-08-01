#pragma once

enum class State
{
	Undefined = 0,
	ConnectToServer = 1,
	WaitConfirmation = 2,
	ReceiveVideoStream = 3,
};

struct Timeouts
{
	static const int serverConfirmation = 1000;
	static const int newDataPacket = 3000;
};