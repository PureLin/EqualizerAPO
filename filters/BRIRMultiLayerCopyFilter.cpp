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
static int layDegree[5]{ -60,-30,0,30,60 };
static int layPosCount[5]{ 8,12,12,12,8 };
static int layHorzionDegree[5]{ 45,30,30,30,45 };

static ConvolutionFilter* convFilters[5][12];

static unsigned int bufsize = 10240;
static unsigned int brFrameCount = 0;
//5 layer->max 12 brir->2 ear for each brir
static float* mlInBuffer[5][12][2];
//indicate this brir need conv
static boolean brirNeedConv[5][12];
static struct CopyJobInfo {
	int layer;
	int brirPos;
	int sourceChannel;
	float volume;
	CopyJobInfo(int l, int b, int s, float v) {
		layer = l; brirPos = b; sourceChannel = s; volume = v;
	}
};
static struct ConvJobInfo {
	int layer;
	int pos;
};
static ConvJobInfo convJobs[32];
static PTP_WORK works[32];

static float volumePrecent;
static bool init = false;
static std::vector <std::wstring> convChannel;
static PTP_POOL pool = NULL;
#pragma AVRT_VTABLES_END

void initMlBuff() {
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

void inline copyInputData(vector<CopyJobInfo>& jobs, float** input, unsigned int frameCount) {
	for (CopyJobInfo job : jobs) {
		if (!brirNeedConv[job.layer][job.brirPos]) {
			for (int er = 0; er != 2; ++er) {
				for (unsigned f = 0; f < frameCount; f++) {
					mlInBuffer[job.layer][job.brirPos][er][f] = input[job.sourceChannel][f] * job.volume;
				}
			}
		}
		else {
			TraceFStatic(L"Reuse br channel %d-%d", job.layer, job.brirPos);
			for (int er = 0; er != 2; ++er) {
				for (unsigned f = 0; f < frameCount; f++) {
					mlInBuffer[job.layer][job.brirPos][er][f] += input[job.sourceChannel][f] * job.volume;
				}
			}
		}
		brirNeedConv[job.layer][job.brirPos] = true;
	}
}

void inline calculateVolume(float toLeftLowPercent, float* volume) {
	double degree = toLeftLowPercent * M_PI_2;
	volume[0] = cos(degree);
	volume[1] = sin(degree);
}


ConvolutionFilter* createFilter(wstring& filePath) {
	void* mem = MemoryHelper::alloc(sizeof(ConvolutionFilter));
	return new(mem)ConvolutionFilter(filePath);
}

BRIRMultiLayerCopyFilter::BRIRMultiLayerCopyFilter(int port, wstring path) {
	if (!init) {
		try {
			LogF(L"Use UDP port %d", port);
			bool result = UDPReceiver::initUdpReceiver(port);
			LogF(L"Create UDPReceiver Finished");
			for (int lay = 0; lay != 5; ++lay) {
				for (int ch = 0; ch != layPosCount[lay]; ++ch) {
					wstring configPath(path);
					configPath.append(to_wstring(lay)).append(L"\\").append(to_wstring(ch)).append(L".wav");
					LogF(L"Create ConvFilter %d-%d %ls", lay, ch, configPath.c_str());
					convFilters[lay][ch] = createFilter(configPath);
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
}

void processOneChannelBrir(ConvJobInfo* job) {
	convFilters[job->layer][job->pos]->process(mlInBuffer[job->layer][job->pos], mlInBuffer[job->layer][job->pos], brFrameCount);
}

VOID
CALLBACK
ConvWorkCallBack(
	PTP_CALLBACK_INSTANCE Instance,
	PVOID                 Parameter,
	PTP_WORK              Work
)
{
	processOneChannelBrir((ConvJobInfo*)Parameter);
	return;
}

void inline createLayerJob(vector<CopyJobInfo>& job, int lay, int horizon, float volume, int ch) {
	horizon += 180;
	int degree = layHorzionDegree[lay];
	if (horizon % degree == 0) {
		job.push_back(CopyJobInfo(lay, horizon / degree, ch, volume));
	}
	else {
		float horizonVolume[2];
		calculateVolume(float(horizon % degree) / degree, horizonVolume);
		job.push_back(CopyJobInfo(lay, horizon / degree, ch, volume * horizonVolume[0]));
		job.push_back(CopyJobInfo(lay, horizon / degree + 1, ch, volume * horizonVolume[1]));
	}
}

vector<CopyJobInfo> createOnechannelJob(int ch) {
	vector<CopyJobInfo> job;
	int downDegree = -1000;
	SoundDirection d = actualDirection[ch];
	int lay = 0;
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
			float volume[2];
			calculateVolume(float(d.vertical - downDegree) / 30.0, volume);
			createLayerJob(job, lay - 1, d.horizon, volume[0], ch);
			createLayerJob(job, lay, d.horizon, volume[1], ch);
		}
		break;
	}
	if (lay == 5) {
		createLayerJob(job, 4, d.horizon, 1.0, ch);
	}
	float totalVolume = 0;
	for (CopyJobInfo j : job) {
		totalVolume += j.volume * j.volume;
	}
	for (CopyJobInfo j : job) {
		j.volume /= totalVolume;
	}
	return job;
}

vector<CopyJobInfo> createJob(int inputChannels) {
	vector<CopyJobInfo> job;
	for (int ch = 0; ch != inputChannels; ++ch) {
		vector<CopyJobInfo> oneChanneljob = createOnechannelJob(ch);
		job.insert(job.end(), oneChanneljob.begin(), oneChanneljob.end());
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
	outputchannel.push_back(L"HRIRL");
	outputchannel.push_back(L"HRIRR");
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
			for (int ch = 0; ch != layPosCount[lay]; ++ch) {
				TraceF(L"init convFilter %d", ch);
				convFilters[lay][ch]->initialize(sampleRate, maxFrameCount, convChannel);
			}
		}
	}
	return outputchannel;
}


void BRIRMultiLayerCopyFilter::process(float** output, float** input, unsigned int frameCount) {
	if (!init) {
		LogF(L"BRIR not init !");
		return;
	}
	try {
		brFrameCount = frameCount;
		double* data = (double*)UDPReceiver::globalReceiver->unionBuff.data;
		data[5] = data[0] = data[1] = data[2] = 0;
		Position postion(data);
		calculateDirection(postion, inputChannels);
		vector<CopyJobInfo> jobs = createJob(inputChannels);
		for (int lay = 0; lay != 5; ++lay) {
			for (int br = 0; br != layPosCount[lay]; ++br) {
				brirNeedConv[lay][br] = false;
			}
		}
		copyInputData(jobs, input, frameCount);
		int convJobCount = 0;
		for (int lay = 0; lay != 5; ++lay) {
			for (int br = 0; br != layPosCount[lay]; ++br) {
				if (brirNeedConv[lay][br]) {
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
		for (int ear = 0; ear != 2; ++ear) {
			for (unsigned f = 0; f < frameCount; f++) {
				output[ear][f] = 0;
			}
		}
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