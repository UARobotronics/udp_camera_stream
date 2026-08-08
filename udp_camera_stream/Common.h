#pragma once

enum class State
{
	Undefined = 0,
	ConnectToServer = 1,
	WaitConfirmation = 2,
	WaitStreamParams = 3,
	ReceiveVideoStream = 4,
};

struct Timeouts
{
	static const int serverConfirmation = 1000;
	static const int newDataPacket = 3000;
};

using DataBuffer = std::vector<unsigned char>;