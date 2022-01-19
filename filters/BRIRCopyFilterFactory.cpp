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
using namespace std;
using namespace mup;

vector<IFilter*> BRIRCopyFilterFactory::createFilter(const wstring& configPath, wstring& command, wstring& parameters)
{
	BRIRCopyFilter * filter = NULL;
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
		filter = new(mem)BRIRCopyFilter(0, absolutePath, parameters.find(L"linear") != parameters.npos);
	}
	if (filter == NULL)
		return vector<IFilter*>(0);
	return vector<IFilter*>(1, filter);
}