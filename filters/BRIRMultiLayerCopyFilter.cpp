#include "stdafx.h"
#include "BRIRMultiLayerCopyFilter.h"
#include "helpers/LogHelper.h"
#include <exception>
#include <atltime.h>
#include <math.h>
#include "../direction/DriectionCal.h"
#include<map>
using namespace std;

#pragma AVRT_VTABLES_BEGIN
namespace BRIRMULTI {
	static const int layDegree[5]{ -60,-30,0,30,60 };
	static const int layCount = 5;
	static const int layPosCount = 12;
	static const int layHorzionDegree = 30;
	static const int layVerticalDegree = 30;
	static float bassVolume = 0.4;



	double inline volume(double d) {
		return cos(d / 30.0 * M_PI_2);
	}

	static float volumePrecent;
	static bool inputHasData[8];
	static int lfeChannel = 3;
	VOID
		CALLBACK
		ConvWorkCallBack(
			PTP_CALLBACK_INSTANCE Instance,
			PVOID                 Parameter,
			PTP_WORK              Work
		)
	{
		BRIRMultiLayerCopyFilter::processOneChannelBrir((ConvJobInfo*)Parameter);
		return;
	}

	VOID
		CALLBACK
		LoPassWorkCallBack(
			PTP_CALLBACK_INSTANCE Instance,
			PVOID                 Parameter,
			PTP_WORK              Work
		)
	{
		float** output = BRIRMultiLayerCopyFilter::currentOutput;
		int frameCount = BRIRMultiLayerCopyFilter::frameCount;
		((BRIRLowPassFilter*)Parameter)->process(output[0], frameCount);
		copy(output[0], output[0] + frameCount, output[1]);
		for (int f = 0; f != frameCount; ++f) {
			output[0][f] *= bassVolume;
			output[1][f] *= bassVolume;
		}
		return;
	}

}

using namespace BRIRMULTI;

BRIRConvolutionFilter* BRIRMultiLayerCopyFilter::convFilters[5][12];

int BRIRMultiLayerCopyFilter::lastDistance[5][12][8][2];
int BRIRMultiLayerCopyFilter::currentDistance[5][12][8][2];
unsigned int BRIRMultiLayerCopyFilter::bufsize = 10240;
unsigned int BRIRMultiLayerCopyFilter::maxBrFrameCount = 0;
unsigned int BRIRMultiLayerCopyFilter::frameCount = 0;
float** BRIRMultiLayerCopyFilter::currentOutput;
allocator<BRIRConvolutionFilter> BRIRMultiLayerCopyFilter::convAllocator;

//5 layer->max 12 brir->2 ear for each brir
float* BRIRMultiLayerCopyFilter::mlInBuffer[5][12][2];
//indicate this brir need conv
int BRIRMultiLayerCopyFilter::brirNeedConv[5][12];

ConvJobInfo BRIRMultiLayerCopyFilter::convJobs[60];
PTP_WORK BRIRMultiLayerCopyFilter::works[60];
PTP_WORK BRIRMultiLayerCopyFilter::loPassWork;

bool BRIRMultiLayerCopyFilter::init = false;
std::vector <std::wstring> BRIRMultiLayerCopyFilter::convChannel;
#pragma AVRT_VTABLES_END



void BRIRMultiLayerCopyFilter::initMlBuff() {
	for (int lay = 0; lay != 5; ++lay) {
		for (int br = 0; br != 12; ++br) {
			for (int er = 0; er != 2; ++er) {
				if (mlInBuffer[lay][br][er] != 0) {
					delete[] mlInBuffer[lay][br][er];
				}
				mlInBuffer[lay][br][er] = new float[bufsize];
			}
		}
	}
}

void inline BRIRMultiLayerCopyFilter::copyInputData(float** input, unsigned int frameCount) {
	for (int lay = 0; lay != 5; ++lay) {
		for (int br = 0; br != 12; ++br) {
			fill(mlInBuffer[lay][br][0], mlInBuffer[lay][br][0] + frameCount, 0);
			fill(mlInBuffer[lay][br][1], mlInBuffer[lay][br][1] + frameCount, 0);
		}
	}
	for (int lay = 0; lay != 5; ++lay) {
		for (int br = 0; br != 12; ++br) {
			for (int ch = 0; ch != 8; ++ch) {
				if (lastDistance[lay][br][ch][0] != 30 && lastDistance[lay][br][ch][1] != 30
					|| currentDistance[lay][br][ch][0] != 30 && currentDistance[lay][br][ch][1] != 30) {
					float vSpeed = float(currentDistance[lay][br][ch][0] - lastDistance[lay][br][ch][0]) / frameCount;
					float hSpeed = float(currentDistance[lay][br][ch][1] - lastDistance[lay][br][ch][1]) / frameCount;
					float vdist = vSpeed + lastDistance[lay][br][ch][0];
					float hdist = hSpeed + lastDistance[lay][br][ch][1];
					for (int f = 0; f != frameCount; ++f) {
						float v = input[ch][f] * volume(vdist) * volume(hdist) *volumePrecent;
						mlInBuffer[lay][br][0][f] += v;
						mlInBuffer[lay][br][1][f] += v;
						vdist += vSpeed;
						hdist += hSpeed;
					}
					brirNeedConv[lay][br] = maxBrFrameCount / frameCount + 1;
				}
				lastDistance[lay][br][ch][0] = currentDistance[lay][br][ch][0];
				lastDistance[lay][br][ch][1] = currentDistance[lay][br][ch][1];
			}
		}
	}
}

void inline calculateVolume(double toLeftLowPercent, double* volume) {
	double degree = toLeftLowPercent * M_PI_2;
	volume[0] = cos(degree);
	volume[1] = sin(degree);
}

BRIRMultiLayerCopyFilter::BRIRMultiLayerCopyFilter(int port, float bassV, wstring path) {
	if (!init) {
		try {
			LogF(L"Use UDP port %d", port);
			bassVolume = bassV;
			bool result = UDPReceiver::initUdpReceiver(port);
			LogF(L"Create UDPReceiver Finished");
			for (int lay = 0; lay != 5; ++lay) {
				for (int br = 0; br != layPosCount; ++br) {
					wstring configPath(path);
					configPath.append(to_wstring(lay)).append(L"\\").append(to_wstring(br)).append(L".wav");
					LogF(L"Create ConvFilter %d-%d %ls", lay, br, configPath.c_str());
					convFilters[lay][br] = convAllocator.allocate(1);
					convAllocator.construct(convFilters[lay][br], configPath);
				}
			}
			LogF(L"create buffsize %d", bufsize);
			initMlBuff();
			convChannel.push_back(L"ToL");
			convChannel.push_back(L"ToR");
			LogF(L"create threadpool");
			init = true;
		}
		catch (exception e) {
			LogF(L"Create UDPReceiver Failed");
		}
	}
}


BRIRMultiLayerCopyFilter::~BRIRMultiLayerCopyFilter() {
	delete loPassFilter;
}

void BRIRMultiLayerCopyFilter::processOneChannelBrir(ConvJobInfo* job) {
	__try {
		convFilters[job->layer][job->pos]->process(mlInBuffer[job->layer][job->pos], mlInBuffer[job->layer][job->pos], frameCount);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {}
}


void inline calculateDistance(int inputChannelCount) {
	fill(BRIRMultiLayerCopyFilter::currentDistance[0][0][0], BRIRMultiLayerCopyFilter::currentDistance[0][0][0] + 5 * 8 * 12 * 2, 30);
	for (int ch = 0; ch != inputChannelCount; ++ch) {
		if (ch != lfeChannel && inputHasData[ch]) {
			SoundDirection d = actualDirection[ch];
			int vertical = max(min(d.vertical, 60), -60) + 60;
			int vd1 = vertical % layVerticalDegree;
			int vd2 = layVerticalDegree - vd1;
			int vp1 = vertical / layVerticalDegree;
			int vp2 = (vp1 + 1) % layCount;
			int horizon = d.horizon + 180;
			int hd1 = horizon % layHorzionDegree;
			int hd2 = layHorzionDegree - hd1;
			int hp1 = (horizon / layHorzionDegree) % layPosCount;
			int hp2 = (hp1 + 1) % layPosCount;
			BRIRMultiLayerCopyFilter::currentDistance[vp1][hp1][ch][0] = vd1;
			BRIRMultiLayerCopyFilter::currentDistance[vp1][hp1][ch][1] = hd1;
			BRIRMultiLayerCopyFilter::currentDistance[vp1][hp2][ch][0] = vd1;
			BRIRMultiLayerCopyFilter::currentDistance[vp1][hp2][ch][1] = hd2;
			BRIRMultiLayerCopyFilter::currentDistance[vp2][hp1][ch][0] = vd2;
			BRIRMultiLayerCopyFilter::currentDistance[vp2][hp1][ch][1] = hd1;
			BRIRMultiLayerCopyFilter::currentDistance[vp2][hp2][ch][0] = vd2;
			BRIRMultiLayerCopyFilter::currentDistance[vp2][hp2][ch][1] = hd2;
		}
	}
}

#pragma AVRT_CODE_BEGIN
std::vector <std::wstring> BRIRMultiLayerCopyFilter::initialize(float sampleRate, unsigned int maxFrameCount, std::vector <std::wstring> channelNames) {

	inputChannels = min((int)channelNames.size(), 8);
	volumePrecent = 4 / double(inputChannels + 4);
	for (std::wstring br : channelNames) {
		TraceF(L"in channel: %br", br);
	}
	std::vector <std::wstring> outputchannel;
	outputchannel.push_back(L"L");
	outputchannel.push_back(L"R");
	if (!init) {
		LogF(L"not init !");
	}
	else {
		if (maxFrameCount > bufsize) {
			LogF(L"bufsize not enough, reacreate buff %d", maxFrameCount);
			bufsize = maxFrameCount;
			initMlBuff();
		}
		for (int lay = 0; lay != 5; ++lay) {
			for (int ch = 0; ch != layPosCount; ++ch) {
				TraceF(L"init convFilter %d", ch);
				convFilters[lay][ch]->sampleRate = sampleRate;
				convFilters[lay][ch]->maxInputFrameCount = maxFrameCount;
				convFilters[lay][ch]->initialize();
				maxBrFrameCount = max(maxBrFrameCount, convFilters[lay][ch]->fileFrameCount);
			}
		}
	}
	loPassFilter = new BRIRLowPassFilter(sampleRate, frameCount);
	fill(lastDistance[0][0][0], lastDistance[0][0][0] + 5 * 8 * 12 * 2, 30);
	fill(brirNeedConv[0], brirNeedConv[0] + 60, 0);
	return outputchannel;
}


void BRIRMultiLayerCopyFilter::process(float** output, float** input, unsigned int frameCount) {
	if (!init) {
		LogF(L"BRIR not init !");
		return;
	}
	try {
		this->frameCount = frameCount;
		currentOutput = output;
		fill(output[0], output[0] + frameCount, 0.0);
		for (int ch = 0; ch != inputChannels; ++ch) {
			for (int f = 0; f != frameCount; ++f) {
				output[0][f] += input[ch][f];
				if (input[ch][f]) {
					inputHasData[ch] = true;
				}
			}
		}
		loPassWork = CreateThreadpoolWork(LoPassWorkCallBack, loPassFilter, NULL);
		SubmitThreadpoolWork(loPassWork);
		double* data = (double*)UDPReceiver::globalReceiver->unionBuff.data;
		Position postion(data);
		calculateDirection(postion, inputChannels);
		calculateDistance(inputChannels);
		copyInputData(input, frameCount);
		int convJobCount = 0;
		for (int lay = 0; lay != 5; ++lay) {
			for (int br = 0; br != layPosCount; ++br) {
				if (brirNeedConv[lay][br] != 0) {
					--brirNeedConv[lay][br];
					convJobs[convJobCount].layer = lay;
					convJobs[convJobCount].pos = br;
					++convJobCount;
				}
			}
		}
		for (int jc = 0; jc != convJobCount; ++jc) {
			works[jc] = CreateThreadpoolWork(ConvWorkCallBack, convJobs + jc, NULL);
			SubmitThreadpoolWork(works[jc]);
		}
		WaitForThreadpoolWorkCallbacks(loPassWork, false);
		CloseThreadpoolWork(loPassWork);
		for (int jc = 0; jc != convJobCount; ++jc) {
			WaitForThreadpoolWorkCallbacks(works[jc], false);
			ConvJobInfo job = convJobs[jc];
			for (int ear = 0; ear != 2; ++ear) {
				for (unsigned f = 0; f < frameCount; f++) {
					output[ear][f] += mlInBuffer[job.layer][job.pos][ear][f];
				}
			}
			CloseThreadpoolWork(works[jc]);
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