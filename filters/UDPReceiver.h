#pragma once
#include"stdafx.h"
#include <array>
#include<WinSock2.h>
#include<iostream>

#pragma comment(lib,"WS2_32.lib")

class UDPReceiver{
public:
	volatile union{
		double data[32];
		char byte[256];
	}unionBuff;
	volatile boolean open = true;
	int port = 2055;
	static bool initUdpReceiver(int port);
	static UDPReceiver* globalReceiver;

private:
	int senderAddrSize;
	WSADATA wsData;
	SOCKET recvSocket;
	sockaddr_in recvAddr;
	sockaddr_in senderAddr;
	int bufLen = 256;
	UDPReceiver(int p);
	void doReceive();
	void CloseUDP();
	
	static DWORD WINAPI UdpReceive(LPVOID p);

	~UDPReceiver(){
		CloseUDP();
	}
};


