#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <ctime>
#include <functional>

namespace mvtk {
namespace utils {

enum class LogLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
};

class Logger {
public:
    using LogCallback = std::function<void(LogLevel level, const std::string& message)>;
    
    static Logger& GetInstance();

    void SetLogFile(const std::string& filepath);
    void SetLogLevel(LogLevel level);
    void SetLogCallback(LogCallback callback);
    
    void Debug(const std::string& message);
    void Info(const std::string& message);
    void Warn(const std::string& message);
    void Error(const std::string& message);
    void Fatal(const std::string& message);
    
    void LogOperation(const std::string& operation, const std::string& details = "");
    void LogData(const std::string& data_name, const std::string& data_value);
    void LogError(const std::string& error_type, const std::string& error_message);

private:
    Logger();
    ~Logger();
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    void WriteLog(LogLevel level, const std::string& message);
    std::string GetTimestamp();
    std::string LogLevelToString(LogLevel level);

    std::ofstream log_file_;
    std::mutex mutex_;
    LogLevel min_level_;
    bool file_open_;
    LogCallback callback_;
};

#define MV_LOG_DEBUG(msg) mvtk::utils::Logger::GetInstance().Debug(msg)
#define MV_LOG_INFO(msg) mvtk::utils::Logger::GetInstance().Info(msg)
#define MV_LOG_WARN(msg) mvtk::utils::Logger::GetInstance().Warn(msg)
#define MV_LOG_ERROR(msg) mvtk::utils::Logger::GetInstance().Error(msg)
#define MV_LOG_FATAL(msg) mvtk::utils::Logger::GetInstance().Fatal(msg)

#define MV_LOG_OPERATION(op, details) mvtk::utils::Logger::GetInstance().LogOperation(op, details)
#define MV_LOG_DATA(name, value) mvtk::utils::Logger::GetInstance().LogData(name, value)
#define MV_LOG_ERROR_DETAIL(type, msg) mvtk::utils::Logger::GetInstance().LogError(type, msg)

}
}