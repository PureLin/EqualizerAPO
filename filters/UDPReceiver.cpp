#pragma once
#include "stdafx.h"
#include "UDPReceiver.h"
#include <exception>
#include<string>
#include "../helpers/LogHelper.h"
#include "../helpers/MemoryHelper.h"
#include <string>
#include <locale>
#include <codecvt>

static bool udpCreated = false;
static HANDLE hThread;
static DWORD threadId;
static HANDLE hotKeyThread;
static DWORD hotKeyThreadId;
UDPReceiver* UDPReceiver::globalReceiver = NULL;

using std::string;
using std::vector;

vector<string> split(const char* input) {
	vector<string> result;
	string item;
	for (; *input != 0; ++input) {
		if (*input == ',') {
			result.push_back(item);
			item = string();
		}
		else {
			item.push_back(*input);
		}
	}
	return result;
}

int UDPReceiver::getDirection() {
	int yaw = getYaw();
	if (yaw < -180) {
		yaw += 360;
	}
	if (yaw > 180) {
		yaw -= 360;
	}
	int direction = 180 + int(yaw);
	//avoid error yaw input
	if (direction < 0 || direction > 360) {
		direction = 180;
	}
	return direction;
}

int UDPReceiver::getYaw() {
	double* data = (double*)unionBuff.data;
	return data[3];
}

DWORD WINAPI UDPReceiver::UdpReceive(LPVOID p)
{
	UDPReceiver* receiver = (UDPReceiver*)p;
	receiver->doReceive();
	return 0;
}


bool UDPReceiver::initUdpReceiver(int port) {
	if (!udpCreated || globalReceiver->port != port) {
		try {
			UDPReceiver* old = globalReceiver;
			globalReceiver = new UDPReceiver(port);
			udpCreated = globalReceiver->start();
			delete old;
			return true;
		}
		catch (std::exception e) {
			LogFStatic(L"create udp receiver failed");
			return false;
		}
	}
	return true;
}


bool UDPReceiver::start() {
	try {
		hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)UdpReceive, (void*)this, 0, &threadId);
		return true;
	}
	catch (std::exception e) {
		LogF(L"start udp receiver failed");
		return false;
	}
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
	portOffset = getPortOffset();
	std::fill(unionBuff.data, unionBuff.data + 25, 0);
	senderAddrSize = sizeof(recvAddr);
	WSAStartup(MAKEWORD(2, 2), &wsData);
	recvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	recvAddr.sin_family = AF_INET;
	recvAddr.sin_port = htons(port + portOffset);
	recvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	int result = bind(recvSocket, (SOCKADDR*)&recvAddr, sizeof(recvAddr));
	if (result != 0) {
		LogF(L"Can't open port %d with result code %d", port + portOffset, result);
		throw std::exception();
	}
}

void UDPReceiver::doReceive() {
	LogF(L"UDP start to receive from port %d", port + portOffset);
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