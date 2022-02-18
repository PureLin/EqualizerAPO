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

	double inline volume(double distancePercent) {
		return cos(distancePercent * M_PI_2);
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
	VOID CALLBACK toLoPass(PTP_CALLBACK_INSTANCE Instance, PVOID  Parameter, PTP_WORK Work) {
		((BRIRFilter*)Parameter)->doLoPass();
	}
	VOID CALLBACK toInitLoHiPass(PTP_CALLBACK_INSTANCE Instance, PVOID  Parameter, PTP_WORK Work) {
		((BRIRFilter*)Parameter)->initLoHiFilter();
	}
	VOID CALLBACK BRIRWorkCallBack(PTP_CALLBACK_INSTANCE Instance, PVOID Parameter, PTP_WORK Work) {
		BRIRFilter::ConvWork* br = (BRIRFilter::ConvWork*)Parameter;
		br->filter->doConv(br->convIndex);
	}

}

using namespace BRIRFILTER;

#pragma AVRT_CODE_BEGIN

void inline BRIRFilter::calculateChannelToBrirDistance(int direction) {
	swap(lastDistance, currentDistance);
	for (int ch = 0; ch != inputChannelCount; ++ch) {
		fill(currentDistance[ch], currentDistance[ch] + brirSize, degreePerBrir);
		int sourceMappingDirection = overflow(direction + channelToHeadDegree[ch]);
		int firstBrir = (sourceMappingDirection / degreePerBrir) % brirSize;
		int secondBrir = (firstBrir + 1) % brirSize;
		currentDistance[ch][firstBrir] = sourceMappingDirection % degreePerBrir;
		currentDistance[ch][secondBrir] = degreePerBrir - sourceMappingDirection % degreePerBrir;
	}
}
void inline BRIRFilter::resetBuff(unsigned int frameCount) {
	for (int br = 0; br != brirSize; ++br) {
		for (int er = 0; er != 2; ++er) {
			fill(convBuffer[br][er], convBuffer[br][er] + frameCount, 0.0);
		}
	}
}

void inline BRIRFilter::copyInputData() {
	resetBuff(frameCount);
	for (int ch = 0; ch != inputChannelCount; ++ch) {
		if (ch == lfeChannel || !inputHasData[ch]) {
			continue;
		}
		float* oneChannelInput = currentInput[ch];
		if (!oneChannelInput) {
			continue;
		}
		for (int br = 0; br != brirSize; ++br) {
			float startDistance = lastDistance[ch][br];
			float endDistance = currentDistance[ch][br];
			if (startDistance == endDistance) {
				if (startDistance == degreePerBrir) {
					continue;
				}
				float v = volume(startDistance / degreePerBrir);
				for (int er = 0; er != 2; ++er) {
					for (unsigned f = 0; f < frameCount; ++f) {
						convBuffer[br][er][f] += oneChannelInput[f] * v * volumePrecent;
					}
				}
			}
			else {
				float diff = (endDistance - startDistance) / frameCount;
				for (unsigned f = 0; f != frameCount; ++f) {
					startDistance += diff;
					float v = volume(startDistance / degreePerBrir);
					convBuffer[br][0][f] += oneChannelInput[f] * v * volumePrecent;
					convBuffer[br][1][f] += oneChannelInput[f] * v * volumePrecent;
				}
				/*
				unsigned f = 0;
				float diff = startDistance > endDistance ? -1 * moveSpeed : moveSpeed;
				startDistance += diff;
				for (; abs(startDistance - endDistance) > moveSpeed && f != frameCount; ++f, startDistance += diff) {
					float v = volume(startDistance / degreePerBrir);
					convBuffer[br][0][f] += oneChannelInput[f] * v * volumePrecent;
					convBuffer[br][1][f] += oneChannelInput[f] * v * volumePrecent;
				}
				if (f != frameCount) {
					float v = volume(endDistance / degreePerBrir);
					for (; f != frameCount; ++f) {
						convBuffer[br][0][f] += oneChannelInput[f] * v * volumePrecent;
						convBuffer[br][1][f] += oneChannelInput[f] * v * volumePrecent;
					}
				}
				else {
					LogF(L"move speed to fast to follow %f to %f", lastDistance[ch][br], endDistance);
					currentDistance[ch][br] = startDistance;
				}
				*/
			}
			brirNeedConv[br] = maxBrFrameCount / frameCount + 1;
		}
	}
}

void BRIRFilter::doLoPass() {
	loPassFilter->process(loPassBuffer, frameCount);
	for (int f = 0; f != frameCount; ++f) {
		loPassBuffer[f] *= bassPercent;
	}
}

void BRIRFilter::doConv(int index) {
	convFilters[index].process(convBuffer[index], convBuffer[index], frameCount);
}

void BRIRFilter::create() {
	if (status == creating) {
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

BRIRFilter::BRIRFilter(int port, wstring name, wstring path, int degree[8], float bassPercent) {
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
		channelToHeadDegree[i] = degree[i];
	}
	this->bassPercent = bassPercent;
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	create();
}


BRIRFilter::~BRIRFilter() {
	if (status != error) {
		for (int br = 0; br != brirSize; ++br) {
			convAllocator.destroy(convFilters + br);
			delete[] convBuffer[br][0];
			delete[] convBuffer[br][1];
		}
		convAllocator.deallocate(convFilters, brirSize);
		delete[] convBuffer;

		for (int ch = 0; ch != inputChannelCount; ++ch) {
			delete[] lastDistance[ch];
			delete[] currentDistance[ch];
		}
		delete[] lastDistance;
		delete[] currentDistance;
		delete[] loPassBuffer;

		delete[] brirNeedConv;
		delete[] inputHasData;
		delete loPassFilter;
	}
}

void BRIRFilter::createBuff() {
	brirNeedConv = new int[brirSize];
	fill(brirNeedConv, brirNeedConv + brirSize, 0);
	inputHasData = new int[inputChannelCount];
	convBuffer = new float** [brirSize];
	for (int br = 0; br != brirSize; ++br) {
		convBuffer[br] = new float* [2];
		for (int er = 0; er != 2; ++er) {
			convBuffer[br][er] = new float[frameCount];
		}
	}

	loPassBuffer = new float[frameCount];

	lastDistance = new int* [inputChannelCount];
	currentDistance = new int* [inputChannelCount];
	for (int ch = 0; ch != inputChannelCount; ++ch) {
		lastDistance[ch] = new int[brirSize];
		fill(lastDistance[ch], lastDistance[ch] + brirSize, degreePerBrir);
		currentDistance[ch] = new int[brirSize];
		fill(currentDistance[ch], currentDistance[ch] + brirSize, degreePerBrir);
	}
}

void BRIRFilter::initLoHiFilter() {
	int sampleRate = sampleRates[currentSampleRateIndex];
	loPassFilter = new BRIRLowPassFilter(sampleRate, frameCount);
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
	volumePrecent = 4 / double(inputChannelCount + 4);
	degreePerBrir = 360 / brirSize;
	PTP_WORK  createBuffWork = CreateThreadpoolWork(toCreateBuff, this, NULL);
	SubmitThreadpoolWork(createBuffWork);
	PTP_WORK loHiPassWork = CreateThreadpoolWork(toInitLoHiPass, this, NULL);
	SubmitThreadpoolWork(loHiPassWork);
	PTP_WORK* initWorks = new PTP_WORK[brirSize];
	convFilters = convAllocator.allocate(brirSize);
	int index = 0;
	for (wstring& s : files) {
		convAllocator.construct(convFilters + index++, s);
	}
	for (int br = 0; br != brirSize; ++br) {
		convFilters[br].sampleRate = sampleRates[currentSampleRateIndex];
		convFilters[br].maxInputFrameCount = frameCount;
		initWorks[br] = CreateThreadpoolWork(toInit, convFilters + br, NULL);
		SubmitThreadpoolWork(initWorks[br]);
	}
	for (int br = 0; br != brirSize; ++br) {
		WaitForThreadpoolWorkCallbacks(initWorks[br], false);
		CloseThreadpoolWork(initWorks[br]);
		if (maxBrFrameCount < convFilters[br].fileFrameCount) {
			maxBrFrameCount = convFilters[br].fileFrameCount;
		}
	}
	delete[] initWorks;
	WaitForThreadpoolWorkCallbacks(loHiPassWork, false);
	CloseThreadpoolWork(loHiPassWork);
	WaitForThreadpoolWorkCallbacks(createBuffWork, false);
	CloseThreadpoolWork(createBuffWork);
	status = processing;
}

std::vector <std::wstring> BRIRFilter::initialize(float sampleRate, unsigned int maxFrameCount, std::vector <std::wstring> channelNames) {
	std::vector <std::wstring> outputchannel;
	outputchannel.push_back(L"L");
	outputchannel.push_back(L"R");
	inputChannelCount = min((int)channelNames.size(), 8);
	if (inputChannelCount == 8) {
		hasLFEChannel = true;
		lfeChannel = 3;
	}
	for (int sr = 0; sr != 4; ++sr) {
		if (sampleRates[sr] == sampleRate) {
			currentSampleRateIndex = sr;
		}
	}
	frameCount = maxFrameCount;
	if (currentSampleRateIndex != -1) {
		init();
	}
	LogF(L"init BRIRFilter %f %d %d", sampleRate, maxFrameCount, inputChannelCount);
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
		this->currentInput = input;
		fill(loPassBuffer, loPassBuffer + frameCount, 0.0);
		for (int ch = 0; ch != inputChannelCount; ++ch) {
			for (int f = 0; f != frameCount; ++f) {
				loPassBuffer[f] += input[ch][f];
				if (input[ch][f]) {
					inputHasData[ch] = true;
				}
			}
		}
		PTP_WORK loPass = CreateThreadpoolWork(toLoPass, this, NULL);
		SubmitThreadpoolWork(loPass);
		int direction = UDPReceiver::globalReceiver->getDirection();
		calculateChannelToBrirDistance(direction);
		copyInputData();
		int jobIndex = 0;
		for (int br = 0; br != brirSize; ++br) {
			if (brirNeedConv[br] > 0) {
				brirNeedConv[br]--;
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
					output[ear][f] += convBuffer[br][ear][f];
				}
			}
			CloseThreadpoolWork(works[jb]);
		}
		WaitForThreadpoolWorkCallbacks(loPass, false);
		for (int ear = 0; ear != 2; ++ear) {
			for (unsigned f = 0; f < frameCount; f++) {
				output[ear][f] += loPassBuffer[f];
			}
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