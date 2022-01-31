#pragma once

#include <vector>
#include "UDPReceiver.h"
#include "IFilter.h"
#include "ConvolutionFilter.h"

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
	BRIRMultiLayerCopyFilter(int port, std::wstring path);
	virtual ~BRIRMultiLayerCopyFilter();


	bool getAllChannels() override { return true; }
	bool getInPlace() override { return false; }
	std::vector<std::wstring> initialize(float sampleRate, unsigned maxFrameCount, std::vector<std::wstring> channelNames) override;
	void process(float** output, float** input, unsigned frameCount) override;
	
	void initMlBuff();
	void copyInputData(std::vector<CopyJobInfo>& jobs, float** input, unsigned int frameCount);
	static ConvolutionFilter* convFilters[5][12];
	static unsigned int bufsize;
	static unsigned int brFrameCount;
	int inputChannels;

	//5 layer->max 12 brir->2 ear for each brir
	static float* mlInBuffer[5][12][2];
	//indicate this brir need conv
	static boolean brirNeedConv[5][12];

	static ConvJobInfo convJobs[32];
	static PTP_WORK works[32];

	static bool init;
	static std::vector <std::wstring> convChannel;
	static PTP_POOL pool;
};
#pragma AVRT_VTABLES_END
