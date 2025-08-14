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
#include "ImageProcessor.h"
#include "MyLogger.hpp"
#include "CommonUtils.h"
#include "MNNDetector.h"
#include "PADetectCore.h"
#import "PADetect/PADetectBridge.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <filesystem>
#include <algorithm>
#include <shared_mutex>
#include <sstream>

// 定义告警类型常量
enum ALERT_TYPE {
    TEXT_PHONE = 0,
    TEXT_PEEP,
    TEXT_NOBODY,
    TEXT_OCCLUDE,
    TEXT_NOCONNECT,
    TEXT_SUSPECT,
    COUNT,
};

#include <iomanip>
#include <fstream>

#import <AVFoundation/AVFoundation.h>
// OpenCV 已通过 ImageProcessor.h 包含

namespace fs = std::filesystem;
constexpr int32_t MAX_CAP_IDX = 9;

void getDateAndImgStr(std::string &dataStr, std::string &ImgStr) {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);

    char dataBuf[128];
    strftime(dataBuf, sizeof(dataBuf), "%Y-%m-%d_%H-%M-%S", std::localtime(&now_time_t));
    std::string fullDateTimeStr = dataBuf;
    dataStr = fullDateTimeStr.substr(0, fullDateTimeStr.find('_'));
    ImgStr = dataBuf;
}

std::string getDateStr() {
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);

    // 获取毫秒部分
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    // 线程安全的时间转换
    std::tm tm_buffer;
#ifdef _WIN32
    localtime_s(&tm_buffer, &now_time);
#else
    localtime_r(&now_time, &tm_buffer);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buffer, "%Y-%m-%d_%H-%M-%S")
        << "-" << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}

// Function to get camera device names and uniqueIDs (macOS only)
// 使用PADetectBridge统一的摄像头枚举实现
std::vector<std::string> getCameraDeviceNames(std::vector<std::string>& deviceUniqueIDs) {
    std::vector<std::string> deviceNames;
    
    @autoreleasepool {
        // 使用PADetectBridge的统一实现
        PADetectBridge *bridge = [PADetectBridge sharedInstance];
        NSArray<NSString *> *cameraIds = [bridge getAvailableCameraIds];
        NSArray<NSString *> *cameraNames = [bridge getAvailableCameras];
        
        // 转换为C++容器
        for (NSUInteger i = 0; i < cameraIds.count && i < cameraNames.count; i++) {
            std::string uniqueID = [cameraIds[i] UTF8String];
            std::string deviceName = [cameraNames[i] UTF8String];
            deviceUniqueIDs.push_back(uniqueID);
            deviceNames.push_back(deviceName);
        }
    }

    return deviceNames;
}



ImageProcessor::ImageProcessor(
    int32_t capInterval,
    const std::string& cameraId,
    int32_t cameraWidth,
    int32_t cameraHeight) :
    m_capInterval(capInterval),
    m_cameraId(cameraId),
    m_cameraWidth(cameraWidth),
    m_cameraHeight(cameraHeight) {
    MY_SPDLOG_DEBUG(">>>ImageProcessor实例创建");
}

ImageProcessor::~ImageProcessor() {
    MY_SPDLOG_DEBUG("<<<ImageProcessor实例销毁");
}

void ImageProcessor::prepare() {
    m_scrShot = std::make_unique<ScreenShotMacOs>();
    bool ret = m_scrShot->init();
    MY_SPDLOG_INFO("screen shot init ret: {}", ret);
}

void ImageProcessor::start() {
    m_continue.store(true);
    m_thread = std::thread(&ImageProcessor::work, this);

    m_alertContinue.store(true);
    m_alertThd = std::thread(&ImageProcessor::alertWork, this);
}

void ImageProcessor::stop() {
    m_continue.store(false);
    if (m_thread.joinable()) {
        m_thread.join();
    }

    m_alertContinue.store(false);
#if PLATFORM_WINDOWS
    SetEvent(m_hAlertEvent);
#endif
    if (m_alertThd.joinable()) {
        m_alertThd.join();
    }

    if (m_scrShot) {
        m_scrShot->deinit();
    }

    if (!m_testVideoPath.empty()) { writeTestDataToJson(); }
}

void ImageProcessor::setAlertEnables(const bool alertPhoneEnable, const bool alertPeepEnable,
    const bool alertNobodyEnable, const bool alertNobodyLockEnable, const bool alertNoconnectEnable) {
    m_alertPhoneEnable = alertPhoneEnable;
    m_alertPeepEnable = alertPeepEnable;
    m_alertNobodyEnable = alertNobodyEnable;
    m_alertNobodyLockEnable = alertNobodyLockEnable;
    m_alertNoconnectEnable = alertNoconnectEnable;
}

void ImageProcessor::setTestConfigs(const bool sourcePreview, const std::string& testVideoPath)
{
    m_testSourcePreview = sourcePreview;
    m_testVideoPath = testVideoPath;
}

bool ImageProcessor::getWorkThreadStatus() const
{
    return m_workThreadStatus.load();
}

void ImageProcessor::work() {

    try {
        if (m_testVideoPath.empty()) {
            // camera
            MY_SPDLOG_INFO("camera width: {} height: {}", m_cameraWidth, m_cameraHeight);
            /*
            if (false == openCameraOnce(m_cameraId)) {
                m_cap.reset();
                cv::destroyAllWindows();
                throw std::runtime_error("open camera once failed");
            }
            */
            if (!openCameraUntilTrue()) {
                throw std::runtime_error("open camera untile true failed");
            }
        }
        else {
            if (false == openVideoOnce()) {
                m_cap.reset();
                cv::destroyAllWindows();
                throw std::runtime_error("open video once failed");
            }
        }

        // 获取MNN检测器实例
        extern MNNDetector* g_mnn_detector;
        MNNDetector* detector = g_mnn_detector;
        int32_t cam_width = static_cast<int32_t>(m_cap->get(cv::CAP_PROP_FRAME_WIDTH));
        int32_t cam_height = static_cast<int32_t>(m_cap->get(cv::CAP_PROP_FRAME_HEIGHT));
        MY_SPDLOG_INFO("camera real resolution {} x {}", cam_width, cam_height);
        uint32_t lenCnt = 0, phoneCnt = 0, faceCnt = 0, suspectedCnt = 0;
        while (m_continue.load()) {
            if (!m_cap) { // only camera situation could run into here
                if (!openCameraUntilTrue()) {
                    throw std::runtime_error("open camera untile true failed");
                }
            }
            // 图像捕获
            m_cap->read(m_cameraFrame);
            if (m_cameraFrame.empty()) {
                if (m_testVideoPath.empty()) { // camera disconnect
                    MY_SPDLOG_ERROR("Frame capture failed. Attempting to reconnect...");
                    if (!openCameraUntilTrue()) {
                        throw std::runtime_error("open camera untile true failed");
                    }
                    continue;
                }
                else { // video end of stream
                    throw std::runtime_error("video end of stream");
                    break;
                }
            }

            // MNN对象检测
            // double detectCost = 0.0; // Unused variable removed
            lenCnt = 0;
            phoneCnt = 0;
            faceCnt = 0;
            suspectedCnt = 0;
            
            if (detector) {
                // MNN检测器返回检测结果
                std::vector<Detection> detections = detector->detect(m_cameraFrame);
                
                // 统计各类别数量
                for (const auto& det : detections) {
                    if (det.class_id == 1) { // lens
                        lenCnt++;
                    } else if (det.class_id == 2) { // phone
                        phoneCnt++;
                    } else if (det.class_id == 0) { // face
                        faceCnt++;
                    }
                }
            }
            MY_SPDLOG_TRACE("lenCnt {} phoneCnt {} faceCnt {} suspectedCnt {}",
                            lenCnt, phoneCnt, faceCnt, suspectedCnt);

            // 通知检测结果到PADetectCore
            PADetectCore* core = PADetectCore::getInstance();
            if (core) {
                DetectionResult result;
                result.lenCount = lenCnt;
                result.phoneCount = phoneCnt;
                result.faceCount = faceCnt;
                result.suspectedCount = suspectedCnt;
                core->reportDetectionResult(result);
            }

            // 确定警报类型和睡眠间隔 - AlertWindowManager已删除，改用简单枚举
            int newMode = ALERT_TYPE::COUNT;
            long sleepInterval = m_capInterval;  // 默认采样间隔

            {                
                std::shared_lock<std::shared_mutex> readLock(m_paramMtx);
                if (0 != lenCnt || 0 != phoneCnt) {
                    ++m_detPhoneCnt;
                    newMode = m_alertPhoneEnable ? ALERT_TYPE::TEXT_PHONE : newMode;
                    sleepInterval = m_alertShowInterval;
                    m_isNoFaceTiming = false;
                }
                else if (1 < faceCnt) {
                    ++m_detPeepCnt;
                    newMode = m_alertPeepEnable ? ALERT_TYPE::TEXT_PEEP : newMode;
                    sleepInterval = m_alertShowInterval;
                    m_isNoFaceTiming = false;
                }
                else if (0 == faceCnt) {
                    if (isCameraOccludedByTraditional(m_cameraFrame)) {
                        ++m_detOcclude;
                        newMode = m_alertOcculeEnable ? ALERT_TYPE::TEXT_OCCLUDE : newMode;
                        sleepInterval = m_alertShowInterval;
                        handleNoFaceLock();
                    }
                    else {
                        ++m_detNobodyCnt;
                        newMode = m_alertNobodyEnable ? ALERT_TYPE::TEXT_NOBODY : newMode;
                        sleepInterval = m_alertShowInterval;
                        handleNoFaceLock();
                    }
                }
                else if (0 != suspectedCnt) {
                    newMode = m_alertSuspectEnable ? ALERT_TYPE::TEXT_SUSPECT : newMode;
                    sleepInterval = m_alertShowInterval;
                    m_isNoFaceTiming = false;
                }
                else {  // 单张人脸情况
                    sleepInterval = m_capInterval;
                    m_isNoFaceTiming = false;
                }
            }

            // 添加警报任务（如果模式改变）
            if (newMode != m_lastAlertMode) {
                {
                    std::unique_lock<std::mutex> lock(m_alertMtx);
                    m_alertTaskVec.emplace_back(newMode);
                }
#if PLATFORM_WINDOWS
 SetEvent(m_hAlertEvent);
#endif
            }

            // 睡眠控制
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepInterval));

            // 调试模式退出检查
            if (m_testSourcePreview && cv::waitKey(1) >= 0)
                break;
        }
        m_cap.reset();
        cv::destroyAllWindows();
    }
    catch (const std::exception& e) {
        m_workThreadStatus.store(false);
        MY_SPDLOG_TRACE("work thread exit since: {}", e.what() );
        return;
    }
}

#define USE_DATA_DIR 0

void ImageProcessor::alertWork() {
    MY_SPDLOG_INFO(">>>");

    // screen
    int32_t screenWidth = 0, screenHeight = 0;
    if (m_scrShot) {
        m_scrShot->getScreenResolution(screenWidth, screenHeight);
        MY_SPDLOG_TRACE("screen width: {} height: {}", screenWidth, screenHeight);
    }
    std::unique_ptr<uint8_t[]> screenBuf = std::make_unique<uint8_t[]>(screenWidth * screenHeight * 4);
    std::memset(screenBuf.get(), 0, screenWidth * screenHeight * 4);
    cv::Mat screenFrame(screenHeight, screenWidth, CV_8UC4, screenBuf.get());
    
    // pic file upload
    PicFileUploader* picUploader = PicFileUploader::getInstance();
    picUploader->start();

    //std::string prefixCapPathStr = imgCapDir.string();
    // 使用用户主目录下的数据目录，避免只读文件系统问题
    std::string homeDir = std::getenv("HOME") ? std::getenv("HOME") : "/tmp";
    std::string dirPathStr = homeDir + "/.padetect_data";
    std::string baseDir = "";
    bool curDirCreate = false;
    try {
        // 使用系统调用创建目录
        std::string mkdirCmd = "mkdir -p \"" + dirPathStr + "\"";
        int result = system(mkdirCmd.c_str());
        if (result == 0) {
            MY_SPDLOG_INFO("data directory created successfully at: {}", dirPathStr);
            curDirCreate = true;
        } else {
            MY_SPDLOG_WARN("Failed to create directory: {}", dirPathStr);
            curDirCreate = false;
        }
    }
    catch (const std::exception& e) {
        MY_SPDLOG_WARN("Error creating directory: {}", e.what());
        curDirCreate = false;
    }

    if (curDirCreate) {
        baseDir = dirPathStr;
    }

    std::string PreDateStr{ "" }, preImgStr{ "" };
    getDateAndImgStr(PreDateStr, preImgStr);
    std::string prefixPathStr = baseDir;
    // 确保目录存在
    if (!prefixPathStr.empty()) {
        std::string mkdirCmd = "mkdir -p \"" + prefixPathStr + "\"";
        system(mkdirCmd.c_str());
    }
    // work thread loop
    // 设置 JPEG 图像质量
    std::vector<int> params;
    params.push_back(cv::IMWRITE_JPEG_QUALITY);
    params.push_back(60); // 设置质量为 60

    while (m_alertContinue.load()) {
        // macOS等待机制
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::unique_lock<std::mutex> lock(m_alertMtx);
        if (!m_alertContinue.load()) break;
        
        if (m_alertTaskVec.empty()) continue;
        m_lastAlertMode = m_alertTaskVec.back();
        m_alertTaskVec.clear();
        lock.unlock();

        // 处理不同的告警类型
        switch (m_lastAlertMode) {
            case TEXT_PHONE: {
                MY_SPDLOG_INFO("Processing TEXT_PHONE alert");
                if (m_alertPhoneScreenEnable && m_scrShot) {
                    m_scrShot->capture(screenBuf.get());
                    std::string dateStr, imgStr;
                    getDateAndImgStr(dateStr, imgStr);
                    std::string screenPath = prefixPathStr + "/screen_phone_" + imgStr + ".jpg";
                    if (cv::imwrite(screenPath, screenFrame, params)) {
                        MY_SPDLOG_INFO("Screen capture saved: {}", screenPath);
                        std::ifstream file(screenPath, std::ios::binary);
                        if (file) {
                            std::vector<uint8_t> fileData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                            picUploader->writePic2Disk(screenPath, fileData);
                        }
                    }
                }
                if (m_alertPhoneCameraEnable && !m_cameraFrame.empty()) {
                    std::string dateStr, imgStr;
                    getDateAndImgStr(dateStr, imgStr);
                    std::string cameraPath = prefixPathStr + "/camera_phone_" + imgStr + ".jpg";
                    if (cv::imwrite(cameraPath, m_cameraFrame, params)) {
                        MY_SPDLOG_INFO("Camera capture saved: {}", cameraPath);
                        std::ifstream file(cameraPath, std::ios::binary);
                        if (file) {
                            std::vector<uint8_t> fileData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                            picUploader->writePic2Disk(cameraPath, fileData);
                        }
                    }
                }
                break;
            }
            case TEXT_SUSPECT: {
                MY_SPDLOG_INFO("Processing TEXT_SUSPECT alert");
                if (m_alertSuspectScreenEnable && m_scrShot) {
                    m_scrShot->capture(screenBuf.get());
                    std::string dateStr, imgStr;
                    getDateAndImgStr(dateStr, imgStr);
                    std::string screenPath = prefixPathStr + "/screen_suspect_" + imgStr + ".jpg";
                    if (cv::imwrite(screenPath, screenFrame, params)) {
                        MY_SPDLOG_INFO("Screen capture saved: {}", screenPath);
                        std::ifstream file(screenPath, std::ios::binary);
                        if (file) {
                            std::vector<uint8_t> fileData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                            picUploader->writePic2Disk(screenPath, fileData);
                        }
                    }
                }
                if (m_alertSuspectCameraEnable && !m_cameraFrame.empty()) {
                    std::string dateStr, imgStr;
                    getDateAndImgStr(dateStr, imgStr);
                    std::string cameraPath = prefixPathStr + "/camera_suspect_" + imgStr + ".jpg";
                    if (cv::imwrite(cameraPath, m_cameraFrame, params)) {
                        MY_SPDLOG_INFO("Camera capture saved: {}", cameraPath);
                        std::ifstream file(cameraPath, std::ios::binary);
                        if (file) {
                            std::vector<uint8_t> fileData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                            picUploader->writePic2Disk(cameraPath, fileData);
                        }
                    }
                }
                break;
            }
            case TEXT_PEEP: {
                MY_SPDLOG_INFO("Processing TEXT_PEEP alert");
                if (!m_cameraFrame.empty()) {
                    std::string dateStr, imgStr;
                    getDateAndImgStr(dateStr, imgStr);
                    std::string cameraPath = prefixPathStr + "/camera_peep_" + imgStr + ".jpg";
                    if (cv::imwrite(cameraPath, m_cameraFrame, params)) {
                        MY_SPDLOG_INFO("Camera capture saved: {}", cameraPath);
                        std::ifstream file(cameraPath, std::ios::binary);
                        if (file) {
                            std::vector<uint8_t> fileData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                            picUploader->writePic2Disk(cameraPath, fileData);
                        }
                    }
                }
                break;
            }
            case TEXT_NOBODY: {
                MY_SPDLOG_INFO("Processing TEXT_NOBODY alert");
                if (!m_cameraFrame.empty()) {
                    std::string dateStr, imgStr;
                    getDateAndImgStr(dateStr, imgStr);
                    std::string cameraPath = prefixPathStr + "/camera_nobody_" + imgStr + ".jpg";
                    if (cv::imwrite(cameraPath, m_cameraFrame, params)) {
                        MY_SPDLOG_INFO("Camera capture saved: {}", cameraPath);
                        std::ifstream file(cameraPath, std::ios::binary);
                        if (file) {
                            std::vector<uint8_t> fileData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                            picUploader->writePic2Disk(cameraPath, fileData);
                        }
                    }
                }
                break;
            }
            case TEXT_OCCLUDE: {
                MY_SPDLOG_INFO("Processing TEXT_OCCLUDE alert");
                if (!m_cameraFrame.empty()) {
                    std::string dateStr, imgStr;
                    getDateAndImgStr(dateStr, imgStr);
                    std::string cameraPath = prefixPathStr + "/camera_occlude_" + imgStr + ".jpg";
                    if (cv::imwrite(cameraPath, m_cameraFrame, params)) {
                        MY_SPDLOG_INFO("Camera capture saved: {}", cameraPath);
                        std::ifstream file(cameraPath, std::ios::binary);
                        if (file) {
                            std::vector<uint8_t> fileData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                            picUploader->writePic2Disk(cameraPath, fileData);
                        }
                    }
                }
                break;
            }
            case TEXT_NOCONNECT: {
                MY_SPDLOG_INFO("Processing TEXT_NOCONNECT alert");
                if (m_scrShot) {
                    m_scrShot->capture(screenBuf.get());
                    std::string dateStr, imgStr;
                    getDateAndImgStr(dateStr, imgStr);
                    std::string screenPath = prefixPathStr + "/screen_noconnect_" + imgStr + ".jpg";
                    if (cv::imwrite(screenPath, screenFrame, params)) {
                        MY_SPDLOG_INFO("Screen capture saved: {}", screenPath);
                        std::ifstream file(screenPath, std::ios::binary);
                        if (file) {
                            std::vector<uint8_t> fileData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                            picUploader->writePic2Disk(screenPath, fileData);
                        }
                    }
                }
                break;
            }
            default:
                MY_SPDLOG_WARN("Unknown alert mode: {}", m_lastAlertMode);
                break;
        }
        
        // 通知SwiftUI显示告警窗口
        MY_SPDLOG_DEBUG("Alert event processed, notifying SwiftUI");
    }
    
    // 清理资源
    picUploader->stop();
    MY_SPDLOG_INFO("<<<");
}

bool ImageProcessor::openCameraOnce(int32_t /* cameraId */) {
    auto beforeTime = std::chrono::steady_clock::now();
    m_cap.reset(new cv::VideoCapture(m_cameraId, cv::CAP_AVFOUNDATION));
    if (m_cap->isOpened()) {
        m_cap->set(cv::CAP_PROP_FRAME_WIDTH, m_cameraWidth);
        m_cap->set(cv::CAP_PROP_FRAME_HEIGHT, m_cameraHeight);
        auto afterTime = std::chrono::steady_clock::now();
        double duration_millsecond = std::chrono::duration<double, std::milli>(afterTime - beforeTime).count();
        MY_SPDLOG_ERROR("device: {} open success, spend: {} ms", m_cameraId, duration_millsecond);
        return true;
    }
    MY_SPDLOG_ERROR("device: {} open failed", m_cameraId);
    return false;
}

bool ImageProcessor::openVideoOnce()
{
    m_cap.reset(new cv::VideoCapture(m_testVideoPath));
    if (m_cap->isOpened()) {
        m_cap->set(cv::CAP_PROP_FRAME_WIDTH, m_cameraWidth);
        m_cap->set(cv::CAP_PROP_FRAME_HEIGHT, m_cameraHeight);
        return true;
    }
    return false;
}

bool ImageProcessor::openCameraUntilTrue() {
#ifdef __APPLE__
    MY_SPDLOG_INFO("Starting camera initialization on macOS");
    
    // 检查摄像头权限
    AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
    MY_SPDLOG_INFO("Camera permission status: {}", (int)status);
    
    if (status != AVAuthorizationStatusAuthorized) {
        MY_SPDLOG_ERROR("Camera permission not granted. Status: {}", (int)status);
        if (status == AVAuthorizationStatusNotDetermined) {
            MY_SPDLOG_INFO("Requesting camera permission...");
            // 同步等待权限请求结果
            dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
            __block BOOL permissionGranted = NO;
            [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL granted) {
                permissionGranted = granted;
                if (granted) {
                    MY_SPDLOG_INFO("Camera permission granted");
                } else {
                    MY_SPDLOG_ERROR("Camera permission denied by user");
                }
                dispatch_semaphore_signal(semaphore);
            }];
            dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
            dispatch_release(semaphore);
            
            if (!permissionGranted) {
                return false;
            }
        } else {
            MY_SPDLOG_ERROR("Camera permission denied or restricted");
            return false;
        }
    } else {
        MY_SPDLOG_INFO("Camera permission already granted");
    }
    
    // 枚举可用的摄像头设备
    MY_SPDLOG_INFO("Enumerating available camera devices...");
    std::vector<std::string> deviceUniqueIDs;
    std::vector<std::string> deviceNames = getCameraDeviceNames(deviceUniqueIDs);
    
    MY_SPDLOG_INFO("System detected {} camera devices:", deviceNames.size());
    for (size_t i = 0; i < deviceNames.size(); i++) {
        MY_SPDLOG_INFO("  Device {}: {} (uniqueID: {})", i, deviceNames[i], deviceUniqueIDs[i]);
    }
    
    MY_SPDLOG_INFO("Total available camera devices: {}", deviceNames.size());
    
    if (deviceUniqueIDs.empty()) {
        MY_SPDLOG_ERROR("No camera devices found! This may indicate a permission or hardware issue.");
        return false;
    }
    
    // 查找指定的摄像头uniqueID
    int32_t deviceIndex = 0; // 默认使用第一个设备
    bool foundDevice = false;
    
    for (size_t i = 0; i < deviceUniqueIDs.size(); i++) {
        if (deviceUniqueIDs[i] == m_cameraId) {
            deviceIndex = static_cast<int32_t>(i);
            foundDevice = true;
            MY_SPDLOG_INFO("Found camera with uniqueID: {} at index: {}", m_cameraId, deviceIndex);
            break;
        }
    }
    
    if (!foundDevice) {
        MY_SPDLOG_WARN("Requested camera uniqueID {} not found, using default device index: {}", m_cameraId, deviceIndex);
    }
    
    MY_SPDLOG_INFO("Using camera device index: {} for uniqueID: {}", deviceIndex, m_cameraId);
#endif

    while (true) {
        if (m_cap) { m_cap.reset(); }
        auto beforeTime = std::chrono::steady_clock::now();
        MY_SPDLOG_ERROR("device index: {} (uniqueID: {}) try open camera", deviceIndex, m_cameraId);
        m_cap.reset(new cv::VideoCapture(deviceIndex, cv::CAP_AVFOUNDATION));
        if (m_cap->isOpened()) {
            m_cap->set(cv::CAP_PROP_FRAME_WIDTH, m_cameraWidth);
            m_cap->set(cv::CAP_PROP_FRAME_HEIGHT, m_cameraHeight);
            auto afterTime = std::chrono::steady_clock::now();
            double duration_millsecond = std::chrono::duration<double, std::milli>(afterTime - beforeTime).count();
            MY_SPDLOG_ERROR("device index: {} (uniqueID: {}) open success, spend: {} ms", deviceIndex, m_cameraId, duration_millsecond);
            return true;
        }

        // 摄像头打开失败，记录详细错误信息
        MY_SPDLOG_ERROR("Failed to open camera device index: {} (uniqueID: {}) after permission check and device enumeration", deviceIndex, m_cameraId);
        MY_SPDLOG_ERROR("This may indicate a hardware issue or the camera is being used by another application");
        
        // open camera failed - AlertWindowManager已删除，改用简单常量
        const int TEXT_NOCONNECT = 6;
        const int COUNT = 0;
        if (m_alertNoconnectEnable) {
            if (m_lastAlertMode != TEXT_NOCONNECT) {
                {
                    std::unique_lock<std::mutex> lock(m_alertMtx);
                    m_alertTaskVec.emplace_back(TEXT_NOCONNECT);
                }
            }
        }
        else {
            if (m_lastAlertMode != COUNT) {
                {
                    std::unique_lock<std::mutex> lock(m_alertMtx);
                    m_alertTaskVec.emplace_back(COUNT);
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(m_alertShowInterval));
    }
    return false;
}

void ImageProcessor::saveMatWithEncode(cv::Mat& inMat, const std::string& inFilePath, const std::vector<int>& encParam, bool isSuspected)
{

    std::vector<uint8_t> jpg_buffer;
    if (!cv::imencode(".jpg", inMat, jpg_buffer, encParam)) {
        MY_SPDLOG_ERROR("encode {} jpg failed", inFilePath.c_str());
        return;
    }

    size_t pos = inFilePath.find("/");
    if (pos != std::string::npos) {
        std::string result = inFilePath.substr(pos + 1);
        std::string filePathBase64Enc = CommonUtils::Base64::encode(result);
        const uint32_t headerLen = htonl(filePathBase64Enc.size());

        // 添加疑似标志(1字节)
        uint8_t suspectedFlag = isSuspected ? 1 : 0;

        std::vector<uint8_t> final_data;
        final_data.reserve(sizeof(headerLen) + filePathBase64Enc.size() + sizeof(suspectedFlag) + jpg_buffer.size());

        // 添加头部长度
        const uchar* len_ptr = reinterpret_cast<const uchar*>(&headerLen);
        final_data.insert(final_data.end(), len_ptr, len_ptr + sizeof(uint32_t));

        // 添加Base64编码的文件路径
        final_data.insert(final_data.end(), filePathBase64Enc.begin(), filePathBase64Enc.end());

        // 添加疑似标志
        final_data.push_back(suspectedFlag);

        // 添加JPEG原始数据
        final_data.insert(final_data.end(), jpg_buffer.begin(), jpg_buffer.end());

        PicFileUploader* picUploader = PicFileUploader::getInstance();
        picUploader->writePic2Disk(inFilePath, final_data);
    }
}

void ImageProcessor::saveRiskEventFile(const std::string& fileName,
    const std::string& eventName, const std::string& eventTime) {
    try {
        std::ofstream outFile(fileName);
        if(outFile.is_open()) {
            outFile << "EVENT_" << eventName.c_str() << eventTime.c_str() << "\n";
            outFile.close();
        }
    } catch (const std::exception &e) {
        MY_SPDLOG_ERROR("save risk event file exception: {}", e.what());
    }
}

void ImageProcessor::handleNoFaceLock() {
    if (!m_alertNobodyEnable && !m_alertOcculeEnable) return;

    // 锁屏处理
    if (m_alertNobodyLockEnable) {
        if (!m_isNoFaceTiming) {
            m_noFaceStartTime = std::chrono::steady_clock::now();
            MY_SPDLOG_DEBUG("No face lock time begin");
            m_isNoFaceTiming = true;
        }
        else {
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - m_noFaceStartTime);
            MY_SPDLOG_DEBUG("No face duration: {} ms", duration_ms.count());

            if (duration_ms.count() >= m_noFaceLockTimeout) {
                MY_SPDLOG_INFO("No face timeout reached, triggering screen lock");
                // macOS锁屏功能
                system("pmset displaysleepnow");
                m_isNoFaceTiming = false; // 重置计时状态
            }
        }
    }
}

void ImageProcessor::processWindowsMessages() {
#if PLATFORM_WINDOWS
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
#endif
}

void ImageProcessor::writeTestDataToJson() {
    Json::Value root;

    root["detNobodyCnt"] = Json::Value::UInt64(m_detNobodyCnt);
    root["detPeepCnt"] = Json::Value::UInt64(m_detPeepCnt);
    root["detPhoneCnt"] = Json::Value::UInt64(m_detPhoneCnt);

    Json::StreamWriterBuilder writer;
    writer["indentation"] = "  ";

    try {
        CommonUtils::FileHelper::writeStrToFile("test.json", Json::writeString(writer, root));
    }
    catch (const std::exception &e) {
        MY_SPDLOG_WARN("write test json to test.json exception: {}", e.what());
    }
}

void ImageProcessor::onConfigUpdated(std::shared_ptr<MyMeta>& newMeta) {
    setDetectParam(newMeta);
}

void ImageProcessor::setDetectParam(const std::shared_ptr<MyMeta>& meta) {
    if (!m_isCfgListReg) {
        ConfigParser* cfg = ConfigParser::getInstance();
        cfg->registerListener("imageProcessSettings", this);
        m_isCfgListReg = true;
    }

    {        
        std::unique_lock<std::shared_mutex> writeLock(m_paramMtx);

        m_capInterval = meta->getInt32OrDefault("detect_interval", m_capInterval);
        m_alertShowInterval = meta->getInt32OrDefault("alert_show_interval", m_alertShowInterval);

        // 手机检测开关
        m_alertPhoneEnable = meta->getBoolOrDefault("alert_phone_enable", m_alertPhoneEnable);
        m_alertPhoneWindowEnable = meta->getBoolOrDefault("alert_phone_window_enable", m_alertPhoneWindowEnable);
        m_alertPhoneScreenEnable = meta->getBoolOrDefault("alert_phone_screen_enable", m_alertPhoneScreenEnable);
        m_alertPhoneCameraEnable = meta->getBoolOrDefault("alert_phone_camera_enable", m_alertPhoneCameraEnable);

        // 可疑检测开关
        m_alertSuspectEnable = meta->getBoolOrDefault("alert_suspect_enable", m_alertSuspectEnable);
        m_alertSuspectScreenEnable = meta->getBoolOrDefault("alert_suspect_screen_enable", m_alertSuspectScreenEnable);
        m_alertSuspectCameraEnable = meta->getBoolOrDefault("alert_suspect_camera_enable", m_alertSuspectCameraEnable);

        // 偷窥检测开关
        m_alertPeepEnable = meta->getBoolOrDefault("alert_peep_enable", m_alertPeepEnable);
        m_alertPeepWindowEnable = meta->getBoolOrDefault("alert_peep_window_enable", m_alertPeepWindowEnable);

        // 无人检测开关
        m_alertNobodyEnable = meta->getBoolOrDefault("alert_nobody_enable", m_alertNobodyEnable);
        m_alertNobodyWindowEnable = meta->getBoolOrDefault("alert_nobody_window_enable", m_alertNobodyWindowEnable);
        m_alertNobodyLockEnable = meta->getBoolOrDefault("alert_nobody_lock_enable", m_alertNobodyLockEnable);
        // occlude detect switch
        m_alertOcculeEnable = meta->getBoolOrDefault("alert_occlude_enable", m_alertOcculeEnable);
        m_alertOccludeWindowEnable = meta->getBoolOrDefault("alert_occlude_window_enable", m_alertOccludeWindowEnable);
        m_brightnessThresholdLow = meta->getDoubleOrDefault("brightness_threshold_low", m_brightnessThresholdLow);
        m_brightnessThresholdHigh = meta->getDoubleOrDefault("brightness_threshold_high", m_brightnessThresholdHigh);
        // 断连检测开关
        m_alertNoconnectEnable = meta->getBoolOrDefault("alert_noconnect_enable", m_alertNoconnectEnable);
        m_alertNoconnectWindowEnable = meta->getBoolOrDefault("alert_noconnect_window_enable", m_alertNoconnectWindowEnable);
    }
    // 日志输出保持不变
    MY_SPDLOG_DEBUG("配置更新: \n"
              "cap_interval={}, alert_interval={}, \n"
              "phone_en={}, phone_win={}, phone_scr={}, phone_cam={}, \n"
              "suspect_en={}, suspect_scr={}, suspect_cam={}, \n"
              "peep_en={}, peep_win={}, \n"
              "nobody_en={}, nobody_win={}, nobody_lock={}, \n"
              "occlude_en={}, occlude_win={}, \n"
              "bri_low={}, bri_hight={}, \n"
              "noconnect_en={}, noconnect_win={}",

              // 第一行：基础参数 (2个)
              m_capInterval, m_alertShowInterval,

              // 第二行：手机检测开关 (4个)
              m_alertPhoneEnable, m_alertPhoneWindowEnable,
              m_alertPhoneScreenEnable, m_alertPhoneCameraEnable,

              // 第三行：可疑检测开关 (3个)
              m_alertSuspectEnable,
              m_alertSuspectScreenEnable, m_alertSuspectCameraEnable,

              // 第四行：偷窥检测开关 (2个)
              m_alertPeepEnable, m_alertPeepWindowEnable,

              // 第五行：无人检测开关 (3个)
              m_alertNobodyEnable,
              m_alertNobodyWindowEnable, m_alertNobodyLockEnable,

              // occlude swich (2)
              m_alertOcculeEnable, m_alertOccludeWindowEnable,
              // occlude threadhold of brightness
              m_brightnessThresholdLow, m_brightnessThresholdHigh,

              // 断连检测开关 (2个)
              m_alertNoconnectEnable, m_alertNoconnectWindowEnable);
}

void ImageProcessor::setTestParam(const std::shared_ptr<MyMeta>& meta) {
    // 使用类型安全的默认值获取方法
    m_testSourcePreview = meta->getBoolOrDefault("test_source_preview", m_testSourcePreview);
    m_testVideoPath = meta->getStringOrDefault("test_video_path", m_testVideoPath);

    // 日志输出保持不变
    MY_SPDLOG_DEBUG("测试参数更新: m_testSourcePreview={}, m_testVideoPath='{}'",
                   m_testSourcePreview, m_testVideoPath);
}

void ImageProcessor::setNoFaceLockEnabled(bool enabled) {
    std::unique_lock<std::shared_mutex> writeLock(m_paramMtx);
    m_alertNobodyLockEnable = enabled;
    MY_SPDLOG_DEBUG("No face lock enabled: {}", enabled);
}

void ImageProcessor::setNoFaceLockTimeout(int32_t timeoutMs) {
    std::unique_lock<std::shared_mutex> writeLock(m_paramMtx);
    m_noFaceLockTimeout = timeoutMs;
    MY_SPDLOG_DEBUG("No face lock timeout set to: {} ms", timeoutMs);
}

bool ImageProcessor::isCameraOccludedByTraditional(cv::InputArray frame) {
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    // STEP 1: 超快速中心亮度检测 (3μs)
    const int grid_w = gray.cols / 4;
    const int grid_h = gray.rows / 4;
    const int x1 = grid_w, x2 = grid_w * 2;
    const int y1 = grid_h, y2 = grid_h * 2;

    // 检测1：中心区域过暗 → 直接判定遮挡
    double center_brightness =
        cv::mean(gray(cv::Rect(x1, y1, grid_w, grid_h)))[0] +
        cv::mean(gray(cv::Rect(x2, y1, grid_w, grid_h)))[0] +
        cv::mean(gray(cv::Rect(x1, y2, grid_w, grid_h)))[0] +
        cv::mean(gray(cv::Rect(x2, y2, grid_w, grid_h)))[0];
    center_brightness /= 4.0;

    if (center_brightness < m_brightnessThresholdLow) {
        MY_SPDLOG_DEBUG("Occluded: center too dark {:.1f} < {}", center_brightness, m_brightnessThresholdLow);
        return true;  // 3μs内完成判定
    }

    // STEP 2: 边缘检测 (仅90%场景需要)
    cv::Mat edges;
    cv::Canny(gray, edges, 50, 150);
    const double edgeRatio = cv::countNonZero(edges) / static_cast<double>(gray.total());

    // 检测2：边缘比例正常 → 通过
    constexpr double EDGE_RATIO_THRESH = 0.01;
    if (edgeRatio >= EDGE_RATIO_THRESH) {
        return false;
    }

    // 检测3：边缘少但中心明亮 → 不是遮挡
    if (center_brightness > m_brightnessThresholdHigh) {
        // MY_SPDLOG_DEBUG("Not occluded: center bright {:.1f} > {}",center_brightness, BRIGHT_THRESH);
        return false;
    }

    MY_SPDLOG_DEBUG("Occluded: low edges {:.4f} and medium center {:.1f}",
                    edgeRatio, center_brightness);
    return true;
}
