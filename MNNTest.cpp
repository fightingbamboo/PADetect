
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <fstream>
#include <cstdio>
#include "MyLogger.hpp"

void redirectStreams(const std::string& logFile) {
    std::ofstream* outFile = new std::ofstream(logFile, std::ios::app);
    if (outFile->is_open()) {
        std::cout.rdbuf(outFile->rdbuf());
        std::cerr.rdbuf(outFile->rdbuf());
    }
    
    if (!MySpdlog::getInstance()->init()) {
        MY_SPDLOG_ERROR("Failed to initialize logger");
    }
}

int main(int argc, char* argv[]) {
    // 重定向输出到日志文件
    std::string logFile = "mnn_test.log";
    redirectStreams(logFile);
    
    MY_SPDLOG_INFO("MNN Test application started");
    MY_SPDLOG_INFO("Note: OpenCV and MNN dependencies not available in current build");
    
    // 模拟一些基本的测试逻辑
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 10; ++i) {
        MY_SPDLOG_DEBUG("Test iteration: {}", i);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    MY_SPDLOG_INFO("Test completed in {} ms", duration.count());
    
    return 0;
}

