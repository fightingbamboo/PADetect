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
#ifndef ALERT_WINDOW_NAMAGER_H
#define ALERT_WINDOW_NAMAGER_H

#define NOMINMAX
#include <windows.h>
#include <algorithm>
namespace Gdiplus
{
    using std::min;
    using std::max;
};
#include <Gdiplus.h>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <memory>

#include "MyMeta.h"
#include "ConfigParser.h"

#pragma comment(lib, "gdiplus.lib")

class AlertWindowManager : public IConfigUpdateListener {
public:
    enum class ALERT_MODE {
        TEXT_PHONE = 0,
        TEXT_PEEP,
        TEXT_NOBODY,
        TEXT_OCCLUDE,
        TEXT_NOCONNECT,
        TEXT_SUSPECT,
        COUNT,
    };

public:
    static AlertWindowManager* getInstance() {
        static AlertWindowManager instance;
        return &instance;
    }

    bool initWind();
    void deinitWind();
    void initGDIPlus();
    void deinitGDIPlus();
    void showAlert();
    void hideAlert();
    void setAlertVersion(const std::string&);
    void setAlertShowMode(ALERT_MODE alertMode);
    void setAlertParam(std::shared_ptr<MyMeta> &meta);
    bool isShow() const;
#if 0
    void startThread();
    void stopThread();
#endif
private:
    AlertWindowManager();
    ~AlertWindowManager();
    AlertWindowManager(const AlertWindowManager&) = delete;
    AlertWindowManager& operator=(const AlertWindowManager&) = delete;
#if 0
    void mainThd();
#endif
    HFONT CreateCompatibleFont(HDC hdc);
    int GetScaledFontSize(int baseSize);
    int GetSystemDPI();
    void onConfigUpdated(std::shared_ptr<MyMeta>& newMeta);

private:
    std::vector<HWND> mAlertWindows;  // Store handles to windows
    HINSTANCE mHInstance;
    WNDCLASSEXW mWc;

    // alert string
    std::wstring mAlertFont{ L"微软雅黑" };
    int32_t mAlertFontSize{ 60 };
    
    //thread relate
#if 0
    std::atomic_bool mContinue;
    std::unique_ptr<std::thread> mMainThd;
    std::mutex mMtx;
    std::condition_variable mCond;
#endif
    mutable std::shared_mutex m_paramMtx;

    std::atomic<bool> mIsShow{ false };
    std::atomic<ALERT_MODE> mAlertMode{ ALERT_MODE::TEXT_PHONE };

    std::wstring m_alertStrPhone{ L"禁止 拍照" };
    std::wstring m_alertStrPeep{ L"存在偷窥风险 请检测周边" };
    std::wstring m_alertStrNobody{ L"无人办公" };
    std::wstring m_alertStrOcclude{ L"摄像头遮挡" };
    std::wstring m_alertStrNoconnect{ L"摄像头异常 请检查线束连接" };
    std::wstring m_alertVersion{ L"" };

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static BOOL CALLBACK MonitorEnumProc(HMONITOR, HDC, LPRECT lprcMonitor, LPARAM dwData);
    static bool m_s_classRegistered;
};

#endif
