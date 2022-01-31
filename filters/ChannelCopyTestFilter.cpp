#include "stdafx.h"
#include "ChannelCopyTestFilter.h"
#include "helpers/LogHelper.h"
#include <exception>
#include <atltime.h>

#pragma once
ChannelCopyTestFilter::ChannelCopyTestFilter() {

}

ChannelCopyTestFilter::~ChannelCopyTestFilter() {

}

std::vector<std::wstring> ChannelCopyTestFilter::initialize(float sampleRate, unsigned maxFrameCount, std::vector<std::wstring> channelNames) {
	return channelNames;
}

void ChannelCopyTestFilter::process(float** output, float** input, unsigned frameCount) {
	if (UDPReceiver::globalReceiver == NULL) {
		return;
	}
	double* data = (double*)UDPReceiver::globalReceiver->unionBuff.data;
	double vol[2];
	vol[0] = data[0];
	vol[1] = data[1];
	if (vol[0] + vol[1] <= 0 || vol[0] + vol[1] > 2) {
		vol[0] = 1, vol[1] = 0;
	}
	for (int ch = 0; ch != 2; ++ch) {
		for (int f = 0; f != frameCount; ++f) {
			output[ch][f] = input[0][f] * vol[ch];
		}
	}
}
