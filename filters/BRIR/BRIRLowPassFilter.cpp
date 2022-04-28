#include "stdafx.h"
#include "BRIRLowPassFilter.h"

#pragma AVRT_CODE_BEGIN
BRIRLowPassFilter::BRIRLowPassFilter(float freq[3], int sampleRate, int maxFrameCount)
{
	this->frameCount = frameCount;
	biquads= (BiQuad*)MemoryHelper::alloc(3 * sizeof(BiQuad));
	float q = 0.70710678118654757;
	for (int bq = 0; bq < 3; ++bq)
	{
		new(biquads + bq)BiQuad(BiQuad::LOW_PASS, 0, freq[bq], sampleRate, q, false);
	}
}

BRIRLowPassFilter::~BRIRLowPassFilter()
{
	MemoryHelper::free(biquads);
}

void BRIRLowPassFilter::process(float* buffer, int frameCount)
{
	for (int p = 0; p != 3; ++p) {
		for (int f = 0; f != frameCount; ++f) {
			buffer[f] = biquads[p].process(buffer[f]);
		}
	}
}
#pragma AVRT_CODE_END