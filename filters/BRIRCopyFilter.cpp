#include "stdafx.h"
#include "BRIRCopyFilter.h"
#include "helpers/LogHelper.h"
#include <exception>
#include <atltime.h>

using namespace std;

struct BrirJobInfo{
	int direction;
	int channel;
	bool useLinearPos;
	float **input;
	int frameCount;
};

static ConvolutionFilter* convFilters[12];
static int bufsize = 10240;
static float ** inbuffer = 0;
//8 input->2 simulate brir->2 ear for each brir
static float * outbuffer[8][2][2];
static UDPReceiver* receiver;
static bool init = false;
static std::vector <std::wstring> convChannel;
static int SourceToHeadDiff[8]{-30, 30, 0, 0, -135, 135 - 70, 70};
static int lastLeftBrir[8];
static BrirJobInfo brirJobs[8];
static PTP_POOL pool = NULL;
static PTP_WORK works[8];

void initBuff(){
	LogFStatic(L"Create buff !");
	for (int ch = 0; ch != 8; ++ch){
		for (int j = 0; j != 2; ++j){
			for (int k = 0; k != 2; ++k){
				if (outbuffer[ch][j][k] != 0){
					delete[] outbuffer;
				}
				outbuffer[ch][j][k] = new float[bufsize];
			}
		}
	}
}


void clearBuff(int frameCount){
	TraceFStatic(L"Clear buff !");
	for (int ch = 0; ch != 8; ++ch){
		for (int j = 0; j != 2; ++j){
			for (int k = 0; k != 2; ++k){
				for (int f = 0; f != frameCount; ++f){
					outbuffer[ch][j][k][f] = 0;
				}
			}
		}
	}
}


DWORD WINAPI ThreadFunc(LPVOID p)
{
	LogFStatic(L"ThreadFunc start !");
	UDPReceiver* receiver = (UDPReceiver*)p;
	receiver->doReceive();
	return 0;
}

ConvolutionFilter* createConvFilter(wstring& filePath){
	void* mem = MemoryHelper::alloc(sizeof(ConvolutionFilter));
	return new(mem)ConvolutionFilter(filePath);
}

int overflow(int pos){
	if (pos < 0){
		pos += 360;
	}
	if (pos >= 360){
		pos -= 360;
	}
	return pos;
}

BRIRCopyFilter::BRIRCopyFilter(int port, wstring path, bool useLinear){
	if (!init){
		try{
			if (port != 0){
				this->port = port;
			}
			this->useLinearPos = useLinear;
			LogF(L"Use LinearPos %d", useLinear);
			LogF(L"Create UDPReceiver");
			void* mem = MemoryHelper::alloc(sizeof(UDPReceiver));
			receiver = new(mem)UDPReceiver(this->port);
			hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadFunc, receiver, 0, &threadId);
			LogF(L"Create UDPReceiver Finished");
			for (int ch = 0; ch != 12; ++ch){
				wstring configPath(path);
				configPath.append(to_wstring(ch)).append(L".wav");
				LogF(L"Create ConvFilter %d %ls", ch, configPath.c_str());
				convFilters[ch] = createConvFilter(configPath);
			}
			LogF(L"create buffsize %d", bufsize);
			initBuff();
			convChannel.push_back(L"ToL");
			convChannel.push_back(L"ToR");
			LogF(L"create threadpool");
			pool = CreateThreadpool(NULL);
			SetThreadpoolThreadMaximum(pool, 16);
			SetThreadpoolThreadMinimum(pool, 8);
			init = true;
		}
		catch (exception e){
			LogF(L"Create UDPReceiver Failed");
			if (receiver != 0){
				receiver->open = false;
				CloseHandle(hThread);
			}
		}
	}
}


BRIRCopyFilter::~BRIRCopyFilter(){
}

std::vector <std::wstring> BRIRCopyFilter::initialize(float sampleRate, unsigned int maxFrameCount, std::vector <std::wstring> channelNames) {

	inputChannels = min(channelNames.size(), 8);
	for (std::wstring br : channelNames){
		TraceF(L"in channel: %br", br);
	}
	std::vector <std::wstring> outputchannel;
	outputchannel.push_back(L"HRIRL");
	outputchannel.push_back(L"HRIRR");
	if (!init){
		LogF(L"not init !");
	}
	else{
		if (maxFrameCount > bufsize){
			LogF(L"bufsize not enough, reacreate buff %d", maxFrameCount);
			bufsize = maxFrameCount;
			initBuff();
		}
		clearBuff(maxFrameCount);
		for (int ch = 0; ch != 12; ++ch){
			TraceF(L"init convFilter %d", ch);
			convFilters[ch]->initialize(sampleRate, maxFrameCount, convChannel);
		}
	}
	return outputchannel;
}


void processOneChannelBrir(BrirJobInfo* job){
	int sourceMappingDirection = overflow(job->direction + SourceToHeadDiff[job->channel]);
	//for each source channel need 2 brir to sim the actual pos
	//the volume will depend on direction,colser bigger
	int brir[2];
	double volume[2];
	brir[0] = sourceMappingDirection / 30;
	brir[1] = (brir[0] + 1) % 12;
	if (job->useLinearPos){
		volume[1] = double(sourceMappingDirection % 30) / 30;
	}
	else{
		volume[1] = pow((double(sourceMappingDirection % 30) - 15) / 15, 3) / 2 + 0.5;
	}
	volume[0] = 1 - volume[1];
	if (lastLeftBrir[job->channel] != brir[0]){
		TraceFStatic(L"Channel %d, direction %f, frameCount %d", job->channel, job->direction, job->frameCount);
		TraceFStatic(L"brir %d %d, volume %f %f", brir[0], brir[1], volume[0], volume[1]);
		lastLeftBrir[job->channel] = brir[0];
	}
	float** inputBuffer = new float*[2];
	for (int br = 0; br != 2; ++br){
		//copy the source channel data to one brir
		inputBuffer[1] = inputBuffer[0] = job->input[job->channel];
		// then conv with the brir
		convFilters[brir[br]]->process(outbuffer[job->channel][br], inputBuffer, job->frameCount);
		//copy result to the leftear and rightear channel
		for (int ear = 0; ear != 2; ++ear){
			for (unsigned f = 0; f < job->frameCount; f++){
				outbuffer[job->channel][br][ear][f] *= volume[br];
			}
		}
	}
	delete[] inputBuffer;
}

VOID
CALLBACK
BRIRWorkCallBack(
PTP_CALLBACK_INSTANCE Instance,
PVOID                 Parameter,
PTP_WORK              Work
)
{
	processOneChannelBrir((BrirJobInfo *)Parameter);
	return;
}

void BRIRCopyFilter::process(float **output, float **input, unsigned int frameCount) {
	if (!init){
		LogF(L"BRIR not init !");
		return;
	}
	try{
		double* data = (double *)receiver->unionBuff.data;
		double yaw = data[3];
		int direction = 180 - yaw / 5;
		//avoid error yaw input
		if (direction < 0 || direction>360){
			direction = 180;
		}
		for (int ch = 0; ch != inputChannels; ++ch){
			TraceFStatic(L"create Channel %d work", ch);
			brirJobs[ch].direction = direction;
			brirJobs[ch].channel = ch;
			brirJobs[ch].useLinearPos = useLinearPos;
			brirJobs[ch].input = input;
			brirJobs[ch].frameCount = frameCount;
			works[ch] = CreateThreadpoolWork(BRIRWorkCallBack, &brirJobs[ch], NULL);
			SubmitThreadpoolWork(works[ch]);
			//processOneChannelBrir(&brirJobs[ch]);
		}
		for (int ear = 0; ear != 2; ++ear){
			for (unsigned f = 0; f < frameCount; f++){
				output[ear][f] = 0;
			}
		}
		for (int ch = 0; ch != inputChannels; ++ch){
			TraceFStatic(L"wait for Channel %d work finish", ch);
			WaitForThreadpoolWorkCallbacks(works[ch], false);
			for (int br = 0; br != 2; ++br){
				for (int ear = 0; ear != 2; ++ear){
					for (unsigned f = 0; f < frameCount; f++){
						output[ear][f] += outbuffer[ch][br][ear][f];
					}
				}
			}
		}
	}
	catch (exception e){
		LogF(L"Exception Occurs");
		LogF(L"%br", e.what());
	}
	catch (...){
		LogF(L"Exception Occurs");
	}
}