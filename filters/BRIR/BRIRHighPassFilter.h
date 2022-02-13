/*
	This file is part of EqualizerAPO, a system-wide equalizer.
	Copyright (C) 2014  Jonas Thedering

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once
#include "stdafx.h"
#include "IFilter.h"
#include "../BiQuad.h"

#pragma AVRT_VTABLES_BEGIN
class BRIRHighPassFilter
{
public:
	BRIRHighPassFilter(int sampleRate, int maxFrameCount, int channelCount, int lfeChannel);
	virtual~BRIRHighPassFilter();
	void process(float** buffer, unsigned frameCount);
	void BRIRHighPassFilter::doHiPass(int channel);

	struct HiPassWork {
		BRIRHighPassFilter* filter;
		int channel;
	};
private:
	int frameCount;
	int channelCount;
	int lfeChannel;
	float** currentBuffer;
	BiQuad* biquads[3];

	HiPassWork hiPassWork[8];
	PTP_WORK works[14];
};
#pragma AVRT_VTABLES_END
