#pragma once

#include <iostream>
#include <string>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif

inline bool resolveHost(const std::string& server_host, std::string& ip) {
	struct addrinfo hints, * result;
	int status;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	status = getaddrinfo(server_host.c_str(), nullptr, &hints, &result);
	if (status != 0) {
		std::cerr << "getaddrinfo failed: "
#ifdef _WIN32
			<< WSAGetLastError()
#else
			<< gai_strerror(status)
#endif
			<< std::endl;
		return false;
	}

	struct sockaddr_in* sockaddr_ipv4 = (struct sockaddr_in*)result->ai_addr;
	char rip[INET_ADDRSTRLEN];

#ifdef _WIN32
	InetNtopA(AF_INET, &sockaddr_ipv4->sin_addr, rip, INET_ADDRSTRLEN);
#else
	inet_ntop(AF_INET, &sockaddr_ipv4->sin_addr, rip, INET_ADDRSTRLEN);
#endif

#ifdef DEBUG
	std::cout << "Resolved IP: " << rip << std::endl;
#endif
	freeaddrinfo(result);
	ip = std::string(rip);
	return true;
}


inline uint32_t crc32(const vector<uint8_t>& data) {
	uint32_t crc = 0xFFFFFFFF;

	for (uint8_t byte : data) {
		crc ^= byte;
		for (int i = 0; i < 8; i++) {
			if (crc & 1) {
				crc = (crc >> 1) ^ 0xEDB88320;
			}
			else {
				crc >>= 1;
			}
		}
	}

	return ~crc;
}