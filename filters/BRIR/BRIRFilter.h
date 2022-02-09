#pragma once

#include <vector>
#include "../UDPReceiver.h"
#include "IFilter.h"
#include "BRIRConvolutionFilter.h"
#include <thread>
#include <io.h>
#include <cstdio>
#include <map>

#pragma AVRT_VTABLES_BEGIN
class BRIRFilter : public IFilter
{
public:
	BRIRFilter(int port,std::wstring name, std::wstring path, int SourceToHeadDiff[8]);
	virtual ~BRIRFilter();

	bool getAllChannels() override { return true; }
	bool getInPlace() override { return false; }
	std::vector<std::wstring> initialize(float sampleRate, unsigned maxFrameCount, std::vector<std::wstring> channelNames) override;
	void process(float** output, float** input, unsigned frameCount) override;

	struct ConvWork {
		BRIRFilter* filter;
		int convIndex;
	};
	void createBuff();
	void create();
	void init();
	void doConv(int index);
private:
	int currentSampleRateIndex = -1;
	int sampleRates[4]{ 44100,48000,88200,96000 };

	int inputChannels = 2;
	float volumePrecent = 0.5;
	int SourceToHeadDiff[8]{ -30, 30, 0, 0, -135, 135, -90, 90 };
	std::map<int, std::vector<std::wstring>> brFilesMap;
	std::allocator<BRIRConvolutionFilter> convAllocator;

	int udpPort = 2055;
	std::wstring brirPath;
	std::wstring name;

	int brirSize;
	int degreePerBrir;
	BRIRConvolutionFilter* convFilters;
	unsigned int frameCount = 0;
	float*** inbuffer;
	bool* brirNeedConv;

	//init thread
	PTP_WORK  createBuffWork;
	PTP_WORK* initWorks;
	//thread pool
	PTP_WORK works[14];
	ConvWork convWorks[14];
	enum Status
	{
		creating,
		initing,
		processing,
		error
	} status = creating;
};
#pragma AVRT_VTABLES_END
