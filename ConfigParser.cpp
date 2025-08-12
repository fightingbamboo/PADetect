#include "ConfigParser.h"
#include "HttpClient.h"
#include "CommonUtils.h"
#include "MyLogger.hpp"
#include "MyMeta.h"
#include <memory>

#define ONLINE_CONFIG_UPDATE 0

ConfigSubscriber::ConfigSubscriber() {
}

ConfigSubscriber::~ConfigSubscriber() {
    if (!m_isStop) { stop(); }
}

void ConfigSubscriber::start() {
    m_subWorkContinue.store(true);
    m_subThd = std::thread(&ConfigSubscriber::subscribeWork, this);
}

void ConfigSubscriber::stop() {
    m_subWorkContinue.store(false);
    if (m_subThd.joinable()) { m_subThd.join(); }
    m_isStop = true;
}

bool ConfigSubscriber::subscribeOnline() {
    HttpClient* hc = HttpClient::getInstance();
    if (!hc->requestConfig()) {
        return false;
    }
    CommonUtils::FileHelper::writeStrToFile("config.json", hc->getConfig());
    return true;
}

constexpr uint64_t SUB_SLEEP_TIME = 5 * 1000;
void ConfigSubscriber::subscribeWork()
{
    MY_SPDLOG_INFO(">>>");
    while (m_subWorkContinue.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(SUB_SLEEP_TIME)); // 5s
        ConfigParser *cfgParser = ConfigParser::getInstance();
#if ONLINE_CONFIG_UPDATE
        subscribeOnline();
#endif
        cfgParser->reloadConfig("config.json");
    }
    MY_SPDLOG_INFO("<<<");
}



/**************************************************************************************/
ConfigParser::ConfigParser() {
    m_detectMeta = std::make_shared<MyMeta>();
    m_alertWindowMeta = std::make_shared<MyMeta>();
    m_inferMeta = std::make_shared<MyMeta>();
    m_imageProcessMeta = std::make_shared<MyMeta>();
    m_logMeta = std::make_shared<MyMeta>();
    m_uploadMeta = std::make_shared<MyMeta>();
    m_testMeta = std::make_shared<MyMeta>();
    m_serverMeta = std::make_shared<MyMeta>();

    m_lastConfigRoot = Json::Value(Json::objectValue);
}

ConfigParser::~ConfigParser() {

}

bool ConfigParser::loadConfig(const std::string& filePath) {
    std::ifstream configFile(filePath);
    if (!configFile.is_open()) {
        throw std::runtime_error("Failed to open config file: " + filePath);
    }

    Json::Value root;
    Json::CharReaderBuilder reader;
    std::string errs;

    if (!Json::parseFromStream(reader, configFile, &root, &errs)) {
        throw std::runtime_error("JSON parse error: " + errs);
    }

    // 填充各模块配置
    populateMeta(m_detectMeta, root["detectSettings"]);
    populateMeta(m_alertWindowMeta, root["alertWindowSettings"]);
    populateMeta(m_inferMeta, root["inferenceSettings"]);
    populateMeta(m_imageProcessMeta, root["imageProcessSettings"]);
    populateMeta(m_logMeta, root["logSettings"]);
    populateMeta(m_uploadMeta, root["uploadSettings"]);
    populateMeta(m_testMeta, root["testSettings"]);

    m_lastConfigRoot = root;

    return true;
}

bool ConfigParser::loadServerConfig(const std::string& filePath) {
    std::ifstream configFile(filePath);
    if (!configFile.is_open()) {
        throw std::runtime_error("Failed to open config file: " + filePath);
    }

    Json::Value root;
    Json::CharReaderBuilder reader;
    std::string errs;

    if (!Json::parseFromStream(reader, configFile, &root, &errs)) {
        throw std::runtime_error("JSON parse error: " + errs);
    }

    // 填充各模块配置
    populateMeta(m_serverMeta, root["serverSettings"]);

    return true;
}

void ConfigParser::registerListener(const std::string& section, IConfigUpdateListener* listener) {
    m_listeners.insert_or_assign(section, listener);
}

void ConfigParser::reloadConfig(const std::string& filePath) {
    std::ifstream configFile(filePath);
    if (!configFile.is_open()) {
        throw std::runtime_error("Failed to open config file: " + filePath);
    }

    Json::Value root;
    Json::CharReaderBuilder reader;
    std::string errs;

    if (!Json::parseFromStream(reader, configFile, &root, &errs)) {
        throw std::runtime_error("JSON parse error: " + errs);
    }

#if 0
    populateMeta(m_detectMeta, root["detectSettings"]);
    populateMeta(m_alertWindowMeta, root["alertWindowSettings"]);
    populateMeta(m_inferMeta, root["inferenceSettings"]);
    populateMeta(m_imageProcessMeta, root["imageProcessSettings"]);
    populateMeta(m_logMeta, root["logSettings"]);
    populateMeta(m_uploadMeta, root["uploadSettings"]);
    populateMeta(m_testMeta, root["testSettings"]);
#endif

    if (m_lastConfigRoot != root) {
        //checkAndUpdateSection("detectSettings", root["detectSettings"], m_detectMeta);
        checkAndUpdateSection("alertWindowSettings", root["alertWindowSettings"], m_alertWindowMeta);
        checkAndUpdateSection("inferenceSettings", root["inferenceSettings"], m_inferMeta);
        checkAndUpdateSection("imageProcessSettings", root["imageProcessSettings"], m_imageProcessMeta);
        //checkAndUpdateSection("logSettings", root["logSettings"], m_logMeta);
        //checkAndUpdateSection("uploadSettings", root["uploadSettings"], m_uploadMeta);
        //checkAndUpdateSection("testSettings", root["testSettings"], m_testMeta);

        m_lastConfigRoot = root;
    }
}

void ConfigParser::populateMeta(std::shared_ptr<MyMeta> &meta, const Json::Value& jsonValue) {
    const auto& members = jsonValue.getMemberNames();
    for (const auto& key : members) {
        const auto& value = jsonValue[key];

        if (value.isNull()) {
            meta->set(key, nullptr); // 或跳过该字段
        }
        else if (value.isBool()) {
            meta->set(key, value.asBool());
            MY_SPDLOG_DEBUG("{} set to asBool {}", key, value.asBool());
        }
        else if (value.isInt()) {
            meta->set(key, value.asInt());
            MY_SPDLOG_DEBUG("{} set to asInt {}", key, value.asInt());
        }
        else if (value.isUInt()) {
            meta->set(key, value.asUInt());
            MY_SPDLOG_DEBUG("{} set to asUInt {}", key, value.asUInt());
        }
        else if (value.isInt64()) {
            meta->set(key, value.asInt64());
            MY_SPDLOG_DEBUG("{} set to asInt64 {}", key, value.asInt64());
        }
        else if (value.isUInt64()) {
            meta->set(key, value.asUInt64());
            MY_SPDLOG_DEBUG("{} set to asUInt64 {}", key, value.asUInt64());
        }
        else if (value.isDouble()) {
            meta->set(key, value.asDouble());
            MY_SPDLOG_DEBUG("{} set to asDouble {}", key, value.asDouble());
        }
        else if (value.isString()) {
            meta->set(key, value.asString());
            MY_SPDLOG_DEBUG("{} set to asString {}", key, value.asString());
        }
        else {
            throw std::runtime_error("Unsupported JSON type for key: " + key);
        }
    }
}

void ConfigParser::notifyListeners(const std::string& section, std::shared_ptr<MyMeta> &meta) {
    auto it = m_listeners.find(section);
    if (it != m_listeners.end() && it->second) {
        MY_SPDLOG_DEBUG("Notifying listener for section: {}", section);
        it->second->onConfigUpdated(meta);
    }
    else {
        MY_SPDLOG_DEBUG("No listener registered for section: {}", section);
    }
}

void ConfigParser::checkAndUpdateSection(const std::string& sectionName,
    const Json::Value& newJsonValue, std::shared_ptr<MyMeta>& meta) {
    const Json::Value& oldJsonValue = m_lastConfigRoot[sectionName];
    if (newJsonValue == oldJsonValue) { return; }

    populateMeta(meta, newJsonValue);
    notifyListeners(sectionName, meta);
}
