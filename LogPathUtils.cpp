
#include "LogPathUtils.h"
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>

std::string LogPathUtils::getLogDirectory() {
#ifdef __APPLE__
    const char* homeDir = getenv("HOME");
    if (homeDir) {
        return std::string(homeDir) + "/Library/Logs/PADetect";
    }
    return "/tmp/PADetect/logs"; // 备用路径
#else
    return "./log";
#endif
}

std::string LogPathUtils::getLogFilePath(const std::string& filename) {
    return getLogDirectory() + "/" + filename;
}

bool LogPathUtils::createLogDirectory() {
    std::string logDir = getLogDirectory();
    
    struct stat st;
    if (stat(logDir.c_str(), &st) == 0) {
        // 目录已存在
        return true;
    }
    
    // 创建目录（包括父目录）
    std::string command = "mkdir -p \"" + logDir + "\"";
    int result = system(command.c_str());
    
    return result == 0;
}

std::string LogPathUtils::expandPath(const std::string& path) {
    if (path.empty() || path[0] != '~') {
        return path;
    }
    
    const char* homeDir = getenv("HOME");
    if (!homeDir) {
        return path; // 无法扩展，返回原路径
    }
    
    if (path.length() == 1 || path[1] == '/') {
        return std::string(homeDir) + path.substr(1);
    }
    
    return path; // 不是简单的 ~ 或 ~/，返回原路径
}