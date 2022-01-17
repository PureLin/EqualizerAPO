#pragma once

#include <string>

#include "IFilterFactory.h"
#include "IFilter.h"

class BRIRCopyFilterFactory : public IFilterFactory
{
public:
	std::vector<IFilter*> createFilter(const std::wstring& configPath, std::wstring& command, std::wstring& parameters) override;
};
