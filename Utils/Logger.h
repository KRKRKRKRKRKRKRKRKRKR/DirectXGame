#pragma once
#include <string>

class Logger{
public:

	Logger() = delete;

	static void Log(const std::string& message);
	static void Log(const std::wstring& message);

};

