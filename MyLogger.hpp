/*
 * Copyright 2024 Sheng Han
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MY_LOGGER_H
#define MY_LOGGER_H

#include "PlatformCompat.h"
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LOGGER_TRACE
#include <spdlog/common.h>
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <mutex>
#include <cstdarg>
#include <ctime>
#include <iomanip>
#if PLATFORM_WINDOWS
#define NOMINMAX
#include <windows.h>
#endif
#include <codecvt>
#if !PLATFORM_WINDOWS
#include <unistd.h>  // for getpid()
#endif
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include "LogPathUtils.h"


constexpr const char* SPD_LOG_PATH = { "./log/app.log" };
constexpr const uint64_t SPD_MAX_LOG_SIZE = 100 * 1024 * 1024;
constexpr const uint64_t SPD_MAX_LOG_FILES = 10;
constexpr const char* SPD_LOG_NAME = { "app" };

class MySpdlog {
public:
    static MySpdlog* getInstance() {
        static MySpdlog instance;
        return &instance;
    }

    bool init() {
        std::lock_guard<std::mutex> lock(m_mutex);
        try {
            if (!m_logger) {
                m_logger = spdlog::rotating_logger_mt(SPD_LOG_NAME, m_logPath,
                                                      m_logSize, m_logFiles);
                configureLogger();
            }
            return true;
        }
        catch (const spdlog::spdlog_ex& ex) {
            std::cerr << "Spdlog init failed: " << ex.what() << std::endl;
            return false;
        }
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_logger) {
            spdlog::drop(SPD_LOG_NAME);
            m_logger->flush();
        }
        spdlog::shutdown();
    }

    std::shared_ptr<spdlog::logger> getLogger() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_logger) {
            throw std::runtime_error("Logger not initialized");
        }
        return m_logger;
    }

    void setLogEnabled(bool enabled) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_logEnabled = enabled;
        if (m_logger) {
            m_logger->set_level(enabled ? m_logLevel : spdlog::level::off);
        }
    }

    void setLogLevel(spdlog::level::level_enum level) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_logLevel = level;
        if (m_logger && m_logEnabled) {
            m_logger->set_level(level);
        }
    }

    void setLogPath(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_logPath = path;
    }

private:
    std::mutex m_mutex;
    std::string m_logPath{ SPD_LOG_PATH };
    uint32_t m_logSize{ SPD_MAX_LOG_SIZE };
    uint32_t m_logFiles{ SPD_MAX_LOG_FILES };
    std::shared_ptr<spdlog::logger> m_logger;
    bool m_logEnabled{ true };
    spdlog::level::level_enum m_logLevel{ spdlog::level::trace };

    MySpdlog() {
        // 初始化正确的日志路径
        LogPathUtils::createLogDirectory();
        m_logPath = LogPathUtils::getLogFilePath("app.log");
    }
    ~MySpdlog() = default;
    MySpdlog(const MySpdlog&) = delete;
    MySpdlog& operator=(const MySpdlog&) = delete;

    void configureLogger() {
        m_logger->set_pattern(
            "%Y-%m-%d %H:%M:%S.%e  %7P %7t %9l (%15s, %10!, %5#) %v");
        m_logger->set_level(m_logEnabled ? m_logLevel : spdlog::level::off);
        spdlog::flush_on(spdlog::level::err);
        spdlog::flush_every(std::chrono::seconds(10));
        spdlog::set_default_logger(m_logger);
    }
};

#define MY_SPDLOG_TRACE(...)                                                   \
  SPDLOG_LOGGER_TRACE(MySpdlog::getInstance()->getLogger(), __VA_ARGS__)
#define MY_SPDLOG_DEBUG(...)                                                   \
  SPDLOG_LOGGER_DEBUG(MySpdlog::getInstance()->getLogger(), __VA_ARGS__)
#define MY_SPDLOG_INFO(...)                                                    \
  SPDLOG_LOGGER_INFO(MySpdlog::getInstance()->getLogger(), __VA_ARGS__)
#define MY_SPDLOG_WARN(...)                                                    \
  SPDLOG_LOGGER_WARN(MySpdlog::getInstance()->getLogger(), __VA_ARGS__)
#define MY_SPDLOG_ERROR(...)                                                   \
  SPDLOG_LOGGER_ERROR(MySpdlog::getInstance()->getLogger(), __VA_ARGS__)
#define MY_SPDLOG_CRITICAL(...)                                                \
  SPDLOG_LOGGER_CRITICAL(MySpdlog::getInstance()->getLogger(), __VA_ARGS__)

#endif // MY_LOGGER_H
