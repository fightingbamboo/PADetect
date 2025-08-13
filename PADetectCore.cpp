
#include "PADetectCore.h"
#include "MyLogger.hpp"
#include "ConfigParser.h"
#include "KeyVerifier.h"
#include "MyMeta.h"
#include "MyWindMsgBox.h"
#include "SingletonApp.h"
#include "ImageProcessor.h"
#include "MNNDetector.h"
#include "PicFileUploader.h"

#include <memory>
#include <functional>
#include <string>
#include <cstdint>
#include <chrono>
#include <thread>
#include <sys/stat.h>
#include <unistd.h>

// 常量定义
static constexpr const char* CLIENT_VERSION = "1.0.7";
static constexpr uint64_t SUPPORT_END_TIME = 1756655999;

// 全局MNN检测器实例
MNNDetector* g_mnn_detector = nullptr;

// 静态成员初始化
PADetectCore* PADetectCore::instance_ = nullptr;

PADetectCore* PADetectCore::getInstance() {
    if (instance_ == nullptr) {
        instance_ = new PADetectCore();
    }
    return instance_;
}

PADetectCore::PADetectCore()
    : singletonApp_(nullptr)
    , logger_(nullptr)
    , configParser_(nullptr)
    , detector_(nullptr)
    , imageProcessor_(nullptr)
    , picUploader_(nullptr)
    , status_(DetectionStatus::Stopped)
    , isInitialized_(false)
    , alertShowing_(false)
    , cameraId_(0)
    , cameraWidth_(640)
    , cameraHeight_(480)
    , captureInterval_(300)
    , alertInterval_(5000)
    , testMode_(false)
    , sourcePreview_(false) {
}

PADetectCore::~PADetectCore() {
    stopDetection();
    if (detector_) {
        delete detector_;
        detector_ = nullptr;
        g_mnn_detector = nullptr;
    }
    if (logger_) {
        logger_->shutdown();
    }
}

bool PADetectCore::initialize() {
    if (isInitialized_) {
        return true;
    }
    
    // 1. 创建log目录并重定向stdout和stderr到文件
    system("mkdir -p /Users/bamboo/Documents/PADetect/logs");
    freopen("/Users/bamboo/Documents/PADetect/logs/output.log", "w", stdout);
    freopen("/Users/bamboo/Documents/PADetect/logs/error.log", "w", stderr);
    
    std::cerr << "[DEBUG] Starting PADetectCore initialization..." << std::endl;
    
    // 2. 首先初始化日志系统
    std::cerr << "[DEBUG] Initializing logger..." << std::endl;
    if (!initializeLogger()) {
        std::cerr << "[ERROR] Logger initialization failed" << std::endl;
        notifyStatusChange(DetectionStatus::Error, "初始化log系统失败");
        return false;
    }
    std::cerr << "[DEBUG] Logger initialized successfully" << std::endl;
    
    // 3. 初始化单例检查
    std::cerr << "[DEBUG] Checking singleton..." << std::endl;
    if (!initializeSingleton()) {
        std::cerr << "[ERROR] Singleton check failed" << std::endl;
        notifyStatusChange(DetectionStatus::Error, "单例检查失败");
        return false;
    }
    std::cerr << "[DEBUG] Singleton check passed" << std::endl;
    
    // 4. 检查软件授权
    std::cerr << "[DEBUG] Checking software authorization..." << std::endl;
    if (!checkSoftwareAuthorization()) {
        std::cerr << "[ERROR] Software authorization failed" << std::endl;
        notifyStatusChange(DetectionStatus::Error, "软件授权过期");
        return false;
    }
    std::cerr << "[DEBUG] Software authorization passed" << std::endl;
    
    // 5. 初始化配置解析器
    std::cerr << "[DEBUG] Initializing config parser..." << std::endl;
    configParser_ = ConfigParser::getInstance();
    if (!configParser_) {
        std::cerr << "[ERROR] Config parser initialization failed" << std::endl;
        notifyStatusChange(DetectionStatus::Error, "配置解析器初始化失败");
        return false;
    }
    std::cerr << "[DEBUG] Config parser initialized successfully" << std::endl;
    
    isInitialized_ = true;
    std::cerr << "[DEBUG] PADetectCore initialization completed successfully" << std::endl;
    notifyStatusChange(DetectionStatus::Stopped);
    return true;
}

bool PADetectCore::setModelPath(const std::string& modelPath) {
    if (status_ == DetectionStatus::Running) {
        return false; // 不能在运行时修改模型路径
    }
    modelPath_ = modelPath;
    return true;
}

bool PADetectCore::loadServerConfig(const std::string& configPath) {
    try {
        MY_SPDLOG_INFO("Loading server config from: {}", configPath);
        if (!configParser_) {
            MY_SPDLOG_ERROR("ConfigParser not initialized");
            return false;
        }
        
        bool result = configParser_->loadServerConfig(configPath);
        if (result) {
            std::shared_ptr<MyMeta> serverMeta = configParser_->getServerMeta();
            if (serverMeta) {
                serverMeta->set("client_version", CLIENT_VERSION);
            }
        }
        return result;
    }
    catch (const std::exception& e) {
        MY_SPDLOG_CRITICAL("serverConfig.json parse Error: {}", e.what());
        return false;
    }
}

bool PADetectCore::loadConfig(const std::string& configPath) {
    try {
        MY_SPDLOG_INFO("Loading config from: {}", configPath);
        if (!configParser_) {
            MY_SPDLOG_ERROR("ConfigParser not initialized");
            return false;
        }
        
        bool result = configParser_->loadConfig(configPath);
        if (result) {
            // 应用日志配置
            std::shared_ptr<MyMeta> logMeta = configParser_->getLogMeta();
            if (logMeta && logger_) {
                bool logEnable = logMeta->getBoolOrDefault("log_enable", true);
                int32_t logLevel = logMeta->getInt32OrDefault("log_level", 1);
                
                if (logEnable) {
                    logger_->setLogLevel(static_cast<spdlog::level::level_enum>(logLevel));
                } else {
                    logger_->setLogLevel(spdlog::level::level_enum::off);
                }
            }
        }
        return result;
    }
    catch (const std::exception& e) {
        MY_SPDLOG_CRITICAL("config.json parse Error: {}", e.what());
        return false;
    }
}

bool PADetectCore::startDetection() {
    if (!isInitialized_) {
        notifyStatusChange(DetectionStatus::Error, "系统未初始化");
        return false;
    }
    
    if (status_ == DetectionStatus::Running) {
        return true;
    }
    
    // 初始化检测器
    if (!initializeDetector()) {
        notifyStatusChange(DetectionStatus::Error, "检测器初始化失败");
        return false;
    }
    
    // 初始化上传器
    if (!initializeUploader()) {
        notifyStatusChange(DetectionStatus::Error, "上传器初始化失败");
        return false;
    }
    
    // 初始化图像处理器
    if (!initializeImageProcessor()) {
        notifyStatusChange(DetectionStatus::Error, "图像处理器初始化失败");
        return false;
    }

    status_ = DetectionStatus::Running;
    notifyStatusChange(DetectionStatus::Running);
    
    return true;
}

void PADetectCore::stopDetection() {
    if (status_ == DetectionStatus::Running) {
        if (imageProcessor_) {
            imageProcessor_->stop();
        }
        status_ = DetectionStatus::Stopped;
        notifyStatusChange(DetectionStatus::Stopped);
    }
}

bool PADetectCore::isDetectionRunning() const {
    return status_ == DetectionStatus::Running;
}

bool PADetectCore::setCameraSettings(int32_t cameraId, int32_t width, int32_t height) {
    if (status_ == DetectionStatus::Running) {
        return false; // 不能在运行时修改摄像头设置
    }
    
    cameraId_ = cameraId;
    cameraWidth_ = width;
    cameraHeight_ = height;
    return true;
}

void PADetectCore::setTestMode(bool enabled, const std::string& videoPath) {
    testMode_ = enabled;
    testVideoPath_ = videoPath;
}

void PADetectCore::setSourcePreview(bool enabled) {
    sourcePreview_ = enabled;
}

void PADetectCore::setAlertEnabled(bool enabled, AlertType alertType) {
    if (!imageProcessor_) {
        return;
    }
    
    // 根据告警类型设置对应的开关
    switch (alertType) {
        case AlertType::Phone:
            imageProcessor_->setAlertEnables(enabled, false, false, false, false);
            break;
        case AlertType::Peep:
             imageProcessor_->setAlertEnables(false, enabled, false, false, false);
             break;
        case AlertType::Suspect:
            imageProcessor_->setAlertEnables(false, false, enabled, false, false);
            break;
        case AlertType::Nobody:
            imageProcessor_->setAlertEnables(false, false, false, enabled, false);
            break;
        case AlertType::Occlude:
            imageProcessor_->setAlertEnables(false, false, false, false, enabled);
            break;
        default:
            break;
    }
}

void PADetectCore::showAlert(AlertType alertType) {
    alertShowing_ = true;
    notifyAlert(alertType);
}

void PADetectCore::hideAlert() {
    alertShowing_ = false;
}

bool PADetectCore::isAlertShowing() const {
    return alertShowing_;
}

void PADetectCore::setDetectionThreshold(float threshold, AlertType alertType) {
    if (!detector_) {
        return;
    }
    
    // 这里可以根据alertType设置不同的检测阈值
    // 由于YOLOv3Detector的接口限制，暂时记录日志
    MY_SPDLOG_INFO("Setting detection threshold: {} for alert type: {}", threshold, static_cast<int>(alertType));
}

void PADetectCore::setAlertInterval(int32_t intervalMs) {
    alertInterval_ = intervalMs;
}

void PADetectCore::setCaptureInterval(int32_t intervalMs) {
    captureInterval_ = intervalMs;
}

void PADetectCore::setDetectionCallback(DetectionCallback callback) {
    detectionCallback_ = callback;
}

void PADetectCore::setAlertCallback(AlertCallback callback) {
    alertCallback_ = callback;
}

void PADetectCore::setStatusCallback(StatusCallback callback) {
    statusCallback_ = callback;
}

void PADetectCore::reportDetectionResult(const DetectionResult& result) {
    notifyDetectionResult(result);
}

std::string PADetectCore::getVersion() const {
    return CLIENT_VERSION;
}

void PADetectCore::setLogLevel(int level) {
    MySpdlog::getInstance()->setLogLevel(static_cast<spdlog::level::level_enum>(level));
}

DetectionStatus PADetectCore::getStatus() const {
    return status_;
}

void PADetectCore::setNoFaceLockEnabled(bool enabled) {
    if (imageProcessor_) {
        imageProcessor_->setNoFaceLockEnabled(enabled);
    }
}

void PADetectCore::setNoFaceLockTimeout(int32_t timeoutMs) {
    if (imageProcessor_) {
        imageProcessor_->setNoFaceLockTimeout(timeoutMs);
    }
}

void PADetectCore::triggerScreenLock() {
    // 这个方法在macOS上由Objective-C层实现
    // 这里只是提供接口，实际锁屏逻辑在PADetectBridge.mm中
}

bool PADetectCore::checkSingletonInstance() {
    return true; // 简化实现
}

bool PADetectCore::checkSoftwareAuthorization() {
    return !isAfterTargetDate();
}

ConfigParser* PADetectCore::getConfigParser() {
    return configParser_;
}

MNNDetector* PADetectCore::getDetector() {
    return detector_;
}

ImageProcessor* PADetectCore::getImageProcessor() {
    return imageProcessor_.get();
}

// 私有方法实现
bool PADetectCore::initializeSingleton() {
    try {
        singletonApp_ = SingletonApp::getInstance();
        if (!singletonApp_ || !singletonApp_->isUniqueInstance()) {
            MY_SPDLOG_ERROR("Another instance is already running");
            return false;
        }
        MY_SPDLOG_INFO("Singleton check passed");
        return true;
    }
    catch (const std::exception& e) {
        MY_SPDLOG_ERROR("Singleton check failed: {}", e.what());
        return false;
    }
}

bool PADetectCore::initializeLogger() {
    try {
        logger_ = MySpdlog::getInstance();
        if (!logger_ || !logger_->init()) {
            return false;
        }
        logger_->setLogLevel(spdlog::level::level_enum::trace);
        return true;
    } catch (const std::exception& e) {
        // 如果日志初始化失败，使用标准输出
        std::cerr << "Logger initialization failed: " << e.what() << std::endl;
        return false;
    }
}

bool PADetectCore::initializeDetector() {
    MY_SPDLOG_INFO("Client Version: {}", CLIENT_VERSION);
    
    if (modelPath_.empty()) {
        MY_SPDLOG_ERROR("Model path not set");
        return false;
    }
    
    try {
        // 创建MNN检测器实例
        const std::vector<std::string> class_names{"lens", "phone", "face"};
        detector_ = new MNNDetector(modelPath_, class_names);
        if (!detector_) {
            MY_SPDLOG_ERROR("Failed to create MNNDetector instance");
            return false;
        }
        
        // 设置全局检测器指针
        g_mnn_detector = detector_;
        
        MY_SPDLOG_INFO("MNN Detector initialized successfully with model: {}", modelPath_);
        return true;
    }
    catch (const std::exception& e) {
        MY_SPDLOG_CRITICAL("Exception: {}", e.what());
        return false;
    }
}

bool PADetectCore::initializeImageProcessor() {
    try {
        // 使用带参数的构造函数创建ImageProcessor
        imageProcessor_ = std::make_unique<ImageProcessor>(captureInterval_, cameraId_, cameraWidth_, cameraHeight_);
        if (!imageProcessor_) {
            MY_SPDLOG_ERROR("Failed to create ImageProcessor instance");
            return false;
        }

        // 从配置文件加载参数
        if (configParser_) {
            std::shared_ptr<MyMeta> detectMeta = configParser_->getDetectMeta();
            if (detectMeta) {
                imageProcessor_->setDetectParam(detectMeta);
            }
        }

        // 准备图像处理器
        imageProcessor_->prepare();
        
        // 启动图像处理（这里可能会抛出摄像头相关异常）
        imageProcessor_->start();
        
        // 等待一小段时间确保工作线程启动
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // 检查工作线程状态
        if (!imageProcessor_->getWorkThreadStatus()) {
            MY_SPDLOG_ERROR("ImageProcessor work thread failed to start properly");
            return false;
        }

        
        // 设置测试模式
        // if (testMode_) {
        //         bool testSourcePreview = configParser_->getTestMeta()->getBoolOrDefault("test_source_preview", false);
        //         std::string testVideoPath = configParser_->getTestMeta()->getStringOrDefault("test_video_path", "");
        //         imageProcessor_->setTestConfigs(testSourcePreview, testVideoPath);
        // }
        
        
        MY_SPDLOG_INFO("ImageProcessor initialized successfully");
        return true;
    }
    catch (const std::exception& e) {
        MY_SPDLOG_ERROR("ImageProcessor initialization failed: {}", e.what());
        return false;
    }
}

bool PADetectCore::initializeUploader() {
    try {
        picUploader_ = PicFileUploader::getInstance();
        if (!picUploader_) {
            MY_SPDLOG_ERROR("Failed to get PicFileUploader instance");
            return false;
        }
        
        // 设置上传参数
        if (configParser_) {
            std::shared_ptr<MyMeta> uploadMeta = configParser_->getUploadMeta();
            if (uploadMeta) {
                picUploader_->setUploadParam(uploadMeta);
            }
        }
        
        MY_SPDLOG_INFO("Uploader initialized");
        return true;
    }
    catch (const std::exception& e) {
        MY_SPDLOG_ERROR("PicFileUploader initialization failed: {}", e.what());
        return false;
    }
}

void PADetectCore::notifyDetectionResult(const DetectionResult& result) {
    if (detectionCallback_) {
        detectionCallback_(result);
    }
}

void PADetectCore::notifyAlert(AlertType alertType) {
    if (alertCallback_) {
        alertCallback_(alertType);
    }
}

void PADetectCore::notifyStatusChange(DetectionStatus status, const std::string& errorMessage) {
    status_ = status;
    
    if (statusCallback_) {
        statusCallback_(status, errorMessage);
    }
    
    // 记录状态变化
    MY_SPDLOG_INFO("Status changed to: {} - {}", 
                   static_cast<int>(status), errorMessage);
}

// 添加基于main.cpp的完整业务逻辑方法
bool PADetectCore::verifyOnlineKey() {
#if ONLINE_MODE
    try {
        // 订阅密钥
        MY_SPDLOG_INFO("Subscribing for key...");
        std::unique_ptr<KeySubscriber> keySub = std::make_unique<KeySubscriber>();
        
        if (configParser_) {
            std::shared_ptr<MyMeta> serverMeta = configParser_->getServerMeta();
            if (serverMeta) {
                keySub->setHttpParam(serverMeta);
            }
        }
        
        if (!keySub->subscribeForKey()) {
            MY_SPDLOG_ERROR("remote subscribe key failed");
            return false;
        }
        
        // 验证密钥
        MY_SPDLOG_INFO("Verifying key...");
        std::unique_ptr<KeyVerifier> keyVer = std::make_unique<KeyVerifier>("key.txt");
        if (!keyVer->Verify()) {
            MY_SPDLOG_ERROR("verify key failed");
            return false;
        }
        
        MY_SPDLOG_INFO("Key verification successful");
        return true;
    }
    catch (const std::exception& e) {
        MY_SPDLOG_ERROR("Key verification failed: {}", e.what());
        return false;
    }
#else
    MY_SPDLOG_INFO("Offline mode - skipping key verification");
    return true; // 离线模式直接返回成功
#endif
}

bool PADetectCore::subscribeOnlineConfig() {
#if ONLINE_MODE
    try {
        MY_SPDLOG_INFO("Subscribing for online config...");
        ConfigSubscriber* confSub = ConfigSubscriber::getInstance();
        if (!confSub) {
            MY_SPDLOG_ERROR("Failed to get ConfigSubscriber instance");
            return false;
        }
        
        if (!confSub->subscribeOnline()) {
            MY_SPDLOG_ERROR("remote subscribe config failed");
            return false;
        }
        
        // 启动配置订阅线程
        confSub->start();
        
        MY_SPDLOG_INFO("Config subscription successful");
        return true;
    }
    catch (const std::exception& e) {
        MY_SPDLOG_ERROR("Config subscription failed: {}", e.what());
        return false;
    }
#else
    MY_SPDLOG_INFO("Offline mode - skipping config subscription");
    return true; // 离线模式直接返回成功
#endif
}

void PADetectCore::runMainLoop() {
    if (status_ != DetectionStatus::Running) {
        return;
    }
    
    // 启动配置订阅线程
    subscribeOnlineConfig();
    
    // 主循环 - 检查更新文件和工作线程状态
    while (status_ == DetectionStatus::Running) {
        // 检查更新文件
        if (checkUpdateFile()) {
            MY_SPDLOG_DEBUG("Update file found, exiting...");
            notifyStatusChange(DetectionStatus::Stopped, "发现更新文件");
            break;
        }
        
        // 检查图像处理线程状态
        if (imageProcessor_) {
            if (!imageProcessor_->getWorkThreadStatus()) {
                if (configParser_) {
                    std::shared_ptr<MyMeta> testMeta = configParser_->getTestMeta();
                    std::string testVideoPath = testMeta ? testMeta->getStringOrDefault("test_video_path", "") : "";
                    
                    if (testVideoPath.empty()) {
                        notifyStatusChange(DetectionStatus::Error, "打开摄像头失败");
                    } else {
                        notifyStatusChange(DetectionStatus::Stopped, "测试视频结束");
                    }
                } else {
                    notifyStatusChange(DetectionStatus::Error, "图像处理线程异常");
                }
                break;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    
    // 停止配置订阅
#if ONLINE_MODE
    ConfigSubscriber* confSub = ConfigSubscriber::getInstance();
    if (confSub) {
        confSub->stop();
    }
#endif
}

bool PADetectCore::isAfterTargetDate() {
    auto now = std::chrono::system_clock::now();
    uint64_t timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    if (timestamp > SUPPORT_END_TIME) {
        MY_SPDLOG_ERROR("current time > GMT：Sun Aug 31 2025 23:59:59 GMT+0800");
        return true;
    }
    return false;
}

bool PADetectCore::checkUpdateFile() {
    try {
        struct stat buffer;
        bool exists = (stat("update.json", &buffer) == 0);
        return exists;
    }
    catch (const std::exception& e) {
        MY_SPDLOG_ERROR("Exception checking update file: {}", e.what());
        return false;
    }
}

// C函数桥接，供Objective-C代码调用
extern "C" {
    PADetectCore* PADetectCore_getInstance() {
        return PADetectCore::getInstance();
    }
    
    bool PADetectCore_initialize(PADetectCore* core) {
        if (!core) return false;
        return core->initialize();
    }
    
    void PADetectCore_startDetection(PADetectCore* core) {
        if (core) core->startDetection();
    }
    
    void PADetectCore_stopDetection(PADetectCore* core) {
        if (core) core->stopDetection();
    }
    
    bool PADetectCore_isDetectionRunning(PADetectCore* core) {
        if (!core) return false;
        return core->isDetectionRunning();
    }
    
    bool PADetectCore_setCameraSettings(PADetectCore* core, int cameraId, int width, int height) {
        if (!core) return false;
        return core->setCameraSettings(cameraId, width, height);
    }
    
    void PADetectCore_setTestMode(PADetectCore* core, bool enabled, const char* videoPath) {
        if (core) {
            std::string path = videoPath ? videoPath : "";
            core->setTestMode(enabled, path);
        }
    }
    
    void PADetectCore_setLogLevel(PADetectCore* core, int level) {
        if (core) core->setLogLevel(level);
    }
    
    const char* PADetectCore_getVersion(PADetectCore* core) {
        if (!core) return "";
        static std::string version = core->getVersion();
        return version.c_str();
    }
    
    int PADetectCore_getStatus(PADetectCore* core) {
        if (!core) return 0;
        return static_cast<int>(core->getStatus());
    }
    
    void PADetectCore_setAlertEnabled(PADetectCore* core, bool enabled, int alertType) {
        if (core) core->setAlertEnabled(enabled, static_cast<AlertType>(alertType));
    }
    
    void PADetectCore_showAlert(PADetectCore* core, int alertType) {
        if (core) core->showAlert(static_cast<AlertType>(alertType));
    }
    
    void PADetectCore_hideAlert(PADetectCore* core) {
        if (core) core->hideAlert();
    }
    
    bool PADetectCore_isAlertShowing(PADetectCore* core) {
        if (!core) return false;
        return core->isAlertShowing();
    }
}