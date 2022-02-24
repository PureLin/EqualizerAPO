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
	int layer;
	int pos;
};
#pragma AVRT_VTABLES_BEGIN
class BRIRMultiLayerCopyFilter : public IFilter
{
public:
	BRIRMultiLayerCopyFilter(int port, float bassVolume, std::wstring path);
	virtual ~BRIRMultiLayerCopyFilter();


	bool getAllChannels() override { return true; }
	bool getInPlace() override { return false; }
	std::vector<std::wstring> initialize(float sampleRate, unsigned maxFrameCount, std::vector<std::wstring> channelNames) override;
	void process(float** output, float** input, unsigned frameCount) override;
	

	static void processOneChannelBrir(ConvJobInfo* job);
	static std::allocator<BRIRConvolutionFilter> convAllocator;
	BRIRLowPassFilter* loPassFilter;


	void initMlBuff();
	void copyInputData(float** input, unsigned int frameCount);
	static BRIRConvolutionFilter* convFilters[5][12];
	static unsigned int bufsize;
	static unsigned int maxBrFrameCount;
	static unsigned int frameCount;
	int inputChannels;

	static float** currentOutput;
	//5 layer->max 12 brir->2 ear for each brir
	static float* mlInBuffer[5][12][2];
	//indicate this brir need conv
	static int brirNeedConv[5][12];

	static int lastDistance[5][12][8][2];
	static int currentDistance[5][12][8][2];

	static ConvJobInfo convJobs[60];
	static PTP_WORK works[60];
	static PTP_WORK loPassWork;

	static bool init;
	static std::vector <std::wstring> convChannel;
};
#pragma AVRT_VTABLES_END
