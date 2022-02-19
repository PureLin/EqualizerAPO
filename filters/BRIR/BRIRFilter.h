#pragma once

#include <vector>
#include "../UDPReceiver.h"
#include "IFilter.h"
#include "BRIRConvolutionFilter.h"
#include <thread>
#include <io.h>
#include <cstdio>
#include <map>
#include "../BiQuadFilter.h"
#include "BRIRLowPassFilter.h"

#pragma AVRT_VTABLES_BEGIN
class BRIRFilter : public IFilter
{
public:
	BRIRFilter(int port, std::wstring name, std::wstring path, int channelToHeadDegree[8],float bassPercent);
	virtual ~BRIRFilter();

	bool getAllChannels() override { return false; }
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
	void initLoHiFilter();
	void doLoPass();
	void doConv(int index);
private:

	void inline calculateChannelToBrirDistance(int direction);
	void inline copyInputData();
	void inline resetBuff(unsigned int frameCount);

	int currentSampleRateIndex = -1;
	int sampleRates[4]{ 44100,48000,88200,96000 };
	
	int inputChannelCount = 2;
	bool hasLFEChannel = false;
	int lfeChannel = -1;
	int* inputHasData;

	float volumePrecent = 0.5;
	float bassPercent = 0.8;
	int channelToHeadDegree[8]{ -30, 30, 0, 0, -135, 135, -90, 90 };
	std::map<int, std::vector<std::wstring>> brFilesMap;
	std::allocator<BRIRConvolutionFilter> convAllocator;

	int udpPort = 2055;
	std::wstring brirPath;
	std::wstring name;

	int brirSize;
	int degreePerBrir;
	BRIRConvolutionFilter* convFilters;
	unsigned int maxBrFrameCount = 0;
	unsigned int frameCount = 0;

	BRIRLowPassFilter* loPassFilter;
	float* loPassBuffer;
	float** currentInput;
	float*** convBuffer;

	int** lastDistance;
	int** currentDistance;
	int* brirNeedConv;

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
