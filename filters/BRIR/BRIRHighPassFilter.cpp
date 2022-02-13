#include "stdafx.h"
#include "BRIRHighPassFilter.h"

#pragma AVRT_CODE_BEGIN
BRIRHighPassFilter::BRIRHighPassFilter(int sampleRate, int maxFrameCount, int channelCount, int lfeChannel)
{
	this->channelCount = channelCount;
	this->frameCount = maxFrameCount;
	this->lfeChannel = lfeChannel;
	biquads[0] = (BiQuad*)MemoryHelper::alloc(channelCount * sizeof(BiQuad));
	biquads[1] = (BiQuad*)MemoryHelper::alloc(channelCount * sizeof(BiQuad));
	biquads[2] = (BiQuad*)MemoryHelper::alloc(channelCount * sizeof(BiQuad));
	float q = 0.70710678118654757;
	float hiPass = 95;
	for (int ch = 0; ch < channelCount; ++ch)
	{
		new(biquads[0] + ch)BiQuad(BiQuad::HIGH_PASS, 0, hiPass, sampleRate, q, false);
		new(biquads[1] + ch)BiQuad(BiQuad::HIGH_PASS, 0, hiPass, sampleRate, q, false);
		new(biquads[2] + ch)BiQuad(BiQuad::HIGH_PASS, 0, hiPass, sampleRate, q, false);
	}
	for (int ch = 0; ch != channelCount; ++ch) {
		hiPassWork[ch].filter = this;
		hiPassWork[ch].channel = ch;
	}
}

BRIRHighPassFilter::~BRIRHighPassFilter()
{
	MemoryHelper::free(biquads[0]);
	MemoryHelper::free(biquads[1]);
	MemoryHelper::free(biquads[2]);
}

void BRIRHighPassFilter::doHiPass(int ch) {
	for (int p = 0; p != 3; ++p) {
		for (int f = 0; f != frameCount; ++f) {
			currentBuffer[ch][f] = biquads[p][ch].process(currentBuffer[ch][f]);
		}
	}
}

namespace BRIRHIPASS {
	VOID CALLBACK toHiPass(PTP_CALLBACK_INSTANCE Instance, PVOID  Parameter, PTP_WORK Work) {
		BRIRHighPassFilter::HiPassWork* work = (BRIRHighPassFilter::HiPassWork*)Parameter;
		(work->filter)->doHiPass(work->channel);
	}

}

using namespace BRIRHIPASS;

void BRIRHighPassFilter::process(float** buffer, unsigned frameCount)
{
	currentBuffer = buffer;
	this->frameCount = frameCount;
	for (int ch = 0; ch != channelCount - 1; ++ch) {
		if (ch == lfeChannel) {
			continue;
		}
		works[ch] = CreateThreadpoolWork(toHiPass, hiPassWork + ch, NULL);
		SubmitThreadpoolWork(works[ch]);
	}
	if (channelCount - 1 != lfeChannel) {
		doHiPass(channelCount - 1);
	}
	for (int ch = 0; ch != channelCount - 1; ++ch) {
		if (ch == lfeChannel) {
			continue;
		}
		WaitForThreadpoolWorkCallbacks(works[ch], false);
		CloseThreadpoolWork(works[ch]);
	}
}
#pragma AVRT_CODE_END