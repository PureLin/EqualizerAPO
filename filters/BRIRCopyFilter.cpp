#include "stdafx.h"
#include "BRIRCopyFilter.h"
#include "helpers/LogHelper.h"
#include <exception>
#include <atltime.h>
#include <math.h>

using namespace std;

#pragma AVRT_VTABLES_BEGIN
static ConvolutionFilter* convFilters[12];
static unsigned int bufsize = 10240;
static unsigned int brFrameCount = 0;
//12 brir->2 ear for each brir
static float* inbuffer[12][2];
//indicate this brir need conv
static boolean brirNeedConv[12];
static int jobIndex[12]{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
static float volumePrecent;

static bool init = false;
static std::vector <std::wstring> convChannel;
static int SourceToHeadDiff[8]{ -30, 30, 0, 0, -135, 135, -70, 70 };
static PTP_POOL pool = NULL;
static PTP_WORK works[12];
#pragma AVRT_VTABLES_END

void initBuff() {
	for (int br = 0; br != 12; ++br) {
		for (int er = 0; er != 2; ++er) {
			if (inbuffer[br][er] != 0) {
				delete[] inbuffer[br][er];
			}
			inbuffer[br][er] = new float[bufsize];
		}
	}
}

void inline copyInputData(int* brir, float* volume, unsigned int frameCount, float* oneChannelInput) {
	for (int sch = 0; sch != 2; ++sch) {
		if (volume[sch] == 0) {
			continue;
		}
		if (!brirNeedConv[brir[sch]]) {
			for (int er = 0; er != 2; ++er) {
				for (unsigned f = 0; f < frameCount; f++) {
					inbuffer[brir[sch]][er][f] = oneChannelInput[f] * volume[sch];
				}
			}
		}
		else {
			for (int er = 0; er != 2; ++er) {
				for (unsigned f = 0; f < frameCount; f++) {
					inbuffer[brir[sch]][er][f] += oneChannelInput[f] * volume[sch];
				}
			}
		}
		brirNeedConv[brir[sch]] = true;
	}
}

void inline calculatePosAndVolume(int* brir, float* volume, int sourceMappingDirection) {
	brir[0] = sourceMappingDirection / 30;
	brir[1] = (brir[0] + 1) % 12;
	double degree = float(sourceMappingDirection % 30) * M_PI_2 / 30;
	volume[0] = cos(degree);
	volume[1] = sin(degree);
	volume[0] *= volumePrecent;
	volume[1] *= volumePrecent;
}

ConvolutionFilter* createConvFilter(wstring& filePath) {
	void* mem = MemoryHelper::alloc(sizeof(ConvolutionFilter));
	return new(mem)ConvolutionFilter(filePath);
}

int overflow(int pos) {
	if (pos < 0) {
		pos += 360;
	}
	if (pos >= 360) {
		pos -= 360;
	}
	return pos;
}

BRIRCopyFilter::BRIRCopyFilter(int port, wstring path) {
	if (!init) {
		LogF(L"Use UDP port %d", port);
		if (!UDPReceiver::initUdpReceiver(port)) {
			LogF(L"UDP receiver init fialed");
			return;
		}
		LogF(L"Create UDPReceiver Finished");
		for (int ch = 0; ch != 12; ++ch) {
			wstring configPath(path);
			configPath.append(to_wstring(ch)).append(L".wav");
			LogF(L"Create ConvFilter %d %ls", ch, configPath.c_str());
			convFilters[ch] = createConvFilter(configPath);
		}
		LogF(L"create buffsize %d", bufsize);
		initBuff();
		convChannel.push_back(L"ToL");
		convChannel.push_back(L"ToR");
		LogF(L"create threadpool");
		pool = CreateThreadpool(NULL);
		SetThreadpoolThreadMaximum(pool, 12);
		SetThreadpoolThreadMinimum(pool, 12);
		init = true;
	}
}


BRIRCopyFilter::~BRIRCopyFilter() {
}

std::vector <std::wstring> BRIRCopyFilter::initialize(float sampleRate, unsigned int maxFrameCount, std::vector <std::wstring> channelNames) {

	inputChannels = min((int)channelNames.size(), 8);
	volumePrecent = 4 / double(inputChannels + 4);
	for (std::wstring br : channelNames) {
		TraceF(L"in channel: %br", br);
	}
	std::vector <std::wstring> outputchannel;
	outputchannel.push_back(L"HRIRL");
	outputchannel.push_back(L"HRIRR");
	if (!init) {
		LogF(L"not init !");
	}
	else {
		if (maxFrameCount > bufsize) {
			LogF(L"bufsize not enough, reacreate buff %d", maxFrameCount);
			bufsize = maxFrameCount;
			initBuff();
		}
		for (int ch = 0; ch != 12; ++ch) {
			TraceF(L"init convFilter %d", ch);
			convFilters[ch]->initialize(sampleRate, maxFrameCount, convChannel);
		}
	}
	return outputchannel;
}


void processOneChannelBrir(int br) {
	convFilters[br]->process(inbuffer[br], inbuffer[br], brFrameCount);
}

VOID
CALLBACK
BRIRWorkCallBack(
	PTP_CALLBACK_INSTANCE Instance,
	PVOID                 Parameter,
	PTP_WORK              Work
)
{
	processOneChannelBrir(*(int*)Parameter);
	return;
}

#pragma AVRT_CODE_BEGIN
void BRIRCopyFilter::process(float** output, float** input, unsigned int frameCount) {
	if (!init) {
		LogF(L"BRIR not init !");
		return;
	}
	try {
		brFrameCount = frameCount;
		double* data = (double*)UDPReceiver::globalReceiver->unionBuff.data;
		double yaw = data[3];
		int direction = 180 - int(yaw / 4);
		//avoid error yaw input
		if (direction < 0 || direction>360) {
			direction = 180;
		}
		for (int br = 0; br != 12; ++br) {
			brirNeedConv[br] = false;
		}
		int brir[2];
		float volume[2];
		for (int ch = 0; ch != inputChannels; ++ch) {
			int sourceMappingDirection = overflow(direction + SourceToHeadDiff[ch]);
			TraceF(L"Channel %d direction %d", ch, sourceMappingDirection);
			//for each source channel need 2 brir to sim the actual pos
			//the volume will depend on direction,colser bigger
			calculatePosAndVolume(brir, volume, sourceMappingDirection);
			//first time input data need be copy not add
			copyInputData(brir, volume, frameCount, input[ch]);
		}

		for (int br = 0; br != 12; ++br) {
			if (brirNeedConv[br]) {
				works[br] = CreateThreadpoolWork(BRIRWorkCallBack, jobIndex + br, NULL);
				SubmitThreadpoolWork(works[br]);
			}
		}
		for (int ear = 0; ear != 2; ++ear) {
			for (unsigned f = 0; f < frameCount; f++) {
				output[ear][f] = 0;
			}
		}
		for (int br = 0; br != 12; ++br) {
			if (brirNeedConv[br]) {
				WaitForThreadpoolWorkCallbacks(works[br], false);
				for (int ear = 0; ear != 2; ++ear) {
					for (unsigned f = 0; f < frameCount; f++) {
						output[ear][f] += inbuffer[br][ear][f];
					}
				}
				CloseThreadpoolWork(works[br]);
			}
		}
	}
	catch (exception e) {
		LogF(L"Exception Occurs");
		LogF(L"%br", e.what());
	}
	catch (...) {
		LogF(L"Exception Occurs");
	}
}
#pragma AVRT_CODE_END