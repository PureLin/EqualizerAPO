#include "stdafx.h"
#include "BRIRFilter.h"
#include "helpers/LogHelper.h"
#include <exception>
#include <atltime.h>
#include <math.h>
using namespace std;

namespace BRIRFILTER {

	int inline overflow(int pos) {
		if (pos < 0) {
			pos += 360;
		}
		if (pos >= 360) {
			pos -= 360;
		}
		return pos;
	}

	int getDirection() {
		double* data = (double*)UDPReceiver::globalReceiver->unionBuff.data;
		double yaw = data[3];
		int direction = 180 - int(yaw / 4);
		//avoid error yaw input
		if (direction < 0 || direction > 360) {
			direction = 180;
		}
		return direction;
	}

	void inline calculatePosAndVolume(int brSize, int degreePerBrir, int* brir, float* volume, int sourceMappingDirection, float volumePrecent) {
		brir[0] = sourceMappingDirection / degreePerBrir;
		brir[1] = (brir[0] + 1) % brSize;
		double radian = float(sourceMappingDirection % degreePerBrir) / degreePerBrir * M_PI_2;
		volume[0] = cos(radian);
		volume[1] = sin(radian);
		volume[0] *= volumePrecent;
		volume[1] *= volumePrecent;
	}

	void inline copyInputData(bool* brirNeedConv, float*** inbuffer, int* brir, float* volume, unsigned int frameCount, float* oneChannelInput) {
		for (int sch = 0; sch != 2; ++sch) {
			if (volume[sch] == 0) {
				continue;
			}
			if (!brirNeedConv[brir[sch]]) {
				for (int er = 0; er != 2; ++er) {
					for (unsigned f = 0; f < frameCount; f++) {
						inbuffer[brir[sch]][er][f] = oneChannelInput[f] * volume[sch];
					}
				}
			}
			else {
				for (int er = 0; er != 2; ++er) {
					for (unsigned f = 0; f < frameCount; f++) {
						inbuffer[brir[sch]][er][f] += oneChannelInput[f] * volume[sch];
					}
				}
			}
			brirNeedConv[brir[sch]] = true;
		}
	}

	bool mycomp(const std::wstring& lhs, const std::wstring& rhs)
	{
		return StrCmpLogicalW(lhs.c_str(), rhs.c_str()) < 0;
	}

	void findOneSampleRateFile(wstring path, vector<wstring>& files) {
		_wfinddata_t file;
		intptr_t HANDLE;
		wstring findPath(path);
		findPath.append(L"\\*.*");
		HANDLE = _wfindfirst(findPath.c_str(), &file);
		if (HANDLE == -1L) {
			LogFStatic(L"can not find the folder path %ls", path);
			return;
		}
		do {
			if (!(file.attrib & _A_SUBDIR)) {
				wstring fname(file.name);
				if (fname.find(L".wav") != string::npos) {
					files.push_back(path + L"\\" + fname);
				}
			}
		} while (_wfindnext(HANDLE, &file) == 0);
		_findclose(HANDLE);
	}

	void findImpulsefile(wstring path, map<int, vector<wstring>>& filesMap) {
		_wfinddata_t file;
		intptr_t HANDLE;
		wstring findPath(path);
		findPath.append(L"\\*.*");
		HANDLE = _wfindfirst(findPath.c_str(), &file);
		if (HANDLE == -1L) {
			LogFStatic(L"can not find the folder path %ls", path);
			return;
		}
		do {
			if ((file.attrib & _A_SUBDIR) && (StrCmpW(file.name, L".") != 0) && (StrCmpW(file.name, L"..") != 0)) {
				//判断是否为"."当前目录，".."上一层目录
				wstring newPath = path + L"\\" + file.name;
				if (newPath.find(L"44") != string::npos) {
					findOneSampleRateFile(newPath, filesMap[0]);
				}
				if (newPath.find(L"48") != string::npos) {
					findOneSampleRateFile(newPath, filesMap[1]);
				}
				if (newPath.find(L"88") != string::npos) {
					findOneSampleRateFile(newPath, filesMap[2]);
				}
				if (newPath.find(L"96") != string::npos) {
					findOneSampleRateFile(newPath, filesMap[3]);
				}
			}
		} while (_wfindnext(HANDLE, &file) == 0);
		_findclose(HANDLE);
	}

	VOID CALLBACK toInit(PTP_CALLBACK_INSTANCE Instance, PVOID  Parameter, PTP_WORK Work) {
		((BRIRConvolutionFilter*)Parameter)->initialize();
	}
	VOID CALLBACK toCreateBuff(PTP_CALLBACK_INSTANCE Instance, PVOID  Parameter, PTP_WORK Work) {
		((BRIRFilter*)Parameter)->createBuff();
	}
	VOID CALLBACK BRIRWorkCallBack(PTP_CALLBACK_INSTANCE Instance, PVOID Parameter, PTP_WORK Work) {
		BRIRFilter::ConvWork* br = (BRIRFilter::ConvWork*)Parameter;
		br->filter->doConv(br->convIndex);
	}

}

using namespace BRIRFILTER;

#pragma AVRT_CODE_BEGIN

void BRIRFilter::doConv(int index) {
	convFilters[index].process(inbuffer[index], inbuffer[index], frameCount);
}

void BRIRFilter::create() {
	if (this->status == creating) {
		if (!UDPReceiver::initUdpReceiver(udpPort)) {
			return;
		}
		brFilesMap[0] = vector<wstring>();
		brFilesMap[1] = vector<wstring>();
		brFilesMap[2] = vector<wstring>();
		brFilesMap[3] = vector<wstring>();
		findImpulsefile(brirPath, brFilesMap);
		for (int j = 0; j != 14; ++j) {
			convWorks[j].filter = this;
		}
		status = initing;
	}
}

BRIRFilter::BRIRFilter(int port,wstring name, wstring path,int diff[8]) {
	this->udpPort = port;
	this->name = name;
	if (name.size() != 0) {
		path.append(name);
	}
	else {
		name = L"BRIR";
	}
	this->brirPath = path;
	for (int i = 0; i != 8; ++i) {
		SourceToHeadDiff[i] = diff[i];
	}
	create();
}


BRIRFilter::~BRIRFilter() {
	if (status != error) {
		for (int br = 0; br != brirSize; ++br) {
			convAllocator.destroy(convFilters + br);
			delete[] inbuffer[br][0];
			delete[] inbuffer[br][1];
		}
		convAllocator.deallocate(convFilters, brirSize);
		delete[] inbuffer;
		delete[] brirNeedConv;
		delete[] initWorks;
	}
}

void BRIRFilter::createBuff() {
	volumePrecent = 4 / double(inputChannels + 4);
	degreePerBrir = 360 / brirSize; 
	brirNeedConv = new bool[brirSize];
	inbuffer = new float** [brirSize];
	for (int br = 0; br != brirSize; ++br) {
		inbuffer[br] = new float* [2];
		for (int er = 0; er != 2; ++er) {
			inbuffer[br][er] = new float[frameCount];
		}
	}
}

void BRIRFilter::init() {
	if (status != initing) {
		status = error;
		return;
	}
	vector<wstring> files = brFilesMap[currentSampleRateIndex];
	sort(files.begin(), files.end(), mycomp);
	brirSize = files.size();
	if (brirSize == 0) {
		status = error;
		return;
	}
	createBuffWork = CreateThreadpoolWork(toCreateBuff, this, NULL);
	SubmitThreadpoolWork(createBuffWork);
	initWorks = new PTP_WORK[brirSize];
	convFilters = convAllocator.allocate(brirSize);
	int index = 0;
	for (wstring& s : files) {
		convAllocator.construct(convFilters + index++, s);
	}
	for (int br = 0; br != brirSize; ++br) {
		convFilters[br].sampleRate = sampleRates[currentSampleRateIndex];
		convFilters[br].maxFrameCount = frameCount;
		initWorks[br] = CreateThreadpoolWork(toInit, convFilters + br, NULL);
		SubmitThreadpoolWork(initWorks[br]);
	}
	for (int ch = 0; ch != brirSize; ++ch) {
		WaitForThreadpoolWorkCallbacks(initWorks[ch], false);
		CloseThreadpoolWork(initWorks[ch]);
	}
	WaitForThreadpoolWorkCallbacks(createBuffWork, false);
	CloseThreadpoolWork(createBuffWork);
	status = processing;
}

std::vector <std::wstring> BRIRFilter::initialize(float sampleRate, unsigned int maxFrameCount, std::vector <std::wstring> channelNames) {
	std::vector <std::wstring> outputchannel;
	outputchannel.push_back(name + L"-L");
	outputchannel.push_back(name + L"-R");
	inputChannels = min((int)channelNames.size(), 8);
	for (int sr = 0; sr != 4; ++sr) {
		if (sampleRates[sr] == sampleRate) {
			currentSampleRateIndex = sr;
		}
	}
	frameCount = maxFrameCount;
	if (currentSampleRateIndex != -1) {
		init();
	}
	return outputchannel;
}


void BRIRFilter::process(float** output, float** input, unsigned int frameCount) {
	if (status != processing) {
		for (int ear = 0; ear != 2; ++ear) {
			for (unsigned f = 0; f < frameCount; f++) {
				output[ear][f] = input[ear][f] * volumePrecent * 0.5;
			}
		}
		return;
	}
	try {
		this->frameCount = frameCount;
		int direction = getDirection();
		fill(brirNeedConv, brirNeedConv + brirSize, 0);
		int brir[2];
		float volume[2];
		for (int ch = 0; ch != inputChannels; ++ch) {
			int sourceMappingDirection = overflow(direction + SourceToHeadDiff[ch]);
			//for each source channel need 2 brir to sim the actual pos
			//the volume will depend on direction,colser bigger
			calculatePosAndVolume(brirSize, degreePerBrir, brir, volume, sourceMappingDirection, volumePrecent);
			//first time input data need be copy not add
			copyInputData(brirNeedConv, inbuffer, brir, volume, frameCount, input[ch]);
		}
		int jobIndex = 0;
		for (int br = 0; br != brirSize; ++br) {
			if (brirNeedConv[br]) {
				convWorks[jobIndex].convIndex = br;
				works[jobIndex] = CreateThreadpoolWork(BRIRWorkCallBack, convWorks + jobIndex, NULL);
				SubmitThreadpoolWork(works[jobIndex]);
				++jobIndex;
			}
		}
		for (int ear = 0; ear != 2; ++ear) {
			fill(output[ear], output[ear] + frameCount, 0);
		}
		for (int jb = 0; jb != jobIndex; ++jb) {
			WaitForThreadpoolWorkCallbacks(works[jb], false);
			int br = convWorks[jb].convIndex;
			for (int ear = 0; ear != 2; ++ear) {
				for (unsigned f = 0; f < frameCount; f++) {
					output[ear][f] += inbuffer[br][ear][f];
				}
			}
			CloseThreadpoolWork(works[jb]);
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