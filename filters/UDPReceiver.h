#pragma once
#include"stdafx.h"
#include <array>
#include<WinSock2.h>
#include<iostream>

#pragma comment(lib,"WS2_32.lib")

class UDPReceiver {
public:
	volatile union {
		double data[32];
		char byte[256];
	}unionBuff;

	int getDirection();
	bool start();

	static bool initUdpReceiver(int port);
	static UDPReceiver* globalReceiver;
	~UDPReceiver() {
		CloseUDP();
	}

private:
	UDPReceiver(int p);
	volatile boolean open = true;

	int getYaw();
	int portOffset;
	int port = 2055;
	int senderAddrSize;
	WSADATA wsData;
	SOCKET recvSocket;
	sockaddr_in recvAddr;
	sockaddr_in senderAddr;
	int bufLen = 256;
	void doReceive();
	void CloseUDP();

	static DWORD WINAPI UdpReceive(LPVOID p);


};


