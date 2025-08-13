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

#ifndef IMAGEPROCESSOR_H
#define IMAGEPROCESSOR_H

#include <thread>
#include <queue>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <atomic>
#include <filesystem>
#include <opencv2/opencv.hpp>

#include "ScreenShot.hpp"
// #include "AlertWindowManager.h" // 已删除，改用事件机制与SwiftUI通信
#include "PicFileUploader.h"
#include "MyMeta.h"
#include "ConfigParser.h"



class ImageProcessor : public IConfigUpdateListener {
public:
    ImageProcessor();
    ImageProcessor(int32_t capInterval, int32_t cameraId,
        int32_t cameraWidth, int32_t cameraHeight);
    ~ImageProcessor();
    void prepare();
    void start();
    void stop();

    void setAlertEnables(const bool, const bool, const bool, const bool, const bool);
    void setTestConfigs(const bool testSourcePreview, const std::string &testVideoPath);
    bool getWorkThreadStatus() const;
    void setDetectParam(const std::shared_ptr<MyMeta>& meta);
    void setTestParam(const std::shared_ptr<MyMeta>& meta);
    bool isCameraOccludedByTraditional(cv::InputArray frame);
    
    // 获取告警开关状态的方法
    bool getAlertPhoneEnabled() const { return m_alertPhoneEnable; }
    bool getAlertPeepEnabled() const { return m_alertPeepEnable; }
    bool getAlertSuspectEnabled() const { return m_alertSuspectEnable; }
    bool getAlertNobodyEnabled() const { return m_alertNobodyEnable; }
    bool getAlertOccludeEnabled() const { return m_alertOcculeEnable; }
    bool getAlertNoconnectEnabled() const { return m_alertNoconnectEnable; }
    
    // 设置单个告警开关状态的方法
    void setAlertPhoneEnabled(bool enabled) { m_alertPhoneEnable = enabled; }
    void setAlertPeepEnabled(bool enabled) { m_alertPeepEnable = enabled; }
    void setAlertSuspectEnabled(bool enabled) { m_alertSuspectEnable = enabled; }
    void setAlertNobodyEnabled(bool enabled) { m_alertNobodyEnable = enabled; }
    void setAlertOccludeEnabled(bool enabled) { m_alertOcculeEnable = enabled; }
    void setAlertNoconnectEnabled(bool enabled) { m_alertNoconnectEnable = enabled; }
    
    // 锁屏相关方法
    void setNoFaceLockEnabled(bool enabled);
    void setNoFaceLockTimeout(int32_t timeoutMs);
private:
    void work();
    void alertWork();
    bool openCameraOnce(int32_t cameraId = 0);
    bool openVideoOnce();
    bool openCameraUntilTrue();
    void saveMatWithEncode(cv::Mat& inMat, const std::string& inFilePath, const std::vector<int>& encParam,
        bool isSuspected);
    void saveRiskEventFile(const std::string &fileName, const std::string &eventName, const std::string &eventTime);
    void handleNoFaceLock();
    void processWindowsMessages();
    void writeTestDataToJson();
    void onConfigUpdated(std::shared_ptr<MyMeta>& newMeta);
    



#if ENABLE_REMOTE_SERVER
    void largeModeDetect();
#endif
    std::thread m_thread;
    std::atomic_bool m_continue{ false };
    std::atomic<bool> m_workThreadStatus{ true };
    // show alert thread
    std::atomic_bool m_alertContinue { false };
    std::thread m_alertThd;
    std::mutex m_alertMtx;
    mutable std::shared_mutex m_paramMtx;
    // AlertWindowManager相关成员变量已删除，改用事件机制
    std::vector<int> m_alertTaskVec;  // 改用int类型存储alert类型
    int m_lastAlertMode;              // 改用int类型
    // other variable
    std::unique_ptr<cv::VideoCapture> m_cap{ nullptr };
    cv::Mat m_cameraFrame;
    std::unique_ptr<ScreenShot> m_scrShot{ nullptr };

    int32_t m_capInterval{ 300 };
    int32_t m_alertShowInterval{ 500 };
    int32_t m_cameraId{ 0 };
    int32_t m_cameraWidth{ 640 };
    int32_t m_cameraHeight{ 640 };

    std::string m_cameraName{ "" };
    std::string m_testVideoPath{ "" };

    bool m_testSourcePreview{ false };

    bool m_alertPhoneEnable{ false };
    bool m_alertPhoneWindowEnable{ false };
    bool m_alertPhoneScreenEnable{ false };
    bool m_alertPhoneCameraEnable{ false };

    bool m_alertSuspectEnable{ false };
    bool m_alertSuspectScreenEnable{ false };
    bool m_alertSuspectCameraEnable{ false };

    bool m_alertPeepEnable{ false };
    bool m_alertPeepWindowEnable{ false };

    bool m_alertNobodyEnable{ false };
    bool m_alertOcculeEnable{ false };
    bool m_alertNobodyWindowEnable{ false };
    bool m_alertOccludeWindowEnable{ false };
    bool m_alertNobodyLockEnable{ false };
    bool m_isNoFaceTiming{ false };

    bool m_alertNoconnectEnable{ false };
    bool m_alertNoconnectWindowEnable{ false };

    std::chrono::steady_clock::time_point m_noFaceStartTime;
    int32_t m_noFaceLockTimeout{ 5000 }; // 默认5秒锁屏

    uint8_t m_detNobodyFrameCnt{ 0 };
    uint64_t m_detOcclude{ 0 };
    uint64_t m_detNobodyCnt{ 0 };
    uint64_t m_detPeepCnt{ 0 };
    uint64_t m_detPhoneCnt{ 0 };
    double m_brightnessThresholdLow = 30.01;
    double m_brightnessThresholdHigh = 150.01;
};

#endif // IMAGEPROCESSOR_H
