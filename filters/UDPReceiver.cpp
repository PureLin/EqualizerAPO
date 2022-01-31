#pragma once
#include "stdafx.h"
#include "UDPReceiver.h"
#include <exception>
#include "../helpers/LogHelper.h"
#include "../helpers/MemoryHelper.h"

static bool udpCreated = false;
static HANDLE hThread;
static DWORD threadId;
UDPReceiver* UDPReceiver::globalReceiver = NULL;

DWORD WINAPI UDPReceiver::UdpReceive(LPVOID p)
{
	LogFStatic(L"UdpReceive start !");
	UDPReceiver* receiver = (UDPReceiver*)p;
	receiver->doReceive();
	return 0;
}

bool UDPReceiver::initUdpReceiver(int port) {
	if (!udpCreated) {
		try {
			void* mem = MemoryHelper::alloc(sizeof(UDPReceiver));
			globalReceiver = new(mem)UDPReceiver(port);
			hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)UdpReceive, (void*)globalReceiver, 0, &threadId);
			udpCreated = true;
			return true;
		}
		catch (std::exception e) {
			LogFStatic(L"create udp receiver failed");
			return false;
		}
	}
	return true;
}


int getPortOffset()
{
	wchar_t szFileFullPath[MAX_PATH];
	::GetModuleFileNameW(NULL, szFileFullPath, MAX_PATH);
	int length = ::lstrlen(szFileFullPath);
	std::wstring fullpath(szFileFullPath);
	if (fullpath.find(L"Benchmark") != std::wstring::npos) {
		return 1;
	}
	if (fullpath.find(L"Voicemeeter") != std::wstring::npos) {
		return 2;
	}
	return 0;
}

UDPReceiver::UDPReceiver(int p) {
	if (p != 0) {
		port = p;
	}
	port += getPortOffset();
	std::fill(unionBuff.data, unionBuff.data + 25, 0);
	senderAddrSize = sizeof(recvAddr);
	WSAStartup(MAKEWORD(2, 2), &wsData);
	recvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	recvAddr.sin_family = AF_INET;
	recvAddr.sin_port = htons(port);
	recvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	int result = bind(recvSocket, (SOCKADDR*)&recvAddr, sizeof(recvAddr));
	if (result != 0) {
		LogF(L"Can't open port %d with result code %d", port, result);
		throw std::exception();
	}
}

void UDPReceiver::doReceive() {
	while (open) {
		int result = recvfrom(recvSocket, (char*)unionBuff.byte, bufLen, 0, (SOCKADDR*)&senderAddr, &senderAddrSize);
	}
}
void UDPReceiver::CloseUDP() {
	LogF(L"Shutdown udp receiver !");
	open = false;
	closesocket(recvSocket);
	WSACleanup();
}