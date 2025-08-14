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
#include <AvailabilityMacros.h>
// 检查是否为 macOS 15.0 或更高版本
#if defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 150000
#define USE_LEGACY_CAPTURE 0
#ifdef __OBJC__
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <Foundation/Foundation.h>
#endif
#else
#define USE_LEGACY_CAPTURE 1
#endif
#else
#error Unsupported platform
#endif

class ScreenShot {
public:
    virtual ~ScreenShot() {}
    virtual bool init() = 0;
    virtual void capture(uint8_t* buffer) = 0;
    virtual void deinit() = 0;
    virtual void getScreenResolution(int& width, int& height) = 0;
};

#ifdef _WIN32
class ScreenShotWindows : public ScreenShot {
private:
    HDC hdcScreen;
    int physicalWidth;
    int physicalHeight;
    RECT monitorRect;
    bool isMultiMonitor;

    bool getActiveMonitorInfo() {
        HWND hWnd = GetForegroundWindow();
        if (!hWnd) {
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
            POINT pt;
            GetCursorPos(&pt);
            hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
        }

        if (!hMonitor) {
            hMonitor = MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
        }

        MONITORINFOEX monitorInfo;
        monitorInfo.cbSize = sizeof(MONITORINFOEX);
        if (!GetMonitorInfo(hMonitor, &monitorInfo)) {
            return false;
        }

        monitorRect = monitorInfo.rcMonitor;

        HDC hdcMonitor = CreateDC(monitorInfo.szDevice, NULL, NULL, NULL);
        if (!hdcMonitor) {
            return false;
        }

        physicalWidth = GetDeviceCaps(hdcMonitor, HORZRES);
        physicalHeight = GetDeviceCaps(hdcMonitor, VERTRES);
        DeleteDC(hdcMonitor);

        isMultiMonitor = (GetSystemMetrics(SM_CMONITORS) > 1);
        return true;
    }

public:
    bool init() override {
        if (!getActiveMonitorInfo()) {
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
        BitBlt(hdcMemory, 0, 0, width, height, hdcScreen,
               monitorRect.left, monitorRect.top, SRCCOPY);

        BITMAP bmp;
        GetObject(hBitmap, sizeof(BITMAP), &bmp);
        GetBitmapBits(hBitmap, bmp.bmWidthBytes * bmp.bmHeight, buffer);

        SelectObject(hdcMemory, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hdcMemory);
    }

    void getScreenResolution(int& width, int& height) override {
        width = physicalWidth;
        height = physicalHeight;
    }

    void deinit() override {
        if (hdcScreen) {
            ReleaseDC(NULL, hdcScreen);
            hdcScreen = NULL;
        }
    }

    bool isMultiMonitorSetup() const {
        return isMultiMonitor;
    }

    int getMonitorIndex() const {
        MONITORINFOEX monitorInfo;
        monitorInfo.cbSize = sizeof(MONITORINFOEX);
        HMONITOR hMonitor = MonitorFromRect(&monitorRect, MONITOR_DEFAULTTONULL);
        if (hMonitor && GetMonitorInfo(hMonitor, &monitorInfo)) {
            return 0;
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

    bool getActiveMonitorInfo() {
        Window root_return, child_return;
        int root_x, root_y, win_x, win_y;
        unsigned int mask_return;

        if (!XQueryPointer(display, DefaultRootWindow(display),
            &root_return, &child_return,
            &root_x, &root_y, &win_x, &win_y,
            &mask_return)) {
            return false;
        }

        int screenCount = ScreenCount(display);
        isMultiMonitor = (screenCount > 1);

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
        if (!getActiveMonitorInfo()) {
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
        memcpy(buffer, img->data, width * height * 4);
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
#if !USE_LEGACY_CAPTURE && defined(__OBJC__)
    SCDisplay* currentDisplay;
    SCContentFilter* contentFilter;
    SCStreamConfiguration* streamConfig;
    NSImage* lastCapturedImage;
    dispatch_semaphore_t captureSemaphore;
#endif

    bool getActiveMonitorInfo() {
        CGDirectDisplayID mainDisplay = CGMainDisplayID();
        uint32_t displayCount;
        CGGetActiveDisplayList(0, NULL, &displayCount);
        isMultiMonitor = (displayCount > 1);

        CGPoint cursorPos = CGEventGetLocation(CGEventCreate(NULL));

        for (uint32_t i = 0; i < displayCount; i++) {
            CGDirectDisplayID displays[16];
            CGGetActiveDisplayList(16, displays, &displayCount);

            CGDisplayModeRef displayMode = CGDisplayCopyDisplayMode(displays[i]);
            if (!displayMode) continue;

            CGSize screenSize = CGSizeMake(CGDisplayModeGetWidth(displayMode), CGDisplayModeGetHeight(displayMode));
            CFRelease(displayMode);

            CGRect displayRect = CGRectMake(0, 0, screenSize.width, screenSize.height);
            if (CGRectContainsPoint(displayRect, cursorPos)) {
                displayID = displays[i];
                width = CGDisplayPixelsWide(displayID);
                height = CGDisplayPixelsHigh(displayID);
                return true;
            }
        }

        displayID = mainDisplay;
        width = CGDisplayPixelsWide(displayID);
        height = CGDisplayPixelsHigh(displayID);
        return true;
    }

#if !USE_LEGACY_CAPTURE && defined(__OBJC__)
    bool initScreenCaptureKit();
    void captureWithScreenCaptureKit(uint8_t* buffer);
    void convertCGImageToBuffer(CGImageRef imageRef, uint8_t* buffer);
    bool isScreenCaptureKitAvailable();
#endif

public:
    bool init() override {
        if (!getActiveMonitorInfo()) {
            displayID = CGMainDisplayID();
            width = CGDisplayPixelsWide(displayID);
            height = CGDisplayPixelsHigh(displayID);

            uint32_t displayCount;
            CGGetActiveDisplayList(0, NULL, &displayCount);
            isMultiMonitor = (displayCount > 1);
        }

#if !USE_LEGACY_CAPTURE && defined(__OBJC__)
        if (!isScreenCaptureKitAvailable()) {
            std::cout << "Warning: ScreenCaptureKit not available on this system" << std::endl;
            return false;
        }
        if (!initScreenCaptureKit()) {
            std::cout << "Warning: Failed to initialize ScreenCaptureKit" << std::endl;
            return false;
        }
#endif

        return true;
    }

    void capture(uint8_t* buffer) override {
#if USE_LEGACY_CAPTURE
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wunguarded-availability"
        CGImageRef image = CGDisplayCreateImage(displayID);
#pragma clang diagnostic pop
        
        if (!image) {
            memset(buffer, 0, width * height * 4);
            return;
        }

        size_t bytesPerRow = CGImageGetBytesPerRow(image);
        size_t imgHeight = CGImageGetHeight(image);
        size_t totalBytes = bytesPerRow * imgHeight;

        CFDataRef pixelDataRef = CGDataProviderCopyData(CGImageGetDataProvider(image));
        if (pixelDataRef) {
            const uint8_t* pixelData = CFDataGetBytePtr(pixelDataRef);
            if (pixelData && totalBytes <= (size_t)(width * height * 4)) {
                memcpy(buffer, pixelData, totalBytes);
            } else {
                memset(buffer, 0, width * height * 4);
            }
            CFRelease(pixelDataRef);
        } else {
            memset(buffer, 0, width * height * 4);
        }

        CGImageRelease(image);
#else
#ifdef __OBJC__
        // 对于macOS 15.0+版本，使用ScreenCaptureKit
        captureWithScreenCaptureKit(buffer);
#else
        // 如果不是Objective-C环境，则清空缓冲区
        memset(buffer, 0, width * height * 4);
        std::cout << "Warning: ScreenCaptureKit requires Objective-C environment" << std::endl;
#endif
#endif
    }

    void getScreenResolution(int& w, int& h) override {
        w = width;
        h = height;
    }

    void deinit() override {
#if !USE_LEGACY_CAPTURE && defined(__OBJC__)
        if (captureSemaphore) {
            dispatch_release(captureSemaphore);
            captureSemaphore = nullptr;
        }
        if (lastCapturedImage) {
            [lastCapturedImage release];
            lastCapturedImage = nil;
        }
        if (contentFilter) {
            [contentFilter release];
            contentFilter = nil;
        }
        if (streamConfig) {
            [streamConfig release];
            streamConfig = nil;
        }
        if (currentDisplay) {
            [currentDisplay release];
            currentDisplay = nil;
        }
#endif
    }

    bool isMultiMonitorSetup() const {
        return isMultiMonitor;
    }
};
#endif

#endif // SCREEN_SHOT_H