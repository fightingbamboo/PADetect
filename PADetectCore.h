#ifndef PADETECTCORE_H
#define PADETECTCORE_H

#include "MyLogger.hpp"

// 前向声明以避免头文件依赖
class ImageProcessor;
class ConfigParser;
class KeyVerifier;
class MyMeta;
class MNNDetector;
class SingletonApp;
class PicFileUploader;

#include <memory>
#include <functional>
#include <string>
#include <cstdint>

// 检测结果结构体
struct DetectionResult {
    uint32_t lenCount;
    uint32_t phoneCount;
    uint32_t faceCount;
    uint32_t suspectedCount;
};

// 告警类型枚举
enum class AlertType {
    Phone = 0,
    Peep,
    Nobody,
    Occlude,
    NoConnect,
    Suspect
};

// 检测状态枚举
enum class DetectionStatus {
    Stopped = 0,
    Running,
    Error
};

// 回调函数类型定义
using DetectionCallback = std::function<void(const DetectionResult&)>;
using AlertCallback = std::function<void(AlertType)>;
using StatusCallback = std::function<void(DetectionStatus, const std::string&)>;

/**
 * PADetectCore - 核心检测引擎类
 * 封装main.cpp中的所有业务逻辑，提供C++接口供Objective-C桥接层调用
 */
class PADetectCore {
public:
    // 单例模式
    static PADetectCore* getInstance();
    
    // 禁用拷贝构造和赋值
    PADetectCore(const PADetectCore&) = delete;
    PADetectCore& operator=(const PADetectCore&) = delete;
    
    // 初始化和配置
    bool initialize();
    bool setModelPath(const std::string& modelPath);
    bool loadServerConfig(const std::string& configPath);
    bool loadConfig(const std::string& configPath);
    
    // 检测控制
    bool startDetection();
    void stopDetection();
    bool isDetectionRunning() const;
    
    // 摄像头设置
    bool setCameraSettings(int32_t cameraId, int32_t width, int32_t height);
    
    // 测试模式设置
    void setTestMode(bool enabled, const std::string& videoPath = "");
    void setSourcePreview(bool enabled);
    
    // 告警设置
    void setAlertEnabled(bool enabled, AlertType alertType);
    bool getAlertEnabled(AlertType alertType) const;
    void showAlert(AlertType alertType);
    void hideAlert();
    bool isAlertShowing() const;
    
    // 参数配置
    void setDetectionThreshold(float threshold, AlertType alertType);
    void setAlertInterval(int32_t intervalMs);
    void setCaptureInterval(int32_t intervalMs);
    
    // 回调设置
    void setDetectionCallback(DetectionCallback callback);
    void setAlertCallback(AlertCallback callback);
    void setStatusCallback(StatusCallback callback);
    
    // 供ImageProcessor调用的检测结果通知方法
    void reportDetectionResult(const DetectionResult& result);
    
    // 版本信息
    std::string getVersion() const;
    
    // 日志级别设置
    void setLogLevel(int level);
    
    // 获取当前状态
    DetectionStatus getStatus() const;
    
    // 锁屏相关方法
    void setNoFaceLockEnabled(bool enabled);
    void setNoFaceLockTimeout(int32_t timeoutMs);
    void triggerScreenLock();
    
    // 检查单例实例
    bool checkSingletonInstance();
    
    // 检查软件授权
    bool checkSoftwareAuthorization();
    
    // 获取配置解析器
    ConfigParser* getConfigParser();
    
    // 获取检测器
    MNNDetector* getDetector();
    
    // 获取图像处理器
    ImageProcessor* getImageProcessor();
    
    // 基于main.cpp的完整业务逻辑方法
    bool verifyOnlineKey();
    bool subscribeOnlineConfig();
    void runMainLoop();
    
private:
    PADetectCore();
    ~PADetectCore();
    
    // 初始化各个组件
    bool initializeSingleton();
    bool initializeLogger();
    bool initializeDetector();
    bool initializeImageProcessor();
    bool initializeUploader();
    
    // 通知回调
    void notifyDetectionResult(const DetectionResult& result);
    void notifyAlert(AlertType alertType);
    void notifyStatusChange(DetectionStatus status, const std::string& errorMessage = "");
    
    // 检查授权过期
    bool isAfterTargetDate();
    
    // 文件系统相关
    bool checkUpdateFile();
    
private:
    static PADetectCore* instance_;
    
    // 核心组件
    SingletonApp* singletonApp_;
    MySpdlog* logger_;
    ConfigParser* configParser_;
    MNNDetector* detector_;
    std::unique_ptr<ImageProcessor> imageProcessor_;
    PicFileUploader* picUploader_;
    
    // 状态变量
    DetectionStatus status_;
    bool isInitialized_;
    bool alertShowing_;
    
    // 配置参数
    int32_t cameraId_;
    int32_t cameraWidth_;
    int32_t cameraHeight_;
    int32_t captureInterval_;
    int32_t alertInterval_;
    bool testMode_;
    std::string testVideoPath_;
    bool sourcePreview_;
    
    // 回调函数
    DetectionCallback detectionCallback_;
    AlertCallback alertCallback_;
    StatusCallback statusCallback_;
    
    // 模型路径
    std::string modelPath_;
    
    // 私有成员变量
    static constexpr const char* CLIENT_VERSION = "1.0.0";
    static constexpr const char* UPDATE_FILE_PATH = "update.json";
    static constexpr uint64_t SUPPORT_END_TIME = 1756655999;
};

#endif // PADETECTCORE_H