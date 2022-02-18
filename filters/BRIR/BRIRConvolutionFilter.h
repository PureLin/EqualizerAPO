/*
    This file is part of EqualizerAPO, a system-wide equalizer.
    Copyright (C) 2015  Jonas Thedering

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

#include "IFilter.h"
#include "libHybridConv-0.1.1/libHybridConv_eapo.h"

#pragma AVRT_VTABLES_BEGIN
class BRIRConvolutionFilter
{
public:
    BRIRConvolutionFilter(std::wstring filename);
	virtual ~BRIRConvolutionFilter();
	void initialize();
	void process(float** output, float** input, unsigned frameCount);
	unsigned fileFrameCount = 0;
    volatile int sampleRate;
    volatile unsigned maxInputFrameCount;
private:
	void cleanup();

	std::wstring filename;
	HConvSingle* filters;
	unsigned channelCount;
};
#pragma AVRT_VTABLES_END
