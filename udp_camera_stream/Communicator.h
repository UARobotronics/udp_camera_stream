#pragma once

#include <string>
#include <vector>

#include "WinSock2.h"

#include "Command.h"
#include "Common.h"

enum class PeerType
{
	Server = 0,
	Client = 1
};

class Communicator
{
public:
	Communicator();
	~Communicator();

	bool init();
	void setDestination(const std::string& dstAddress, const unsigned int dstPort);
	void setSocketTimeout(DWORD ms);
	void resetSocketTimeout();

	bool sendData(const char* data, const size_t dataLen);
	int receiveData(DataBuffer& buffer, bool* isTimeout);

	bool sendCmdConnectToServer();
	bool sendCmdSetColorDepth(uint8_t colorDepth);
	bool sendCmdSetSkipRows(uint8_t skipRows);
	bool sendCmdSetMinPacketSize(uint16_t packetSize);

private:
	ULONG getAddressByDNS(const std::string& dnsName);

private:
	SOCKET m_socket = INVALID_SOCKET;
	unsigned int m_port = 8080;
	unsigned int m_dstPort = 0;
	std::string m_dstAddress = "";
};