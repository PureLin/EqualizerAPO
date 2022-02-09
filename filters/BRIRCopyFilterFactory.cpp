#include "stdafx.h"
#include <sstream>

#include "helpers/LogHelper.h"
#include "helpers/StringHelper.h"
#include "parser/RegexFunctions.h"
#include "parser/RegistryFunctions.h"
#include "parser/StringOperators.h"
#include "parser/LogicalOperators.h"
#include "FilterEngine.h"
#include "BRIRCopyFilterFactory.h"
#include "BRIRCopyFilter.h"
#include "BRIRMultiLayerCopyFilter.h"
#include "BRIR/BRIRFilter.h" 
using namespace std;
using namespace mup;

vector<IFilter*> BRIRCopyFilterFactory::createFilter(const wstring& configPath, wstring& command, wstring& parameters)
{
	IFilter* filter = NULL;
	if (command == L"BRIRCopy")
	{
		LogF(L"create BRIR copy filter");
		void* mem = MemoryHelper::alloc(sizeof(BRIRCopyFilter));

		wchar_t filePath[MAX_PATH];
		configPath._Copy_s(filePath, sizeof(filePath) / sizeof(wchar_t), MAX_PATH);
		if (configPath.size() < MAX_PATH)
			filePath[configPath.size()] = L'\0';
		else
			filePath[MAX_PATH - 1] = L'\0';
		PathRemoveFileSpecW(filePath);
		wstring absolutePath = filePath;
		absolutePath.append(L"\\brir\\");
		filter = new(mem)BRIRCopyFilter(2055, absolutePath);
	}
	if (command == L"BRIR")
	{
		LogF(L"create BRIR filter");
		void* mem = MemoryHelper::alloc(sizeof(BRIRFilter));

		wchar_t filePath[MAX_PATH];
		configPath._Copy_s(filePath, sizeof(filePath) / sizeof(wchar_t), MAX_PATH);
		if (configPath.size() < MAX_PATH)
			filePath[configPath.size()] = L'\0';
		else
			filePath[MAX_PATH - 1] = L'\0';
		PathRemoveFileSpecW(filePath);
		wstring absolutePath = filePath;
		wstring name;
		wstringstream stream(parameters);
		stream >> name;
		absolutePath.append(L"\\brir\\");
		int sourceToHeadDiff[8]{ -30, 30, 0, 0, -135, 135, -90, 90 };
		int index = 0;
		int diff;
		while (stream >> diff) {
			sourceToHeadDiff[index++] = diff;
		}
		filter = new(mem)BRIRFilter(2055, name, absolutePath, sourceToHeadDiff);
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
		filter = new(mem)BRIRMultiLayerCopyFilter(2055, absolutePath);
	}
	if (filter == NULL)
		return vector<IFilter*>(0);
	return vector<IFilter*>(1, filter);
}