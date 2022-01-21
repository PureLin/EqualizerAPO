#include "stdafx.h"
#include <sstream>

#include "helpers/LogHelper.h"
#include "helpers/StringHelper.h"
#include "parser/RegexFunctions.h"
#include "parser/RegistryFunctions.h"
#include "parser/StringOperators.h"
#include "parser/LogicalOperators.h"
#include "FilterEngine.h"
#include "ChannelCopyTestFilterFactory.h"
#include "ChannelCopyTestFilter.h"
using namespace std;
using namespace mup;

vector<IFilter*> ChannelCopyTestFilterFactory::createFilter(const wstring& configPath, wstring& command, wstring& parameters)
{
	ChannelCopyTestFilter * filter = NULL;
	if (command == L"ChannelTest")
	{
		LogF(L"create ChannelCopyTest filter");
		void* mem = MemoryHelper::alloc(sizeof(ChannelCopyTestFilter));

		filter = new(mem)ChannelCopyTestFilter();
	}
	if (filter == NULL)
		return vector<IFilter*>(0);
	return vector<IFilter*>(1, filter);
}