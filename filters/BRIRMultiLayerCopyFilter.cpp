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
	static const int layPosCount = 12;
	static const int layHorzionDegree = 30;
	static float volumePrecent;
	static bool inputHasData[8];
	static int lfeChannel = 4;
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
		return;
	}

}

using namespace BRIRMULTI;

BRIRConvolutionFilter* BRIRMultiLayerCopyFilter::convFilters[5][12];


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
PTP_POOL BRIRMultiLayerCopyFilter::pool = NULL;
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

void inline BRIRMultiLayerCopyFilter::copyInputData(vector<CopyJobInfo>& jobs, float** input, unsigned int frameCount) {
	for (int lay = 0; lay != 5; ++lay) {
		for (int br = 0; br != 12; ++br) {
			fill(mlInBuffer[lay][br][0], mlInBuffer[lay][br][0] + frameCount, 0);
			fill(mlInBuffer[lay][br][1], mlInBuffer[lay][br][1] + frameCount, 0);
		}
	}
	for (CopyJobInfo job : jobs) {
		for (int er = 0; er != 2; ++er) {
			for (unsigned f = 0; f < frameCount; f++) {
				mlInBuffer[job.layer][job.brirPos][er][f] += input[job.sourceChannel][f] * job.volume;
			}
		}
		brirNeedConv[job.layer][job.brirPos] = maxBrFrameCount / frameCount + 1;
	}
}

void inline calculateVolume(double toLeftLowPercent, double* volume) {
	double degree = toLeftLowPercent * M_PI_2;
	volume[0] = cos(degree);
	volume[1] = sin(degree);
}

BRIRMultiLayerCopyFilter::BRIRMultiLayerCopyFilter(int port, wstring path) {
	if (!init) {
		try {
			LogF(L"Use UDP port %d", port);
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
			pool = CreateThreadpool(NULL);
			SetThreadpoolThreadMaximum(pool, 16);
			SetThreadpoolThreadMinimum(pool, 32);
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

void inline createLayerJob(vector<CopyJobInfo>& job, int lay, int horizon, double volume, int ch) {
	horizon += 180;
	int distance = horizon % layHorzionDegree;
	int pos1 = (horizon / layHorzionDegree) % layPosCount;
	int pos2 = (pos1 + 1) % layPosCount;
	if (distance == 0) {
		job.push_back(CopyJobInfo(lay, pos1, ch, volume));
	}
	else {
		double horizonVolume[2];
		calculateVolume(float(distance) / layHorzionDegree, horizonVolume);
		job.push_back(CopyJobInfo(lay, pos1, ch, volume * horizonVolume[0]));
		job.push_back(CopyJobInfo(lay, pos2, ch, volume * horizonVolume[1]));
	}
}

vector<CopyJobInfo> createOnechannelJob(int ch) {
	vector<CopyJobInfo> job;
	int downDegree = -1000;
	SoundDirection d = actualDirection[ch];
	int lay = 0;
	double volume[2];
	for (; lay != 5; ++lay) {
		if (layDegree[lay] < d.vertical) {
			downDegree = layDegree[lay];
			continue;
		}
		if (layDegree[lay] == d.vertical || downDegree == -1000) {
			createLayerJob(job, lay, d.horizon, 1.0, ch);
			break;
		}
		else {
			calculateVolume(float(d.vertical - downDegree) / 30.0, volume);
			createLayerJob(job, lay - 1, d.horizon, volume[0], ch);
			createLayerJob(job, lay, d.horizon, volume[1], ch);
		}
		break;
	}
	if (lay == 5) {
		createLayerJob(job, 4, d.horizon, 1.0, ch);
	}

	for (CopyJobInfo& j : job) {
		j.volume = j.volume * volumePrecent;
	}
	return job;
}

vector<CopyJobInfo> createJob(int inputChannelCount) {
	vector<CopyJobInfo> job;
	for (int ch = 0; ch != inputChannelCount; ++ch) {
		if (ch != lfeChannel && inputHasData[ch]) {
			vector<CopyJobInfo> oneChanneljob = createOnechannelJob(ch);
			job.insert(job.end(), oneChanneljob.begin(), oneChanneljob.end());
		}
	}
	return job;
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
		vector<CopyJobInfo> jobs = createJob(inputChannels);
		copyInputData(jobs, input, frameCount);
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