#include "utils/logger.h"
#include <sstream>
#include <iomanip>
#include <iostream>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <unistd.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

namespace mvtk {
namespace utils {

Logger::Logger() : min_level_(LogLevel::LOG_DEBUG), file_open_(false) {
    // Ensure log/ directory exists in the current working directory
    MKDIR("log");

    std::time_t now = std::time(nullptr);
    std::tm* local_time = std::localtime(&now);
    std::ostringstream oss;
    oss << "cameratoolkit_"
        << std::setw(4) << std::setfill('0') << local_time->tm_year + 1900
        << std::setw(2) << std::setfill('0') << local_time->tm_mon + 1
        << std::setw(2) << std::setfill('0') << local_time->tm_mday
        << "_"
        << std::setw(2) << std::setfill('0') << local_time->tm_hour
        << std::setw(2) << std::setfill('0') << local_time->tm_min
        << std::setw(2) << std::setfill('0') << local_time->tm_sec
        << ".log";
    SetLogFile(std::string("log/") + oss.str());
}

Logger::~Logger() {
    if (file_open_) {
        log_file_.close();
    }
}

Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

void Logger::SetLogFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_open_) {
        log_file_.close();
    }
    log_file_.open(filepath, std::ios::out | std::ios::app);
    file_open_ = log_file_.is_open();
    if (file_open_) {
        log_file_ << "========== Log Started ==========" << std::endl;
        log_file_ << "Timestamp: " << GetTimestamp() << std::endl;
        log_file_ << "==================================" << std::endl;
    }
}

void Logger::SetLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    min_level_ = level;
}

void Logger::SetLogCallback(LogCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = callback;
}

void Logger::Debug(const std::string& message) {
    WriteLog(LogLevel::LOG_DEBUG, message);
}

void Logger::Info(const std::string& message) {
    WriteLog(LogLevel::LOG_INFO, message);
}

void Logger::Warn(const std::string& message) {
    WriteLog(LogLevel::LOG_WARN, message);
}

void Logger::Error(const std::string& message) {
    WriteLog(LogLevel::LOG_ERROR, message);
}

void Logger::Fatal(const std::string& message) {
    WriteLog(LogLevel::LOG_FATAL, message);
}

void Logger::LogOperation(const std::string& operation, const std::string& details) {
    std::ostringstream oss;
    oss << "[OPERATION] " << operation;
    if (!details.empty()) {
        oss << " - " << details;
    }
    Info(oss.str());
}

void Logger::LogData(const std::string& data_name, const std::string& data_value) {
    std::ostringstream oss;
    oss << "[DATA] " << data_name << " = " << data_value;
    Debug(oss.str());
}

void Logger::LogError(const std::string& error_type, const std::string& error_message) {
    std::ostringstream oss;
    oss << "[ERROR-" << error_type << "] " << error_message;
    Error(oss.str());
}

void Logger::WriteLog(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (level < min_level_) {
        return;
    }
    
    std::ostringstream oss;
    oss << "[" << GetTimestamp() << "] ";
    oss << "[" << LogLevelToString(level) << "] ";
    oss << message << std::endl;
    
    std::string log_str = oss.str();
    
    if (file_open_) {
        log_file_ << log_str;
        log_file_.flush();
    }
    
    std::cout << log_str;
    
    if (callback_) {
        callback_(level, log_str);
    }
}

std::string Logger::GetTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm* local_time = std::localtime(&now);
    
    std::ostringstream oss;
    oss << std::setw(4) << std::setfill('0') << local_time->tm_year + 1900 << "-"
        << std::setw(2) << std::setfill('0') << local_time->tm_mon + 1 << "-"
        << std::setw(2) << std::setfill('0') << local_time->tm_mday << " "
        << std::setw(2) << std::setfill('0') << local_time->tm_hour << ":"
        << std::setw(2) << std::setfill('0') << local_time->tm_min << ":"
        << std::setw(2) << std::setfill('0') << local_time->tm_sec;
    
    return oss.str();
}

std::string Logger::LogLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::LOG_DEBUG: return "DEBUG";
        case LogLevel::LOG_INFO:  return "INFO";
        case LogLevel::LOG_WARN:  return "WARN";
        case LogLevel::LOG_ERROR: return "ERROR";
        case LogLevel::LOG_FATAL: return "FATAL";
        default:                  return "UNKNOWN";
    }
}

}
}