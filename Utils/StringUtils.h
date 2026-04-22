#pragma once
#include <string>
class StringUtils {

public:

	StringUtils() = delete;

	static std::wstring ConvertString(const std::string& str);
	static std::string  ConvertString(const std::wstring& str);

};

