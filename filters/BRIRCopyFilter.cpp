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

DWORD WINAPI ThreadFunc(LPVOID p)
{
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

BRIRCopyFilter::BRIRCopyFilter(int port, wstring path){
	if (!init){
		try{
			if (port != 0){
				this->port = port;
			}
			LogF(L"Create UDPReceiver");
			void* mem = MemoryHelper::alloc(sizeof(UDPReceiver));
			receiver = new(mem)UDPReceiver(2055);
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
				inbuffer[i] = new float[bufsize];
				outbuffer[i] = new float[bufsize];
			}
			convChannel.push_back(L"ToL");
			convChannel.push_back(L"ToR");
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

	inputChannels = channelNames.size();
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
				inbuffer[i][j] = 0;
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
		TraceF(L"Yaw %f", yaw);
		int direction = 180 - yaw / 5;
		//avoid error yaw input
		if (direction < 0 || direction>360){
			direction = 180;
		}
		for (int i = 0; i != 2; ++i){
			int sourceMappingDirection = overflow(direction + SourceToHeadDiff[i]);
			//for each channel need 2 brir to sim the actual pos
			//the volume will depend on direction,colser bigger
			int brir[2];
			double volume[2];
			brir[0] = sourceMappingDirection / 30;
			brir[1] = (brir[0] + 1) % 12;
			volume[1] = double(sourceMappingDirection % 30) / 30;
			volume[0] = 1 - volume[1];
			//result with sound to leftear and rightear
			for (int s = 0; s != 2; ++s){
				//copy the source channel data to one brir
				for (int bj = 0; bj != 2; ++bj){
					for (unsigned f = 0; f < frameCount; f++){
						inbuffer[bj][f] = volume[s] * input[i][f];
					}
				}
				// then conv with the brir
				convFilters[brir[s]]->process(outbuffer, inbuffer, frameCount);
				//copy result to the leftear and rightear channel
				//*0.5 for not clipping
				for (int bj = 0; bj != 2; ++bj){
					for (unsigned f = 0; f < frameCount; f++){
						output[bj][f] += outbuffer[bj][f];
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