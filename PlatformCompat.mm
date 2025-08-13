#include "PlatformCompat.h"
#include <iostream>

#include <CoreFoundation/CoreFoundation.h>
#include <AppKit/AppKit.h>
#include <AVFoundation/AVFoundation.h>
#include <sys/utsname.h>

namespace PlatformCompat {

void ShowMessageBox(const std::string& message, const std::string& title) {
    // 使用NSAlert显示消息框
    NSAlert* alert = [[NSAlert alloc] init];
    [alert setMessageText:[NSString stringWithUTF8String:title.c_str()]];
    [alert setInformativeText:[NSString stringWithUTF8String:message.c_str()]];
    [alert setAlertStyle:NSAlertStyleCritical];
    [alert runModal];
}

bool IsSystemLocked() {
    // macOS实现 - 检查屏幕保护程序状态
    CFDictionaryRef sessionDict = CGSessionCopyCurrentDictionary();
    if (sessionDict) {
        CFBooleanRef isLocked = (CFBooleanRef)CFDictionaryGetValue(sessionDict, kCGSessionOnConsoleKey);
        bool locked = !CFBooleanGetValue(isLocked);
        CFRelease(sessionDict);
        return locked;
    }
    return false;
}

bool SetDpiAwareness(std::string& errorMsg) {
    // macOS自动处理DPI，无需特殊设置
    return true;
}

std::string GetSystemVersion() {
    struct utsname systemInfo;
    if (uname(&systemInfo) == 0) {
        return std::string("macOS ") + systemInfo.release;
    }
    return "macOS Unknown";
}

std::string GetProgramDataPath() {
    return std::string(getenv("HOME")) + "/Library/Application Support/PADetect/";
}

void ProcessMessages() {
    // macOS事件处理
    NSEvent* event;
    while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                       untilDate:[NSDate distantPast]
                                          inMode:NSDefaultRunLoopMode
                                         dequeue:YES])) {
        [NSApp sendEvent:event];
    }
}

} // namespace PlatformCompat