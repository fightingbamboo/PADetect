#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

#include <string>
#include <vector>

// 平台检测宏
#ifdef _WIN32
    #define PLATFORM_WINDOWS 1
    #define PLATFORM_MACOS 0
    #define PLATFORM_LINUX 0
#elif defined(__APPLE__)
    #define PLATFORM_WINDOWS 0
    #define PLATFORM_MACOS 1
    #define PLATFORM_LINUX 0
#elif defined(__linux__)
    #define PLATFORM_WINDOWS 0
    #define PLATFORM_MACOS 0
    #define PLATFORM_LINUX 1
#else
    #define PLATFORM_WINDOWS 0
    #define PLATFORM_MACOS 0
    #define PLATFORM_LINUX 0
#endif

// Windows特定包含
#if PLATFORM_WINDOWS
    #define NOMINMAX
    #include <Windows.h>
    #include <iphlpapi.h>
    #include <shcore.h>
    #include <shellscalingapi.h>
    #include <wtsapi32.h>
    #include <mfapi.h>
    #include <shlobj.h>
    #include <mfreadwrite.h>
    #include <Gdiplus.h>
    #pragma comment(lib, "iphlpapi.lib")
    #pragma comment(lib, "shcore.lib")
    #pragma comment(lib, "wtsapi32.lib")
    #pragma comment(lib, "mfplat.lib")
    #pragma comment(lib, "shell32.lib")
    #pragma comment(lib, "gdiplus.lib")
#else
    // 非Windows平台的简化定义
    #define MB_OK 0x00000000L
    #define MB_ICONERROR 0x00000010L
    #define MB_ICONWARNING 0x00000030L
    #define MB_ICONINFORMATION 0x00000040L
    #define PLATFORM_TRUE 1
    #define PLATFORM_FALSE 0
    
    typedef int PLATFORM_BOOL;
#endif

// macOS特定包含 (仅在需要时包含)
#if PLATFORM_MACOS
    // 注意: AppKit/AVFoundation 等 Objective-C 框架不应在 C++ 头文件中包含
    // 如需使用，请在具体的 .mm 文件中包含
#endif

// Linux特定包含
#if PLATFORM_LINUX
    #include <X11/Xlib.h>
    #include <gtk/gtk.h>
#endif

// 跨平台类型定义
#if !PLATFORM_WINDOWS
    // 为非Windows平台定义Windows类型
    typedef void* HWND;
    typedef void* HINSTANCE;
    typedef void* HDC;
    typedef void* HFONT;
    typedef void* HANDLE;
    typedef unsigned int UINT;
    typedef long LONG;
    typedef unsigned long DWORD;
    typedef int PLATFORM_BOOL;
    typedef unsigned short WORD;
    typedef unsigned char BYTE;
    typedef long long LONG_PTR;
    typedef unsigned long long WPARAM;
    typedef long long LPARAM;
    typedef long LRESULT;
    
    // Windows常量定义
    #define PLATFORM_TRUE 1
    #define PLATFORM_FALSE 0
    #define MB_OK 0x00000000L
    #define MB_ICONERROR 0x00000010L
    #define SW_SHOW 5
    #define SW_HIDE 0
    #define PM_REMOVE 0x0001
    #define WM_PAINT 0x000F
    #define WM_CREATE 0x0001
    #define WM_DESTROY 0x0002
    #define WM_KEYDOWN 0x0100
    #define WM_KEYUP 0x0101
    #define WM_CHAR 0x0102
    #define WM_SYSKEYDOWN 0x0104
    #define WM_SYSKEYUP 0x0105
    #define WM_LBUTTONDOWN 0x0201
    #define WM_LBUTTONUP 0x0202
    #define WM_RBUTTONDOWN 0x0204
    #define WM_RBUTTONUP 0x0205
    #define WM_MOUSEMOVE 0x0200
    #define WM_MOUSEWHEEL 0x020A
    #define WM_MOUSEHWHEEL 0x020E
    #define WM_SETCURSOR 0x0020
    #define WM_ERASEBKGND 0x0014
    #define WM_NCCREATE 0x0081
    #define WM_WTSSESSION_CHANGE 0x02B1
    #define GWLP_USERDATA (-21)
    #define LWA_ALPHA 0x00000002
    #define HWND_MESSAGE ((HWND)-3)
    #define MONITOR_DEFAULTTONEAREST 0x00000002
    #define WTS_SESSION_UNLOCK 0x8
    #define NOTIFY_FOR_THIS_SESSION 0
    
    // Windows结构体定义
    typedef struct tagPOINT {
        long x;
        long y;
    } POINT;
    
    typedef struct tagRECT {
        long left;
        long top;
        long right;
        long bottom;
    } RECT;
    
    typedef struct tagMSG {
        HWND hwnd;
        UINT message;
        WPARAM wParam;
        LPARAM lParam;
        DWORD time;
        POINT pt;
    } MSG;
    
    typedef struct tagPAINTSTRUCT {
        HDC hdc;
        PLATFORM_BOOL fErase;
        RECT rcPaint;
        PLATFORM_BOOL fRestore;
        PLATFORM_BOOL fIncUpdate;
        BYTE rgbReserved[32];
    } PAINTSTRUCT;
    
    typedef struct tagWNDCLASSEXW {
        UINT cbSize;
        UINT style;
        void* lpfnWndProc;
        int cbClsExtra;
        int cbWndExtra;
        HINSTANCE hInstance;
        void* hIcon;
        void* hCursor;
        void* hbrBackground;
        const wchar_t* lpszMenuName;
        const wchar_t* lpszClassName;
        void* hIconSm;
    } WNDCLASSEXW;
    
    typedef struct tagCREATESTRUCTW {
        void* lpCreateParams;
        HINSTANCE hInstance;
        void* hMenu;
        HWND hwndParent;
        int cy;
        int cx;
        int y;
        int x;
        long style;
        const wchar_t* lpszName;
        const wchar_t* lpszClass;
        DWORD dwExStyle;
    } CREATESTRUCTW;
#endif

// 跨平台函数声明
namespace PlatformCompat {
    // 消息框
    void ShowMessageBox(const std::string& message, const std::string& title = "提示");
    
    // 系统锁定检测
    bool IsSystemLocked();
    
    // DPI设置
    bool SetDpiAwareness(std::string& errorMsg);
    
    // 获取系统版本
    std::string GetSystemVersion();
    
    // 文件路径处理
    std::string GetProgramDataPath();
    
    // 摄像头设备枚举
    std::vector<std::string> GetCameraDeviceNames(std::vector<int>& deviceIDs);
    
    // 窗口消息处理（仅在需要时使用）
    void ProcessMessages();
}

#endif // PLATFORM_COMPAT_H