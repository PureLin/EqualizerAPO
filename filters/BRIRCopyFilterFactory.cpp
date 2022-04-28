#include "stdafx.h"
#include <sstream>
#include <locale>
#include <codecvt>
#include <string>

#include "helpers/LogHelper.h"
#include "helpers/StringHelper.h"
#include "parser/RegexFunctions.h"
#include "parser/RegistryFunctions.h"
#include "parser/StringOperators.h"
#include "parser/LogicalOperators.h"
#include "FilterEngine.h"
#include "BRIRCopyFilterFactory.h"
#include "BRIRMultiLayerCopyFilter.h"
#include "BRIR/BRIRFilter.h" 
#include "brir/lib_json/json.h"

using namespace std;
using namespace mup;

vector<IFilter*> BRIRCopyFilterFactory::createFilter(const wstring& configPath, wstring& command, wstring& parameters)
{
	IFilter* filter = NULL;
	if (command == L"BRIR")
	{
		void* mem = MemoryHelper::alloc(sizeof(BRIRFilter));
		wchar_t filePath[MAX_PATH];
		configPath._Copy_s(filePath, sizeof(filePath) / sizeof(wchar_t), MAX_PATH);
		if (configPath.size() < MAX_PATH)
			filePath[configPath.size()] = L'\0';
		else
			filePath[MAX_PATH - 1] = L'\0';
		PathRemoveFileSpecW(filePath);
		wstring absolutePath = filePath;
		absolutePath.append(L"\\brir\\");
		int channelToHeadDegree[8]{ -30, 30, 0, 0, -135, 135, -90, 90 };
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		string jsonStr = converter.to_bytes(parameters);
		string err;
		Json::Value json;
		if (!parse(&jsonStr, &json, &err)) {
			LogF(L"Failed to parse json input %s, erro: %s", jsonStr, err);
		}
		else {
			wstring name = converter.from_bytes(json["name"].asString());
			int port = 2055;
			if (json["port"].isInt()) {
				port = json["port"].asInt();
			}
			float bassPercent = json["bassVolume"].asFloat();
			for (int ch = 0; ch != 8; ++ch) {
				if (!json["directions"][ch].isInt()) {
					break;
				}
				channelToHeadDegree[ch] = json["directions"][ch].asInt();
			}
			LogF(L"create BRIR filter %d %ls %f", port, name, bassPercent);
			float loPassFreq[3]{ 200,250,250 };
			for (int l = 0; l != 3; ++l) {
				if (!json["loPassFreq"][l].isDouble()) {
					break;
				}
				loPassFreq[l] = json["loPassFreq"][l].asDouble();
			}
			filter = new(mem)BRIRFilter(port, name, absolutePath, channelToHeadDegree, bassPercent, loPassFreq);
		}
	}
	if (command == L"BRIRMulti")
	{
		LogF(L"create BRIRMulti filter");
		void* mem = MemoryHelper::alloc(sizeof(BRIRMultiLayerCopyFilter));

		wchar_t filePath[MAX_PATH];
		configPath._Copy_s(filePath, sizeof(filePath) / sizeof(wchar_t), MAX_PATH);
		if (configPath.size() < MAX_PATH)
			filePath[configPath.size()] = L'\0';
		else
			filePath[MAX_PATH - 1] = L'\0';
		PathRemoveFileSpecW(filePath);
		wstring absolutePath = filePath;
		absolutePath.append(L"\\brir\\");

		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		string jsonStr = converter.to_bytes(parameters);
		string err;
		Json::Value json;
		if (!parse(&jsonStr, &json, &err)) {
			LogF(L"Failed to parse json input %s, erro: %s", jsonStr, err);
		}
		else {
			int port = 3053;
			float bassPercent = json["bassVolume"].asFloat();
			if (json["port"].isInt()) {
				port = json["port"].asInt();
			}
			LogF(L"create Multi BRIR filter %d %f", port, bassPercent);
			float loPassFreq[3]{ 200,250,250 };
			for (int l = 0; l != 3; ++l) {
				if (!json["loPassFreq"][l].isDouble()) {
					break;
				}
				loPassFreq[l] = json["loPassFreq"][l].asDouble();
			}
			filter = new(mem)BRIRMultiLayerCopyFilter(port, bassPercent, absolutePath, loPassFreq);
		}
	}
	if (filter == NULL)
		return vector<IFilter*>(0);
	return vector<IFilter*>(1, filter);
}