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

bool Communicator::init()
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
	si.sin_port = htons(m_port);
	si.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(m_socket, reinterpret_cast<sockaddr*>(&si), sizeof(si)) == SOCKET_ERROR)
	{
		int error = WSAGetLastError();

		return false;
	}

	return true;
}

void Communicator::setDestination(const std::string& dstAddress, const unsigned int dstPort)
{
	m_dstAddress = dstAddress;
	m_dstPort = dstPort;
}

void Communicator::setSocketTimeout(DWORD ms)
{
	setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&ms, sizeof(ms));
}

void Communicator::resetSocketTimeout()
{
	DWORD timeoutMs = 0;
	setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));
}

bool Communicator::sendData(const char* data, const size_t dataLen)
{
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(m_dstPort);
	inet_pton(AF_INET, m_dstAddress.c_str(), &addr.sin_addr);

	int res = sendto(m_socket, data, dataLen, 0, (sockaddr*)(&addr), sizeof(addr));

	return (res != SOCKET_ERROR);
}

// buffer size must be large enough to receive all data
// contained in a socket
int Communicator::receiveData(DataBuffer& buffer, bool* isTimeout = nullptr)
{
	sockaddr_in from;
	int fromLen = sizeof(from);
	int len = recvfrom(m_socket, reinterpret_cast<char*>(buffer.data()), buffer.size(), 0,
		reinterpret_cast<sockaddr*>(&from), &fromLen);

	if (isTimeout)
		*isTimeout = ((len == SOCKET_ERROR) && (WSAGetLastError() == WSAETIMEDOUT));

	return len;
}

bool Communicator::sendCmdConnectToServer()
{
	std::vector<char> buf = { char(CMD_CONNECT_TO_SERVER) };

	return sendData(buf.data(), buf.size());
}

bool Communicator::sendCmdSetColorDepth(uint8_t colorDepth)
{
	std::vector<char> buf(CMD_SEND_COLOR_DEPTH_TO_SERVER_SIZE);
	buf[0] = char(CMD_SEND_COLOR_DEPTH_TO_SERVER);
	buf[1] = char(colorDepth);

	return sendData(buf.data(), buf.size());
}

bool Communicator::sendCmdSetSkipRows(uint8_t skipRows)
{
	std::vector<char> buf(CMD_SEND_SKIP_ROWS_TO_SERVER_SIZE);
	buf[0] = char(CMD_SEND_SKIP_ROWS_TO_SERVER);
	buf[1] = char(skipRows);

	return sendData(buf.data(), buf.size());
}

bool Communicator::sendCmdSetMinPacketSize(uint16_t packetSize)
{
	std::vector<char> buf(CMD_SEND_MIN_PACKET_SIZE_TO_SERVER_SIZE);
	buf[0] = char(CMD_SEND_MIN_PACKET_SIZE_TO_SERVER);
	buf[1] = char(packetSize >> 8);
	buf[2] = char(packetSize & 0xFF);

	return sendData(buf.data(), buf.size());
}

ULONG Communicator::getAddressByDNS(const std::string& dnsName)
{
	ULONG result = INADDR_NONE;

	struct addrinfo hints = { 0 };
	hints.ai_family = AF_UNSPEC; // AF_INET to force IPv4, AF_INET6 for IPv6
	hints.ai_socktype = SOCK_STREAM;
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
