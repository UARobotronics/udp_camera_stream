#pragma once

enum class State
{
	Undefined = 0,
	ConnectToServer = 1,
	WaitConfirmation = 2,
	ReceiveVideoStream = 3,
};
