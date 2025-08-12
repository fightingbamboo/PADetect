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
#include "AlertWindowManager.h"
#include "ImageProcessor.h"
#include "ConfigParser.h"
#include "MyLogger.hpp"

#include "KeyVerifier.h"
#include "HttpClient.h"
#include "MyMeta.h"
#include "MyWindMsgBox.h"
#include "YOLOv3Detector.h"
#include "SingletonApp.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <memory>
#include <thread>



static constexpr const char* modeRunDevice = "cpu";
static constexpr const char* modePath = "onnx";
static constexpr const char* MODEL_PATH = "./onnx/end2end.onnx";
static constexpr const char* CONFIG_PATH = "./onnx/detail.json";
static constexpr const char* PIPELINE_PATH = "./onnx/pipeline.json";
static constexpr const char* picDataDir = "data";
static constexpr const char* UPDATE_FILE_PATH = "update.json";
static constexpr const char* CLIENT_VERSION = "1.0.7";

static constexpr uint64_t supportEndTime = 1756655999;

bool isAfterTargetDate() {
    // 获取当前时间
    auto now = std::chrono::system_clock::now();
    uint64_t timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();


    if (timestamp > supportEndTime) {
        MY_SPDLOG_ERROR("current time > GMT：Sun Aug 31 2025 23:59:59 GMT+0800");
        return true;
    }
    return false;
}

int main(int argc, char* argv[]) {
    // 1. config singleton
    SingletonApp* singleApp = SingletonApp::getInstance();
    if (!singleApp->isUniqueInstance()) {
        MyWindMsgBox box("您已经开启一个实例, 即将退出...");
        return EXIT_FAILURE;
    }

    // 2. re direct stdout and stderr to file
    freopen("output.log", "w", stdout);
    freopen("error.log", "w", stderr);

    // 3. init log system
    MySpdlog* spdLogger = MySpdlog::getInstance();
    if (NULL == spdLogger || false == spdLogger->init()) {
        MyWindMsgBox box("初始化log系统失败");
        return 1;
    }
    spdLogger->setLogLevel(spdlog::level::level_enum::trace);

    // 4. check software timeout
    if (isAfterTargetDate()) {
        MyWindMsgBox box("软件授权过期, 请联系管理员");
        return 1;
    }

    /*
    { // 5. set dpi
        int32_t dpiSetRet = -1;
        std::string setDpiStr = "";
        dpiSetRet = DpiUtils::SetDpiAwarenessSafely(setDpiStr);
        if (-1 == dpiSetRet) {
            MY_SPDLOG_CRITICAL("set window dpi failed {}", setDpiStr.c_str());
            MyWindMsgBox box("设置屏幕DPI兼容失败");
            return 1;
        }
    }
    */

    // 6. set version and set model run device
    std::string device = "AUTO";
    std::string osVerString = "";
#if defined(_WIN32) || defined(_WIN64)
    osVerString = OsVersionUtil::GetWindowsVersionString();
#else
    // TODO
#endif
    MY_SPDLOG_DEBUG("os version: {}", osVerString);
    if (osVerString == "Windows 7") {
        device = "CPU";
    }
    MY_SPDLOG_INFO("Client Version: {}", CLIENT_VERSION);

    // 7. get and parse server config
    ConfigParser* cfgParser = ConfigParser::getInstance();
    try {
        cfgParser->loadServerConfig("serverConfig.json");
    }
    catch (const std::exception& e) {
        MY_SPDLOG_CRITICAL("serverConfig.json parse Error: {}", e.what());
        MyWindMsgBox box("解析服务器参数失败, 请检查服务器参数文件是否存在");
        return 1;
    }
    std::shared_ptr<MyMeta> serverMeta = cfgParser->getServerMeta();
    serverMeta->set("client_version", CLIENT_VERSION);

#if ONLINE_MODE
    // obtain key
    try {
        // subscribe key
        std::unique_ptr<KeySubscriber> keySub = std::make_unique<KeySubscriber>();
        keySub->setHttpParam(serverMeta);

        if (false == keySub->subscribeForKey()) {
            MY_SPDLOG_ERROR("remote subscribe key failed");
        }
#if 0
        if (false == keySub->subscribeForUnKey()) {
            MY_SPDLOG_ERROR("remote subscribe uninstall key failed");
        }
#endif
    }
    catch (const std::exception& e) {
        MY_SPDLOG_ERROR("get key remote excaption: {}", e.what());
    }
    // verify key
    try {
        std::unique_ptr<KeyVerifier> keyVer = std::make_unique<KeyVerifier>("key.txt");
        if (false == keyVer->Verify()) {
            MY_SPDLOG_ERROR("verity key failed");
            MyWindMsgBox box("授权验证失败");
            return 1;
        }
    }
    catch (const std::exception& e) {
        MY_SPDLOG_ERROR("check key exception: {}", e.what());
        MyWindMsgBox box("授权验证失败");
        return 1;
    }
    // obtain config
    try {
        ConfigSubscriber* confSub = ConfigSubscriber::getInstance();
        if (false == confSub->subscribeOnline()) {
            MY_SPDLOG_ERROR("remote subscribe config failed");
        }
    }
    catch (const std::exception& e) {
        MY_SPDLOG_ERROR("get remote config excaption: {}", e.what());
    }
#endif

    // 8. get and parse normal config
    try {
        cfgParser->loadConfig("config.json");
    }
    catch (const std::exception& e) {
        MY_SPDLOG_CRITICAL("config.json parse Error: {}", e.what());
        MyWindMsgBox box("解析客户端参数失败, 请检查客户端参数文件是否存在");
        return 1;
    }

    std::shared_ptr<MyMeta> logMeta = cfgParser->getLogMeta();
    bool logEnable = logMeta->getBoolOrDefault("log_enable", true);
    int32_t logLevel = logMeta->getInt32OrDefault("log_level", 1);

    if (logEnable) {
        spdLogger->setLogLevel(static_cast<spdlog::level::level_enum>(logLevel) );
    }
    else {
        spdLogger->setLogLevel(spdlog::level::level_enum::off);
    }

    // 9. init alert window manager and set param
    std::shared_ptr<MyMeta> alertWindowMeta = cfgParser->getAlertWindowMeta();
    AlertWindowManager* alertWindMgr = AlertWindowManager::getInstance();
    alertWindMgr->setAlertVersion(CLIENT_VERSION);
    alertWindMgr->setAlertParam(alertWindowMeta);

    std::shared_ptr<MyMeta> inferMeta = cfgParser->getInferMeta();
#if (OPENVINO_MODE)
    try {
        YOLOv3Detector* detector = YOLOv3Detector::getInstance();
        if (!detector->Initialize(MODEL_PATH, CONFIG_PATH, PIPELINE_PATH, device)) {
            MY_SPDLOG_CRITICAL("Failed to initialize detector");
            return -1;
        }
        detector->setDetectParam(inferMeta);
    }
    catch (const std::exception& e) {
        MY_SPDLOG_CRITICAL("Exception: {}", e.what());
        return -1;
    }
#else

#endif

    // 10. init file upload and set param
    std::shared_ptr<MyMeta> uploadMeta = cfgParser->getUploadMeta();
    PicFileUploader* picUploader = PicFileUploader::getInstance();
    picUploader->setUploadParam(uploadMeta);

    // 11. init camera process and set param
    std::shared_ptr<MyMeta> imageProcessMeta = cfgParser->getImageProcessMeta();
    std::shared_ptr<MyMeta> testMeta = cfgParser->getTestMeta();
    std::unique_ptr<ImageProcessor> imgProc = std::make_unique<ImageProcessor>();
    imgProc->setDetectParam(imageProcessMeta);
    imgProc->setTestParam(testMeta);
    imgProc->prepare();
    imgProc->start();
    std::string testVideoPath = testMeta->getStringOrDefault("test_video_path", "");

    // 12. start config subscribe thread
    ConfigSubscriber* confSub = ConfigSubscriber::getInstance();
    confSub->start();
    while (true) {
        std::filesystem::path updateFilePath = UPDATE_FILE_PATH;
        std::error_code ec;
        bool exists = std::filesystem::exists(updateFilePath, ec);
        if (ec) {
            MyWindMsgBox box("打开更新文件失败, 请联系管理员");
            break;
        }

        if (exists) {
            MY_SPDLOG_DEBUG("begin update, exit current process");
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        if (false == imgProc->getWorkThreadStatus()) {
            if (testVideoPath.empty()) {
                MyWindMsgBox box("打开摄像头失败, 请联系管理员");
            }
            else {
                MyWindMsgBox box("测试视频结束, 请查看测试报告");
            }
            break;
        }
        // hs debug

    }
    confSub->stop();
    imgProc->stop();
    // 恢复原始的 stdout 和 stderr
    fclose(stdout);
    fclose(stderr);
    spdLogger->shutdown();
    return 0;

}
