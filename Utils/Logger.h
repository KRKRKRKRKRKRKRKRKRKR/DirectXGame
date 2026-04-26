#pragma once
#include <string>
#include <fstream>
class Logger{
public:

	Logger() = delete;

	static void Initialize();

	static void Log(const std::string& message);
	static void Log(const std::wstring& message);

private:
	static std::ofstream logStream_;
};

