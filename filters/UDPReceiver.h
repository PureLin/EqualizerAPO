#pragma once
#include"stdafx.h"
#include <array>
#include<WinSock2.h>
#include<iostream>

#pragma comment(lib,"WS2_32.lib")

enum UDPDataType {
	number,
	character
};

class UDPReceiver{
public:
	volatile union {
		double data[32];
		char byte[256];
	}unionBuff; 
	
	int getDirection(UDPDataType dataType);
	void doGetHotKey();

	static bool initUdpReceiver(int port);
	static UDPReceiver* globalReceiver;

private:
	volatile boolean open = true;
	
	double centerYaw = 1000;
	MSG hotKeyMsg = { 0 };

	int port = 2055;
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


