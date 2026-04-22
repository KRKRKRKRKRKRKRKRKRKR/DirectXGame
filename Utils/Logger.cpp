#include "Logger.h"
#include "StringUtils.h"
#include <Windows.h>
#include <filesystem>
#include <format>
#include <chrono>

std::ofstream Logger::logStream_;

void Logger::Initialize() {
    std::filesystem::create_directory("logs");

    // 現在時刻を取得
    auto now = std::chrono::system_clock::now();
    auto nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
    std::chrono::zoned_time localTime{ std::chrono::current_zone(), nowSeconds };

    // 年月日_時分秒でファイル名を生成
    std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
    std::string logFilePath = "logs/" + dateString + ".log";

    logStream_.open(logFilePath);

    Log("Created log file: " + logFilePath + "\n");
}

void Logger::Log(const std::string& message) {
    // 出力ウィンドウへ
    OutputDebugStringA(message.c_str());

    // ファイルへ
    if (logStream_.is_open()) {
        logStream_ << message;
        logStream_.flush();
    }
}

void Logger::Log(const std::wstring& message) {
	OutputDebugStringA(StringUtils::ConvertString(message).c_str());
}