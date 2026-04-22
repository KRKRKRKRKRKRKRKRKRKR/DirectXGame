#include "Logger.h"
#include "StringUtils.h"
#include <Windows.h>

void Logger::Log(const std::string& message) {
	OutputDebugStringW(StringUtils::ConvertString(message).c_str());
}

void Logger::Log(const std::wstring& message) {
	OutputDebugStringA(StringUtils::ConvertString(message).c_str());
}