#pragma once
#include "stdafx.h"
#include "UDPReceiver.h"
#include <exception>
#include<string>
#include "../helpers/LogHelper.h"
#include "../helpers/MemoryHelper.h"

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

int UDPReceiver::getDirection(UDPDataType dataType) {
	double yaw = 0;
	if (dataType == number) {
		double* data = (double*)UDPReceiver::globalReceiver->unionBuff.data;
		yaw = data[3];
	}
	if (dataType == character) {
		string s((char*)UDPReceiver::globalReceiver->unionBuff.byte);
		vector<string> data = split(s.c_str());
		if (data.size() >= 9) {
			yaw = stod(data.at(8)) * -1;
		}
	}
	if (centerYaw == 1000) {
		centerYaw = yaw;
	}
	if (hotKeyMsg.message == WM_HOTKEY) {
		centerYaw = yaw;
		hotKeyMsg = { 0 };
	}
	yaw = yaw - centerYaw;
	if (yaw < -180) {
		yaw += 360;
	}
	if (yaw > 180) {
		yaw -= 360;
	}
	int direction = 180 - int(yaw);
	//avoid error yaw input
	if (direction < 0 || direction > 360) {
		direction = 180;
	}
	return direction;
}

DWORD WINAPI UDPReceiver::UdpReceive(LPVOID p)
{
	LogFStatic(L"UdpReceive start !");
	UDPReceiver* receiver = (UDPReceiver*)p;
	receiver->doReceive();
	return 0;
}

DWORD WINAPI hotKeyReceive(LPVOID p)
{
	LogFStatic(L"UdpReceive hotkey start !");
	UDPReceiver* receiver = (UDPReceiver*)p;
	receiver->doGetHotKey();
	return 0;
}

bool UDPReceiver::initUdpReceiver(int port) {
	if (!udpCreated) {
		try {
			void* mem = MemoryHelper::alloc(sizeof(UDPReceiver));
			globalReceiver = new(mem)UDPReceiver(port);
			hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)UdpReceive, (void*)globalReceiver, 0, &threadId);
			hotKeyThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)hotKeyReceive, (void*)globalReceiver, 0, &hotKeyThreadId);
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

void UDPReceiver::doGetHotKey() {
	if (!RegisterHotKey(NULL, 1, MOD_SHIFT | MOD_ALT | MOD_CONTROL | MOD_NOREPEAT, VK_F1))
	{
		LogFStatic(L"UdpReceive hotkey register failed !");
		return;
	}
	while (open) {
		GetMessage(&hotKeyMsg, NULL, 0, 0);
	}
}
void UDPReceiver::CloseUDP() {
	LogF(L"Shutdown udp receiver !");
	open = false;
	closesocket(recvSocket);
	WSACleanup();
}