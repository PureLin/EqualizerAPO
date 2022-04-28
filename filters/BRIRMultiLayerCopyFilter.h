#pragma once

#include <vector>
#include "UDPReceiver.h"
#include "IFilter.h"
#include "ConvolutionFilter.h"
#include "BRIR/BRIRConvolutionFilter.h"
#include "BRIR/BRIRLowPassFilter.h"

static struct CopyJobInfo {
	int layer;
	int brirPos;
	int sourceChannel;
	double volume;
	CopyJobInfo(int l, int b, int s, double v) {
		layer = l; brirPos = b; sourceChannel = s; volume = v;
	}
};
static struct ConvJobInfo {
	void* filter;
	int layer;
	int pos;
};
#pragma AVRT_VTABLES_BEGIN
class BRIRMultiLayerCopyFilter : public IFilter
{
public:
	BRIRMultiLayerCopyFilter(int port, float bassVolume, std::wstring path, float loPassFreq[3]);
	virtual ~BRIRMultiLayerCopyFilter();


	bool getAllChannels() override { return true; }
	bool getInPlace() override { return false; }
	std::vector<std::wstring> initialize(float sampleRate, unsigned maxFrameCount, std::vector<std::wstring> channelNames) override;
	void process(float** output, float** input, unsigned frameCount) override;
	

	void processOneChannelBrir(ConvJobInfo* job);
	std::allocator<BRIRConvolutionFilter> convAllocator;
	BRIRLowPassFilter* loPassFilter;
	float loPassFreq[3];

	void initMlBuff();
	void copyInputData(float** input, unsigned int frameCount);
	BRIRConvolutionFilter* convFilters[5][12];
	unsigned int bufsize = 10240;
	unsigned int maxBrFrameCount = 0;
	unsigned int frameCount = 0;
	int inputChannels;

	float** currentOutput;
	//5 layer->max 12 brir->2 ear for each brir
	float* mlInBuffer[5][12][2];
	//indicate this brir need conv
	int brirNeedConv[5][12];

	int lastDistance[5][12][8][2];
	int currentDistance[5][12][8][2];

	ConvJobInfo convJobs[60];
	PTP_WORK works[60];
	PTP_WORK loPassWork;

	std::vector <std::wstring> convChannel;
};
#pragma AVRT_VTABLES_END
