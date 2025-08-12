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

#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include "json/json.h"
#include "MyLogger.hpp"
#include "MyMeta.h"

#include <string>
#include <iostream>
#include <fstream>
#include <locale>

class IConfigUpdateListener {
public:
    virtual ~IConfigUpdateListener() = default;
    virtual void onConfigUpdated(std::shared_ptr<MyMeta> &newMeta) = 0;
protected:
    bool m_isCfgListReg = false;
};

class ConfigSubscriber {
public:
    static ConfigSubscriber* getInstance() {
        static ConfigSubscriber instance;
        return &instance;
    }
    
    void start();
    void stop();
    bool subscribeOnline();
private:
    ConfigSubscriber();
    ~ConfigSubscriber();
    ConfigSubscriber(const ConfigSubscriber&) = delete;
    ConfigSubscriber& operator=(const ConfigSubscriber&) = delete;

    void subscribeWork();
private:
    bool m_isStop = false;
    std::thread m_subThd;
    std::atomic<bool> m_subWorkContinue;
};


class ConfigParser {
public:
    static ConfigParser* getInstance() {
        static ConfigParser instance;
        return &instance;
    }

    bool loadConfig(const std::string& filePath);

    bool loadServerConfig(const std::string& filePath);

    void registerListener(const std::string& section, IConfigUpdateListener* listener);
    void reloadConfig(const std::string& filePath);

    std::shared_ptr<MyMeta> getDetectMeta() const { return m_detectMeta; }
    std::shared_ptr<MyMeta> getAlertWindowMeta() const { return m_alertWindowMeta; }
    std::shared_ptr<MyMeta> getInferMeta() const { return m_inferMeta; }
    std::shared_ptr<MyMeta> getImageProcessMeta() const { return m_imageProcessMeta; }
    std::shared_ptr<MyMeta> getLogMeta() const { return m_logMeta; }
    std::shared_ptr<MyMeta> getUploadMeta() const { return m_uploadMeta; }
    std::shared_ptr<MyMeta> getTestMeta() const { return m_testMeta; }
    std::shared_ptr<MyMeta> getServerMeta() const { return m_serverMeta; }

private:
    ConfigParser();
    ~ConfigParser();
    ConfigParser(const ConfigParser&) = delete;
    ConfigParser& operator=(const ConfigParser&) = delete;

    // 从 Json::Value 填充 Meta 对象
    void populateMeta(std::shared_ptr<MyMeta> &meta, const Json::Value& jsonValue);
    void notifyListeners(const std::string& section, std::shared_ptr<MyMeta> &meta);
    void checkAndUpdateSection(const std::string& sectionName,
        const Json::Value& newJsonValue, std::shared_ptr<MyMeta>& meta);

    // 成员变量
    std::shared_ptr<MyMeta> m_detectMeta;
    std::shared_ptr<MyMeta> m_alertWindowMeta;
    std::shared_ptr<MyMeta> m_inferMeta;
    std::shared_ptr<MyMeta> m_imageProcessMeta;
    std::shared_ptr<MyMeta> m_logMeta;
    std::shared_ptr<MyMeta> m_uploadMeta;
    std::shared_ptr<MyMeta> m_testMeta;
    std::shared_ptr<MyMeta> m_serverMeta;

    // std::mutex m_mutex;
    Json::Value m_lastConfigRoot;
    std::unordered_map<std::string, IConfigUpdateListener*> m_listeners;
};

#endif // CONFIG_PARSER_H
