#include "pch.h"

#include "Communicator.h"

#include <winsock2.h>
#include <Ws2tcpip.h>

Communicator::Communicator()
{
}

Communicator::~Communicator()
{
	closesocket(m_socket);

	WSACleanup();
}

bool Communicator::initServer()
{
	return init(PeerType::Server, INADDR_ANY);
}

bool Communicator::initClientByAddress(const std::string& ipAddr)
{
	IN_ADDR a;
	int res = inet_pton(AF_INET, ipAddr.c_str(), &a);

	if (res == 1)
		return init(PeerType::Client, a.s_addr);
	else
		return false;
}

bool Communicator::initClientByDNS(const std::string& dnsName)
{
	return true;
}

bool Communicator::sendData(const std::string& dnsName, const std::string& data)
{
	return true;
}

// buffer size must be large enough to receive all data
// contained in a socket
int Communicator::receiveData(std::vector<unsigned char>& buffer)
{
	sockaddr_in from;
	int fromLen = sizeof(from);
	int len = recvfrom(m_socket, reinterpret_cast<char*>(buffer.data()), buffer.size(), 0,
		reinterpret_cast<sockaddr*>(&from), &fromLen);

	return len;
}

ULONG Communicator::getAddressByDNS(const std::string& dnsName)
{
	ULONG result = INADDR_NONE;

	struct addrinfo hints = { 0 };
	hints.ai_family = AF_UNSPEC;     // AF_INET to force IPv4, AF_INET6 for IPv6
	hints.ai_socktype = SOCK_STREAM;   // TCP
	hints.ai_protocol = IPPROTO_TCP;

	addrinfo* pAddr = nullptr;
	int rc = getaddrinfo(dnsName.c_str(), "80", &hints, &pAddr);
	if (rc != 0)
		return 0;

	if (pAddr)
	{
		struct sockaddr_in* v4 = (struct sockaddr_in*)pAddr->ai_addr;
		result = v4->sin_addr.s_addr;
	}

	freeaddrinfo(pAddr);

	return result;
}

bool Communicator::init(PeerType type, ULONG addr)
{
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return false;

	m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_socket == INVALID_SOCKET)
		return false;

	sockaddr_in si;
	ZeroMemory(&si, sizeof(si));
	si.sin_family = AF_INET;
	si.sin_addr.s_addr = addr;
	si.sin_port = htons(m_port);

	if (bind(m_socket, reinterpret_cast<sockaddr*>(&si), sizeof(si)) == SOCKET_ERROR)
		return false;

	return true;
}
