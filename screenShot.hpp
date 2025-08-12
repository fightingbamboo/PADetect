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

#ifndef SCREEN_SHOT_H
#define SCREEN_SHOT_H


#include <iostream>
#include <vector>
#include <cstdint>

#if defined (_WIN32)
#define NOMINMAX
#include <windows.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#elif defined(__APPLE__)
#include <ApplicationServices/ApplicationServices.h>
#else
#error Unsupported platform
#endif

class ScreenShot {
public:
    virtual ~ScreenShot() {}
    virtual bool init() = 0;
    virtual void capture(uint8_t* buffer) = 0;
    virtual void deinit() = 0;
    virtual void getScreenResolution(int& width, int& height) = 0; // Added to get resolution
};

#ifdef _WIN32
class ScreenShotWindows : public ScreenShot {
private:
    HDC hdcScreen; // 屏幕设备上下文
    int physicalWidth; // 当前显示器物理宽度
    int physicalHeight; // 当前显示器物理高度
    RECT monitorRect; // 当前显示器屏幕矩形（用于定位）
    bool isMultiMonitor; // 是否多显示器环境

    // 获取当前活动显示器的物理分辨率
    bool getActiveMonitorInfo() {
        // 获取当前活动窗口
        HWND hWnd = GetForegroundWindow();
        if (!hWnd) {
            // 如果没有活动窗口，使用鼠标位置
            POINT cursorPos;
            if (GetCursorPos(&cursorPos)) {
                hWnd = WindowFromPoint(cursorPos);
            }
        }

        HMONITOR hMonitor;
        if (hWnd) {
            hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
        }
        else {
            // 没有活动窗口时，使用鼠标位置
            POINT pt;
            GetCursorPos(&pt);
            hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
        }

        if (!hMonitor) {
            hMonitor = MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
        }

        // 获取显示器信息
        MONITORINFOEX monitorInfo;
        monitorInfo.cbSize = sizeof(MONITORINFOEX);
        if (!GetMonitorInfo(hMonitor, &monitorInfo)) {
            return false;
        }

        // 保存显示器矩形
        monitorRect = monitorInfo.rcMonitor;

        // 创建设备上下文以获取物理分辨率
        HDC hdcMonitor = CreateDC(monitorInfo.szDevice, NULL, NULL, NULL);
        if (!hdcMonitor) {
            return false;
        }

        physicalWidth = GetDeviceCaps(hdcMonitor, HORZRES);
        physicalHeight = GetDeviceCaps(hdcMonitor, VERTRES);
        DeleteDC(hdcMonitor);

        // 检查是否多显示器环境
        isMultiMonitor = (GetSystemMetrics(SM_CMONITORS) > 1);

        return true;
    }

public:
    bool init() override {
        // 获取当前活动显示器信息
        if (!getActiveMonitorInfo()) {
            // 失败时回退到主显示器
            hdcScreen = GetDC(NULL);
            if (!hdcScreen) {
                return false;
            }

            physicalWidth = GetDeviceCaps(hdcScreen, DESKTOPHORZRES);
            physicalHeight = GetDeviceCaps(hdcScreen, DESKTOPVERTRES);
            monitorRect = { 0, 0, physicalWidth, physicalHeight };
            isMultiMonitor = (GetSystemMetrics(SM_CMONITORS) > 1);
        }
        else {
            hdcScreen = GetDC(NULL);
            if (!hdcScreen) {
                return false;
            }
        }

        return true;
    }

    void capture(uint8_t* buffer) override {
        int width = monitorRect.right - monitorRect.left;
        int height = monitorRect.bottom - monitorRect.top;

        HDC hdcMemory = CreateCompatibleDC(hdcScreen);
        HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
        if (!hBitmap) {
            DeleteDC(hdcMemory);
            return;
        }

        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMemory, hBitmap);

        // 仅捕获当前显示器区域
        BitBlt(hdcMemory, 0, 0, width, height, hdcScreen,
               monitorRect.left, monitorRect.top, SRCCOPY);

        BITMAP bmp;
        GetObject(hBitmap, sizeof(BITMAP), &bmp);
        GetBitmapBits(hBitmap, bmp.bmWidthBytes * bmp.bmHeight, buffer);

        // 恢复并清理
        SelectObject(hdcMemory, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hdcMemory);
    }

    void getScreenResolution(int& width, int& height) override {
        // 返回当前显示器的物理分辨率
        width = physicalWidth;
        height = physicalHeight;
    }

    void deinit() override {
        if (hdcScreen) {
            ReleaseDC(NULL, hdcScreen);
            hdcScreen = NULL;
        }
    }

    // 新增：检查是否多显示器环境
    bool isMultiMonitorSetup() const {
        return isMultiMonitor;
    }

    // 新增：获取显示器索引（用于多显示器场景）
    int getMonitorIndex() const {
        MONITORINFOEX monitorInfo;
        monitorInfo.cbSize = sizeof(MONITORINFOEX);

        HMONITOR hMonitor = MonitorFromRect(&monitorRect, MONITOR_DEFAULTTONULL);
        if (hMonitor && GetMonitorInfo(hMonitor, &monitorInfo)) {
            // 这里简化处理，实际应用中可能需要更复杂的索引逻辑
            return 0; // 实际应用中应返回正确的显示器索引
        }
        return 0;
    }
};
#endif

#ifdef __linux__
class ScreenShotLinux : public ScreenShot {
private:
    Display* display;
    Window root;
    int width;
    int height;
    int screenNum;
    bool isMultiMonitor;

    // 获取当前活动显示器
    bool getActiveMonitorInfo() {
        // 在X11中，获取当前鼠标所在屏幕
        Window root_return, child_return;
        int root_x, root_y, win_x, win_y;
        unsigned int mask_return;

        if (!XQueryPointer(display, DefaultRootWindow(display),
            &root_return, &child_return,
            &root_x, &root_y, &win_x, &win_y,
            &mask_return)) {
            return false;
        }

        // 获取屏幕数量
        int screenCount = ScreenCount(display);
        isMultiMonitor = (screenCount > 1);

        // 简化处理：使用鼠标位置确定屏幕
        // 实际应用中应更精确地计算鼠标所在的屏幕
        for (int i = 0; i < screenCount; i++) {
            Screen* screen = ScreenOfDisplay(display, i);
            if (root_x >= screen->x && root_x < screen->x + screen->width &&
                root_y >= screen->y && root_y < screen->y + screen->height) {
                screenNum = i;
                width = screen->width;
                height = screen->height;
                return true;
            }
        }

        // 默认使用主屏幕
        screenNum = 0;
        width = DisplayWidth(display, screenNum);
        height = DisplayHeight(display, screenNum);
        return true;
    }

public:
    bool init() override {
        display = XOpenDisplay(NULL);
        if (!display) {
            return false;
        }

        root = DefaultRootWindow(display);

        // 尝试获取当前活动显示器
        if (!getActiveMonitorInfo()) {
            // 失败时使用默认屏幕
            screenNum = 0;
            width = DisplayWidth(display, screenNum);
            height = DisplayHeight(display, screenNum);
            isMultiMonitor = (ScreenCount(display) > 1);
        }

        return true;
    }

    void capture(uint8_t* buffer) override {
        XImage* img = XGetImage(display, root, 0, 0, width, height, AllPlanes, ZPixmap);
        if (!img) {
            return;
        }

        // X11 通常返回物理分辨率，但需注意字节顺序
        memcpy(buffer, img->data, width * height * 4); // 假设为RGBA格式
        XDestroyImage(img);
    }

    void getScreenResolution(int& w, int& h) override {
        w = width;
        h = height;
    }

    void deinit() override {
        if (display) {
            XCloseDisplay(display);
            display = NULL;
        }
    }

    bool isMultiMonitorSetup() const {
        return isMultiMonitor;
    }
};
#endif

#ifdef __APPLE__
class ScreenShotMacOs : public ScreenShot {
private:
    int width;
    int height;
    CGDirectDisplayID displayID;
    bool isMultiMonitor;

    // 获取当前活动显示器
    bool getActiveMonitorInfo() {
        // 获取所有显示器
        CGDirectDisplayID mainDisplay = CGMainDisplayID();
        uint32_t displayCount;
        CGGetActiveDisplayList(0, NULL, &displayCount);

        isMultiMonitor = (displayCount > 1);

        // 获取鼠标位置
        CGPoint cursorPos = CGEventGetLocation(CGEventCreate(NULL));

        // 查找鼠标所在的显示器
        for (uint32_t i = 0; i < displayCount; i++) {
            CGDirectDisplayID displays[16];
            CGGetActiveDisplayList(16, displays, &displayCount);

            CGDisplayModeRef displayMode = CGDisplayCopyDisplayMode(displays[i]);
            if (!displayMode) continue;

            CGSize screenSize = CGSizeMake(CGDisplayModeGetWidth(displayMode), CGDisplayModeGetHeight(displayMode));
            CFRelease(displayMode);

            // 获取显示器位置（简化处理）
            // 实际应用中应使用 CGDisplayBounds
            CGRect displayRect = CGRectMake(0, 0, screenSize.width, screenSize.height);
            if (CGRectContainsPoint(displayRect, cursorPos)) {
                displayID = displays[i];
                width = CGDisplayPixelsWide(displayID);
                height = CGDisplayPixelsHigh(displayID);
                return true;
            }
        }

        // 默认使用主显示器
        displayID = mainDisplay;
        width = CGDisplayPixelsWide(displayID);
        height = CGDisplayPixelsHigh(displayID);
        return true;
    }

public:
    bool init() override {
        // 尝试获取当前活动显示器
        if (!getActiveMonitorInfo()) {
            // 失败时使用主显示器
            displayID = CGMainDisplayID();
            width = CGDisplayPixelsWide(displayID);
            height = CGDisplayPixelsHigh(displayID);

            uint32_t displayCount;
            CGGetActiveDisplayList(0, NULL, &displayCount);
            isMultiMonitor = (displayCount > 1);
        }

        return true;
    }

    void capture(uint8_t* buffer) override {
        // 仅捕获当前显示器
        // 注意：CGDisplayCreateImage在macOS 15.0中已废弃，建议使用ScreenCaptureKit
        // 暂时禁用此功能以避免编译错误
        /*
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wunguarded-availability"
        CGImageRef image = CGDisplayCreateImage(displayID);
#pragma clang diagnostic pop
        if (!image) {
            return;
        }

        size_t bytesPerRow = CGImageGetBytesPerRow(image);
        size_t imgHeight = CGImageGetHeight(image);
        size_t totalBytes = bytesPerRow * imgHeight;

        CFDataRef pixelDataRef = CGDataProviderCopyData(CGImageGetDataProvider(image));
        if (pixelDataRef) {
            const uint8_t* pixelData = CFDataGetBytePtr(pixelDataRef);
            if (pixelData) {
                memcpy(buffer, pixelData, totalBytes);
            }
            CFRelease(pixelDataRef);
        }

        CGImageRelease(image);
        */
        // 临时实现：清空缓冲区
        memset(buffer, 0, width * height * 4);
    }

    void getScreenResolution(int& w, int& h) override {
        w = width;
        h = height;
    }

    void deinit() override {
        // macOS 通常不需要特殊清理
    }

    bool isMultiMonitorSetup() const {
        return isMultiMonitor;
    }
};
#endif

#endif // _SCREEN_SHOT_H