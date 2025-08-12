#include "PlatformCompat.h"
#include <iostream>
#include <filesystem>

#if PLATFORM_MACOS
#include <CoreFoundation/CoreFoundation.h>
#include <AppKit/AppKit.h>
#include <AVFoundation/AVFoundation.h>
#include <sys/utsname.h>
#endif

namespace PlatformCompat {

void ShowMessageBox(const std::string& message, const std::string& title) {
#if PLATFORM_WINDOWS
    std::wstring wMessage(message.begin(), message.end());
    std::wstring wTitle(title.begin(), title.end());
    MessageBoxW(nullptr, wMessage.c_str(), wTitle.c_str(), MB_OK | MB_ICONERROR);
#elif PLATFORM_MACOS
    // 使用NSAlert显示消息框
    NSAlert* alert = [[NSAlert alloc] init];
    [alert setMessageText:[NSString stringWithUTF8String:title.c_str()]];
    [alert setInformativeText:[NSString stringWithUTF8String:message.c_str()]];
    [alert setAlertStyle:NSAlertStyleCritical];
    [alert runModal];
#else
    // 控制台输出作为fallback
    std::cerr << "[" << title << "] " << message << std::endl;
#endif
}

bool IsSystemLocked() {
#if PLATFORM_WINDOWS
    // Windows实现 - 检查会话状态
    DWORD sessionId = WTSGetActiveConsoleSessionId();
    if (sessionId == 0xFFFFFFFF) {
        return true; // 无活动会话
    }
    
    // 检查桌面切换
    HDESK hDesk = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (hDesk == nullptr) {
        return true; // 无法访问输入桌面，可能被锁定
    }
    CloseDesktop(hDesk);
    return false;
#elif PLATFORM_MACOS
    // macOS实现 - 检查屏幕保护程序状态
    CFDictionaryRef sessionDict = CGSessionCopyCurrentDictionary();
    if (sessionDict) {
        CFBooleanRef isLocked = (CFBooleanRef)CFDictionaryGetValue(sessionDict, kCGSessionOnConsoleKey);
        bool locked = !CFBooleanGetValue(isLocked);
        CFRelease(sessionDict);
        return locked;
    }
    return false;
#else
    return false; // 其他平台默认未锁定
#endif
}

bool SetDpiAwareness(std::string& errorMsg) {
#if PLATFORM_WINDOWS
    HRESULT hr = SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (FAILED(hr)) {
        DWORD error = GetLastError();
        errorMsg = "SetProcessDpiAwarenessContext failed with error code: " + std::to_string(error);
        return false;
    }
    return true;
#elif PLATFORM_MACOS
    // macOS自动处理DPI，无需特殊设置
    return true;
#else
    return true; // 其他平台默认成功
#endif
}

std::string GetSystemVersion() {
#if PLATFORM_WINDOWS
    OSVERSIONINFOW osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOW));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
    
    if (GetVersionExW(&osvi)) {
        return "Windows " + std::to_string(osvi.dwMajorVersion) + "." + std::to_string(osvi.dwMinorVersion);
    }
    return "Windows Unknown";
#elif PLATFORM_MACOS
    struct utsname systemInfo;
    if (uname(&systemInfo) == 0) {
        return std::string("macOS ") + systemInfo.release;
    }
    return "macOS Unknown";
#else
    return "Unknown OS";
#endif
}

std::string GetProgramDataPath() {
#if PLATFORM_WINDOWS
    wchar_t* path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &path))) {
        std::wstring wPath(path);
        CoTaskMemFree(path);
        return std::string(wPath.begin(), wPath.end());
    }
    return "C:\\ProgramData";
#elif PLATFORM_MACOS
    return "/Library/Application Support";
#else
    return "/var/lib";
#endif
}

std::vector<std::string> GetCameraDeviceNames(std::vector<int>& deviceIDs) {
    std::vector<std::string> deviceNames;
    deviceIDs.clear();
    
#if PLATFORM_WINDOWS
    // Windows Media Foundation实现
    // 这里需要完整的Media Foundation代码，暂时返回空列表
    // 实际实现需要枚举视频设备
#elif PLATFORM_MACOS
    @autoreleasepool {
        // 使用AVFoundation枚举摄像头设备
        NSArray<AVCaptureDevice *> *devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
        
        for (NSUInteger i = 0; i < devices.count; i++) {
            AVCaptureDevice *device = devices[i];
            std::string deviceName = [device.localizedName UTF8String];
            deviceNames.push_back(deviceName);
            deviceIDs.push_back((int)i);
        }
        
        // 如果没有找到摄像头，添加默认项
        if (deviceNames.empty()) {
            deviceNames.push_back("No Camera Found");
            deviceIDs.push_back(-1);
        }
    }
#endif
    
    return deviceNames;
}

void ProcessMessages() {
#if PLATFORM_WINDOWS
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
#elif PLATFORM_MACOS
    // macOS事件处理
    NSEvent* event;
    while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                       untilDate:[NSDate distantPast]
                                          inMode:NSDefaultRunLoopMode
                                         dequeue:YES])) {
        [NSApp sendEvent:event];
    }
#endif
}

} // namespace PlatformCompat