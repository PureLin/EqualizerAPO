#pragma once

#include <vector>
#include "UDPReceiver.h"
#include "IFilter.h"
#include "ConvolutionFilter.h"

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
private:
	int inputChannels;
};
#pragma AVRT_VTABLES_END
