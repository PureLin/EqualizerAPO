#include "stdafx.h"
#include "ChannelCopyTestFilter.h"
#include "helpers/LogHelper.h"
#include <exception>
#include <atltime.h>

#pragma once
ChannelCopyTestFilter::ChannelCopyTestFilter(){

}

ChannelCopyTestFilter::~ChannelCopyTestFilter(){

}

std::vector<std::wstring> ChannelCopyTestFilter::initialize(float sampleRate, unsigned maxFrameCount, std::vector<std::wstring> channelNames){
	return channelNames;
}

void ChannelCopyTestFilter::process(float** output, float** input, unsigned frameCount) {
	if (receiver == NULL){
		return;
	}
	double* data = (double *)receiver->unionBuff.data;
	double vol[2];
	vol[0] = data[1];
	if (vol[0] <= 0.0001 || vol[0] > 1){
		vol[0] = 1;
	}
	vol[1] = 1 - vol[0];
	for (int ch = 0; ch != 2; ++ch){
		for (int f = 0; f != frameCount; ++f){
			output[ch][f] = input[ch][f] * vol[ch];
		}
	}
}
