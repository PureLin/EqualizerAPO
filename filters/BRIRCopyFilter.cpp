#include "stdafx.h"
#include "BRIRCopyFilter.h"
#include "helpers/LogHelper.h"
#include <exception>
#include <atltime.h>

using namespace std;

static ConvolutionFilter* convFilters[12];
static int bufsize = 65536;
static float ** inbuffer;
static float ** outbuffer;
static UDPReceiver* receiver;
static bool init = false;
static std::vector <std::wstring> convChannel;
static int SourceToHeadDiff[8]{-30, 30, 0, 0, -135, 135 - 70, 70};
static int lastLeftBrir[2];

DWORD WINAPI ThreadFunc(LPVOID p)
{
	LogFStatic(L"ThreadFunc start !");
	UDPReceiver* receiver = (UDPReceiver*)p;
	receiver->doReceive();
	return 0;
}

DWORD WINAPI CopyAndConvFunc(LPVOID p){
	return 0;
}

ConvolutionFilter* createConvFilter(wstring& filePath){
	void* mem = MemoryHelper::alloc(sizeof(ConvolutionFilter));
	return new(mem)ConvolutionFilter(filePath);
}

int overflow(int pos){
	if (pos < 0){
		pos += 360;
	}
	if (pos >= 360){
		pos -= 360;
	}
	return pos;
}

BRIRCopyFilter::BRIRCopyFilter(int port, wstring path, bool useLinear){
	if (!init){
		try{
			if (port != 0){
				this->port = port;
			}
			this->useLinearPos = useLinear;
			LogF(L"Use LinearPos %d", useLinear);
			LogF(L"Create UDPReceiver");
			void* mem = MemoryHelper::alloc(sizeof(UDPReceiver));
			receiver = new(mem)UDPReceiver(this->port);
			hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadFunc, receiver, 0, &threadId);
			LogF(L"Create UDPReceiver Finished");
			for (int i = 0; i != 12; ++i){
				wstring configPath(path);
				configPath.append(to_wstring(i)).append(L".wav");
				LogF(L"Create ConvFilter %d %ls", i, configPath.c_str());
				convFilters[i] = createConvFilter(configPath);
			}
			LogF(L"create buffsize %d", bufsize);
			inbuffer = new float*[2];
			outbuffer = new float*[2];
			for (int i = 0; i != 2; ++i){
				outbuffer[i] = new float[bufsize];
			}
			convChannel.push_back(L"ToL");
			convChannel.push_back(L"ToR");
			LogF(L"init finished");
			init = true;
		}
		catch (exception e){
			LogF(L"Create UDPReceiver Failed");
			if (receiver != 0){
				receiver->open = false;
				CloseHandle(hThread);
			}
		}
	}
}


BRIRCopyFilter::~BRIRCopyFilter(){
}

std::vector <std::wstring> BRIRCopyFilter::initialize(float sampleRate, unsigned int maxFrameCount, std::vector <std::wstring> channelNames) {

	inputChannels = max(channelNames.size(), 8);
	for (std::wstring s : channelNames){
		TraceF(L"in channel: %s", s);
	}
	std::vector <std::wstring> outputchannel;
	outputchannel.push_back(L"HRIRL");
	outputchannel.push_back(L"HRIRR");
	if (!init){
		LogF(L"not init !");
	}
	else{
		for (int i = 0; i != 12; ++i){
			TraceF(L"init convFilter %d", i);
			convFilters[i]->initialize(sampleRate, maxFrameCount, convChannel);
		}
		for (int i = 0; i != 2; ++i){
			for (int j = 0; j != bufsize; ++j){
				outbuffer[i][j] = 0;
			}
		}
	}
	return outputchannel;
}

void BRIRCopyFilter::process(float **output, float **input, unsigned int frameCount) {
	if (!init){
		LogF(L"BRIR not init !");
		return;
	}
	try{
		double* data = (double *)receiver->unionBuff.data;
		double yaw = data[3];
		int direction = 180 - yaw / 5;
		//avoid error yaw input
		if (direction < 0 || direction>360){
			direction = 180;
		}
		for (int i = 0; i != 2; ++i){
			int sourceMappingDirection = overflow(direction + SourceToHeadDiff[i]);
			//for each source channel need 2 brir to sim the actual pos
			//the volume will depend on direction,colser bigger
			int brir[2];
			double volume[2];
			brir[0] = sourceMappingDirection / 30;
			brir[1] = (brir[0] + 1) % 12;
			if (useLinearPos){
				volume[1] = double(sourceMappingDirection % 30) / 30;
			}
			else{
				volume[1] = pow((double(sourceMappingDirection % 30) - 15) / 15, 3) / 2 + 0.5;
			}
			volume[0] = 1 - volume[1];
			if (lastLeftBrir[i] != brir[0]){
				TraceF(L"Channel %d, Yaw %f, frameCount %d", i, yaw, frameCount);
				TraceF(L"brir %d %d, volume %f %f", brir[0], brir[1], volume[0], volume[1]);
				lastLeftBrir[i] = brir[0];
			}
			for (int s = 0; s != 2; ++s){
				//copy the source channel data to one brir
				inbuffer[0] = input[i];
				inbuffer[1] = input[i];
				// then conv with the brir
				convFilters[brir[s]]->process(outbuffer, inbuffer, frameCount);
				//copy result to the leftear and rightear channel
				for (int bj = 0; bj != 2; ++bj){
					for (unsigned f = 0; f < frameCount; f++){
						output[bj][f] += outbuffer[bj][f] * volume[s];
					}
				}
			}
		}
	}
	catch (exception e){
		LogF(L"Exception Occurs");
		LogF(L"%s", e.what());
	}
	catch (...){
		LogF(L"Exception Occurs");
	}
}