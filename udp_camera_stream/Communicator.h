#pragma once

#include <string>
#include <vector>

#include "WinSock2.h"

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

	bool initServer();
	bool initClientByAddress(const std::string& ipAddr);
	bool initClientByDNS(const std::string& dnsName);

	bool sendData(const std::string& dnsName, const std::string& data);
	int receiveData(std::vector<unsigned char>& buffer);

private:
	ULONG getAddressByDNS(const std::string& dnsName);
	bool init(PeerType type, ULONG addr);

private:
	SOCKET m_socket = INVALID_SOCKET;
	unsigned int m_port = 8080;
};