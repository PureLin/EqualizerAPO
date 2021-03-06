/*
	This file is part of EqualizerAPO, a system-wide equalizer.
	Copyright (C) 2012  Jonas Thedering

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

#include "stdafx.h"
#ifdef DEBUG
#include <stdlib.h>
#include <crtdbg.h>
#endif
#include <io.h>
#include <cstdio>
#define _USE_MATH_DEFINES
#include <cmath>
#include <string>
#include <sndfile.h>
#include <tclap/CmdLine.h>

#include "version.h"
#include "FilterEngine.h"
#include "helpers/LogHelper.h"
#include "helpers/StringHelper.h"
#include "helpers/PrecisionTimer.h"
#include "helpers/MemoryHelper.h"

using namespace std;

double mainTest(int argc, char** argv)
{
	try
	{
		stringstream versionStream;
		versionStream << MAJOR << "." << MINOR;
		if (REVISION != 0)
			versionStream << "." << REVISION;
		TCLAP::CmdLine cmd("Benchmark generates a linear sine sweep or reads from the given input file. "
			"It then filters the waveform using the Equalizer APO filter configuration "
			"and finally writes to the given file or into the user's temp directory.", ' ', versionStream.str());

		TCLAP::SwitchArg noPauseArg("", "nopause", "Do not wait for key press at the end", cmd);
		TCLAP::SwitchArg verboseArg("v", "verbose", "Print trace and error messages to console instead of logfile", cmd);
		TCLAP::ValueArg<string> guidArg("", "guid", "Endpoint GUID to use when parsing configuration (Default: <empty>)", false, "", "string", cmd);
		TCLAP::ValueArg<string> connectionnameArg("", "connectionname", "Connection name to use when parsing configuration (Default: File output)", false, "File output", "string", cmd);
		TCLAP::ValueArg<string> devicenameArg("", "devicename", "Device name to use when parsing configuration (Default: Benchmark)", false, "Benchmark", "string", cmd);
		TCLAP::ValueArg<unsigned> batchsizeArg("", "batchsize", "Number of frames processed in one batch (Default: 4410)", false, 4410, "integer", cmd);
		TCLAP::ValueArg<string> outputArg("o", "output", "File to write sound data to", false, "", "string", cmd);
		TCLAP::ValueArg<string> inputArg("i", "input", "File to load sound data from instead of generating sweep", false, "", "string", cmd);
		TCLAP::ValueArg<unsigned> rateArg("r", "rate", "Sample rate of generated sweep (Default: 44100)", false, 44100, "integer", cmd);
		TCLAP::ValueArg<float> toArg("t", "to", "End frequency of generated sweep in Hz (Default: 20000.0)", false, 20000.0f, "float", cmd);
		TCLAP::ValueArg<float> fromArg("f", "from", "Start frequency of generated sweep in Hz (Default: 0.1)", false, 1.0f, "float", cmd);
		TCLAP::ValueArg<float> lengthArg("l", "length", "Length of generated sweep in seconds (Default: 200.0)", false, 200.0f, "float", cmd);
		TCLAP::ValueArg<unsigned> channelArg("c", "channels", "Number of channels of generated sweep (Default: 2)", false, 2, "integer", cmd);

		cmd.parse(argc, argv);

		bool verbose = verboseArg.getValue();
		LogHelper::set(stderr, verbose, true, true);
#ifdef _DEBUG
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
		// _CrtSetBreakAlloc(3318);
#endif

		unsigned sampleRate;
		unsigned channelCount;
		unsigned channelMask;
		unsigned frameCount;
		float length;
		float* buf;

		if (REVISION == 0)
			printf("Benchmark %d.%d\n", MAJOR, MINOR);
		else
			printf("Benchmark %d.%d.%d\n", MAJOR, MINOR, REVISION);

		printf("Run \"%s -h\" to show usage info\n", argv[0]);
		printf("\n");

		string input = inputArg.getValue();
		if (input != "")
		{
			printf("Reading sound data from %s\n", input.c_str());

			PrecisionTimer timer;
			timer.start();

			SF_INFO info;
			SNDFILE* inFile = sf_open(input.c_str(), SFM_READ, &info);
			if (inFile == NULL)
			{
				fprintf(stderr, "%s", sf_strerror(inFile));
				return 1;
			}

			sampleRate = info.samplerate;
			channelCount = info.channels;
			channelMask = 0;
			frameCount = (unsigned)info.frames;
			length = float(frameCount) / sampleRate;

			buf = new float[frameCount * channelCount];

			printf("file frame count %d\n", frameCount);
			sf_count_t numRead = 0;
			sf_count_t numReadLast = 0;
			while (numRead < frameCount)
			{
				numRead += sf_readf_float(inFile, buf + numRead * channelCount, frameCount - numRead);
				printf("read %lld frame\n", numRead);
				if (numRead == numReadLast) {
					printf("warning : file could read more ,remain %lld not read", frameCount - numRead);
					break;
				}
				numReadLast = numRead;
			}
			sf_close(inFile);
			inFile = NULL;

			double readTime = timer.stop();
			printf("Reading input file took %g seconds\n", readTime);
		}
		else
		{
			sampleRate = rateArg.getValue();
			channelMask = 0;
			channelCount = channelArg.getValue();
			float sweepFrom = fromArg.getValue();
			float sweepTo = toArg.getValue();
			float sweepDiff = sweepTo - sweepFrom;
			length = lengthArg.getValue();
			frameCount = (unsigned)(length * sampleRate);

			printf("No input file given, so generating linear sine sweep from %g to %g Hz over %g seconds\n", sweepFrom, sweepTo, length);

			PrecisionTimer timer;
			timer.start();

			buf = new float[frameCount * channelCount];
			for (unsigned i = 0; i < frameCount; i++)
			{
				double t = i * 1.0 / sampleRate;
				float s = (float)sin(((sweepFrom + sweepDiff * (t / length) / 2) * t) * 2 * M_PI);

				for (unsigned j = 0; j < channelCount; j++)
					buf[i * channelCount + j] = s;
			}

			double genTime = timer.stop();
			printf("Generating sweep took %g seconds\n", genTime);
		}

		unsigned batchsize = batchsizeArg.getValue();

		float* buf2 = new float[frameCount * channelCount];
		for (unsigned i = 0; i < frameCount * channelCount; i++)
			buf2[i] = 0.0f;

		PrecisionTimer timer;
		timer.start();
		{
			FilterEngine engine;
			wstring deviceName = StringHelper::toWString(devicenameArg.getValue(), CP_ACP);
			wstring connectionName = StringHelper::toWString(connectionnameArg.getValue(), CP_ACP);
			wstring deviceGuid = StringHelper::toWString(guidArg.getValue(), CP_ACP);
			engine.setDeviceInfo(false, true, deviceName, connectionName, deviceGuid, deviceName + L" " + connectionName + L" " + deviceGuid);
			engine.initialize((float)sampleRate, channelCount, channelCount, channelCount, channelMask, batchsize);

			double initTime = timer.stop();
			if (!verbose)
				printf("\nLoading configuration took %g ms\n", initTime * 1000.0);

			printf("\nProcessing %d frames from %d channel(s)\n", frameCount, channelCount);

			timer.start();

			for (unsigned i = 0; i < frameCount; i += batchsize)
			{
				engine.process(buf2 + i * channelCount, buf + i * channelCount, min(batchsize, frameCount - i));
			}

			double time = timer.stop();

			printf("%d samples processed in %f seconds\n", frameCount * channelCount, time);
			printf("This is equivalent to %.2f%% CPU load (one core) when processing in real time\n", 100.0f * time / length);

			unsigned clipCount = 0;
			float max = 0;
			for (unsigned i = 0; i < frameCount * channelCount; i++)
			{
				float f = fabs(buf2[i]);
				if (f > max)
					max = f;
				if (f > 1.0f)
					clipCount++;
			}

			printf("Max output level: %f (%f dB)", max, log10(max) * 20.0f);
			if (clipCount > 0)
				printf(" (%d samples clipped!)", clipCount);
			printf("\n");

			string output = outputArg.getValue();
			if (output == "")
			{
				char temp[255];
				GetTempPathA(sizeof(temp) / sizeof(wchar_t), temp);

				output = temp;
				output += "testout.wav";
			}

			printf("\nWriting output to %s\n", output.c_str());
			SF_INFO info;
			if (output.find(".flac") != string::npos) {
				info = { frameCount, (int)sampleRate, (int)channelCount, SF_FORMAT_FLAC | SF_FORMAT_PCM_16, 0 };
			}
			else {
				info = { frameCount, (int)sampleRate, (int)channelCount, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 0 };
			}

			SNDFILE* outFile = sf_open(output.c_str(), SFM_WRITE, &info);
			if (outFile == NULL)
			{
				fprintf(stderr, "%s", sf_strerror(outFile));
				return 1;
			}

			sf_count_t numWritten = 0;
			while (numWritten < frameCount)
				numWritten += sf_writef_float(outFile, buf2 + numWritten * channelCount, frameCount - numWritten);

			sf_close(outFile);
			outFile = NULL;

			delete[] buf;
			delete[] buf2;
			return time;
		}

	}
	catch (TCLAP::ArgException e)
	{
		printf("Error: %s for arg %s\n", e.error().c_str(), e.argId().c_str());
		return -1;
	}
}

void runBatchTest(unsigned sampleRate) {
	int c = 5;
	char** v = new char* [5];
	v[0] = "BatchTest.exe";
	v[1] = "-c";
	v[2] = "2";
	v[3] = "-r";
	char buffer[100];
	v[4] = itoa(sampleRate, buffer, 10);
	double channel2MaxTime = 0;
	for (int i = 0; i != 4; ++i) {
		channel2MaxTime = max(mainTest(c, v), channel2MaxTime);
	}
	v[1] = "-c";
	v[2] = "8";
	double channel8MaxTime = 0;
	for (int i = 0; i != 4; ++i) {
		channel8MaxTime = max(mainTest(c, v), channel8MaxTime);
	}
	printf("1ms could hanlde %f sample of 2 channel\n", 8820000 / 1000 / channel2MaxTime);
	printf("1ms could hanlde %f sample of 8 channel\n", 8820000 / 1000 / channel8MaxTime);
	printf("8channel use %f time of 2channel \n\n\n", channel8MaxTime / channel2MaxTime);

}

struct files {
	string input;
	string output;
	files(string a, string b) {
		input = a;
		output = b;
	}
};

void findfile(string path, string mode, vector<string>& fileNames)
{
	_finddata_t file;
	intptr_t HANDLE;
	string Onepath = path + mode;
	HANDLE = _findfirst(Onepath.c_str(), &file);
	if (HANDLE == -1L) {
		cout << "can not match the folder path" << endl;
		return;
	}
	do {
		//????????????????  
		if (file.attrib & _A_SUBDIR) {
			//??????????"."??????????".."??????????
			if ((strcmp(file.name, ".") != 0) && (strcmp(file.name, "..") != 0)) {
				string newPath = path + "\\" + file.name;
				findfile(newPath, mode, fileNames);
			}
		}
		else {
			cout << file.name << " " << endl;
			string fname(file.name);
			if (fname.find(".flac") != string::npos) {
				fileNames.push_back(path + "\\" + fname);
			}
		}
	} while (_findnext(HANDLE, &file) == 0);
	_findclose(HANDLE);
}

void processMusicAudioFiles(string directory) {
	vector<string> fileNames;
	findfile(directory, "\\*.*", fileNames);
	int argc = 5;
	char** argv = new char* [5];
	argv[0] = "123.exe";
	argv[1] = "--input";
	argv[3] = "--output";
	for (string name : fileNames) {
		argv[2] = (char*)name.c_str();
		argv[4] = (char*)name.c_str();
		mainTest(argc, argv);
	}
}

int main(int argc, char** argv) {
	stringstream versionStream;
	versionStream << MAJOR << "." << MINOR;
	if (REVISION != 0)
		versionStream << "." << REVISION;
	TCLAP::CmdLine cmd("Benchmark generates a linear sine sweep or reads from the given input file. "
		"It then filters the waveform using the Equalizer APO filter configuration "
		"and finally writes to the given file or into the user's temp directory.", ' ', versionStream.str());
	TCLAP::SwitchArg batchArg("b", "batchTest", "Will run the batch process and not accept any input", cmd);
	TCLAP::ValueArg<string> processArg("", "ProcessAudio", "Will run the batch process for audio in this directory", false, "", "string", cmd);
	TCLAP::SwitchArg noPauseArg("", "nopause", "Do not wait for key press at the end", cmd);
	TCLAP::SwitchArg verboseArg("v", "verbose", "Print trace and error messages to console instead of logfile", cmd);
	TCLAP::ValueArg<string> guidArg("", "guid", "Endpoint GUID to use when parsing configuration (Default: <empty>)", false, "", "string", cmd);
	TCLAP::ValueArg<string> connectionnameArg("", "connectionname", "Connection name to use when parsing configuration (Default: File output)", false, "File output", "string", cmd);
	TCLAP::ValueArg<string> devicenameArg("", "devicename", "Device name to use when parsing configuration (Default: Benchmark)", false, "Benchmark", "string", cmd);
	TCLAP::ValueArg<unsigned> batchsizeArg("", "batchsize", "Number of frames processed in one batch (Default: 4410)", false, 4410, "integer", cmd);
	TCLAP::ValueArg<string> outputArg("o", "output", "File to write sound data to", false, "", "string", cmd);
	TCLAP::ValueArg<string> inputArg("i", "input", "File to load sound data from instead of generating sweep", false, "", "string", cmd);
	TCLAP::ValueArg<unsigned> rateArg("r", "rate", "Sample rate of generated sweep (Default: 44100)", false, 44100, "integer", cmd);
	TCLAP::ValueArg<float> toArg("t", "to", "End frequency of generated sweep in Hz (Default: 20000.0)", false, 20000.0f, "float", cmd);
	TCLAP::ValueArg<float> fromArg("f", "from", "Start frequency of generated sweep in Hz (Default: 0.1)", false, 1.0f, "float", cmd);
	TCLAP::ValueArg<float> lengthArg("l", "length", "Length of generated sweep in seconds (Default: 200.0)", false, 200.0f, "float", cmd);
	TCLAP::ValueArg<unsigned> channelArg("c", "channels", "Number of channels of generated sweep (Default: 2)", false, 2, "integer", cmd);

	cmd.parse(argc, argv);

	if (batchArg.getValue()) {
		runBatchTest(rateArg.getValue());
		return 0;
	}
	if (processArg.getValue().length() != 0) {
		processMusicAudioFiles(processArg.getValue());
		return 0;
	}
	mainTest(argc, argv);
	system("pause");
	return 0;
}