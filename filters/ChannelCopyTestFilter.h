#pragma once

#include <vector>
#include "UDPReceiver.h"
#include "IFilter.h"
#include "ConvolutionFilter.h"

#pragma AVRT_VTABLES_BEGIN
class ChannelCopyTestFilter : public IFilter
{
public:
	ChannelCopyTestFilter();
	virtual ~ChannelCopyTestFilter();

	bool getAllChannels() override { return true; }
	bool getInPlace() override { return false; }
	std::vector<std::wstring> initialize(float sampleRate, unsigned maxFrameCount, std::vector<std::wstring> channelNames) override;
	void process(float** output, float** input, unsigned frameCount) override;
private:
	DWORD threadId;
	HANDLE hThread;
	int inputChannels;
	int port = 2055;
	bool useLinearPos;
};
#pragma AVRT_VTABLES_END
