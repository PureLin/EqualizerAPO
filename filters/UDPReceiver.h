#pragma once
#include"stdafx.h"
#include<WinSock2.h>
#include<iostream>
#include "helpers/LogHelper.h"
#pragma comment(lib,"WS2_32.lib")


class UDPReceiver{
public:
	WSADATA wsData;
	SOCKET recvSocket;
	sockaddr_in recvAddr;
	int port = 2055;
	volatile union{
		double data[32];
		char byte[256];
	}unionBuff;
	int bufLen = 256;
	sockaddr_in senderAddr;
	int senderAddrSize;
	volatile boolean open = true;

	void doReceive(){
		while (open){
			recvfrom(recvSocket,(char*)unionBuff.byte, bufLen, 0, (SOCKADDR*)&senderAddr, &senderAddrSize);
		}
	}

	void CloseUDP(){
		open = false;
		closesocket(recvSocket);
		WSACleanup();
	}

	UDPReceiver(int port){
		senderAddrSize = sizeof(senderAddr);
		WSAStartup(MAKEWORD(2, 2), &wsData);
		recvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		recvAddr.sin_family = AF_INET;
		recvAddr.sin_port = htons(port);
		recvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
		int result = bind(recvSocket, (SOCKADDR*)&recvAddr, sizeof(recvAddr));
		if (result != 0){
			LogF(L"Can't open port %d with result code %d", port, result);
			throw std::exception();
		}
	}

	~UDPReceiver(){
		CloseUDP();
	}
};