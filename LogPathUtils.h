#ifndef LOG_PATH_UTILS_H
#define LOG_PATH_UTILS_H

#include <string>

class LogPathUtils {
public:
    // 获取正确的日志目录路径
    static std::string getLogDirectory();
    
    // 获取完整的日志文件路径
    static std::string getLogFilePath(const std::string& filename = "app.log");
    
    // 创建日志目录（如果不存在）
    static bool createLogDirectory();
    
    // 扩展路径中的波浪号（~）为用户主目录
    static std::string expandPath(const std::string& path);
    
private:
    LogPathUtils() = default;
};

#endif // LOG_PATH_UTILS_H