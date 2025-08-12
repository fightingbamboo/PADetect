#include <cstddef>
#include <minwindef.h>
#include <string>
#include <vector>
#include <windows.h>
#include <winnt.h>

#include "AlertWindowManager.h"
#include "MyLogger.hpp"
#include "CommonUtils.h"

bool AlertWindowManager::m_s_classRegistered = false;

// Constructor: Register window class and create windows
AlertWindowManager::AlertWindowManager()
    : mHInstance(GetModuleHandle(NULL)),
    mIsShow(false)
#if 0
    , mContinue(false), mMainThd(nullptr)
#endif
{
    // Register the window class.
    mWc = {};
    mWc.cbSize = sizeof(WNDCLASSEXW);
    mWc.lpfnWndProc = WindowProc;
    mWc.hInstance = mHInstance;
    mWc.lpszClassName = L"AlertWindowClass";
    //mWc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    mWc.hCursor = NULL;
}

// Destructor: Ensure windows are destroyed
AlertWindowManager::~AlertWindowManager() {

}
#if 0
void AlertWindowManager::mainThd() {
    while (mContinue.load()) {
        std::unique_lock<std::mutex> lock(mMtx);
        mCond.wait(lock);
        if (!mContinue.load()) {
            break;
        }

    }
}

void AlertWindowManager::startThread() {
    mContinue.store(true);
    if (!mMainThd) {
        mMainThd = std::make_unique<std::thread>(&AlertWindowManager::mainThd, this);
    }
}

void AlertWindowManager::stopThread() {
    mContinue.store(false);
    {
        std::unique_lock<std::mutex> lock(mMtx);
        mCond.notify_one();
    }

    if (mMainThd && mMainThd->joinable()) {
        mMainThd->join();
        mMainThd.reset();
    }
}
#endif

bool AlertWindowManager::initWind() {
    if (!m_s_classRegistered) {
        if (!RegisterClassExW(&mWc)) {
            DWORD err = GetLastError();
            if (err != ERROR_CLASS_ALREADY_EXISTS) {
                return false;
            }
        }
        m_s_classRegistered = true;
    }
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(this));
    return true;
}

void AlertWindowManager::deinitWind() {
    for (HWND hwnd : mAlertWindows) {
        DestroyWindow(hwnd);
    }
    mAlertWindows.swap(std::vector<HWND>());
    UnregisterClassW(mWc.lpszClassName, mHInstance);
}

void AlertWindowManager::initGDIPlus() {
#if 0
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&mGdiplusToken, &gdiplusStartupInput, NULL);
#endif
}

void AlertWindowManager::deinitGDIPlus() {
#if 0
    mAlertImage.reset();
    Gdiplus::GdiplusShutdown(mGdiplusToken);
#endif
}

void AlertWindowManager::showAlert() {
    mIsShow.store(true);
    for (HWND hwnd : mAlertWindows) {
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);  // 强制立即更新窗口
    }
}

void AlertWindowManager::hideAlert() {
    for (HWND hwnd : mAlertWindows) {
        ShowWindow(hwnd, SW_HIDE);
    }
    mIsShow.store(false);
}

void AlertWindowManager::setAlertVersion(const std::string& version) {
    m_alertVersion = CommonUtils::Utf8ToWide(version);
}

void AlertWindowManager::setAlertShowMode(ALERT_MODE alertMode) {
    if (mAlertMode.load() != alertMode) {
        mAlertMode.store(alertMode);
    }
}

bool AlertWindowManager::isShow() const {
    return mIsShow.load();
}

/*static*/
LRESULT CALLBACK AlertWindowManager::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    // keybord
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_CHAR:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP: {
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    // mouse
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MOUSEMOVE:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL: {
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    case WM_SETCURSOR: {
        SetCursor(NULL);
        return TRUE;  // 直接返回，阻止进一步处理
    }

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        //HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0)); // Black
        HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 139)); // 深蓝色背景
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);
        return 1;
    }

    case WM_PAINT: {
        // Use the stored alertText for display
        AlertWindowManager* pManager = reinterpret_cast<AlertWindowManager*>(
            GetWindowLongPtr(hwnd, GWLP_USERDATA));

        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        //SetTextColor(hdc, RGB(255, 0, 0));    // Red
        SetTextColor(hdc, RGB(255, 255, 255)); // 白色文字
        SetBkMode(hdc, TRANSPARENT);

        if (!pManager) {
            return 0;
        }
        HFONT hFont = pManager->CreateCompatibleFont(hdc);

        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

        RECT rect;
        GetClientRect(hwnd, &rect);


        if (pManager) {
            std::shared_lock<std::shared_mutex> readLock(pManager->m_paramMtx);

            switch (pManager->mAlertMode.load() )
            {
            case ALERT_MODE::TEXT_PHONE: {
                DrawTextW(hdc, pManager->m_alertStrPhone.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            } break;
            case ALERT_MODE::TEXT_PEEP: {
                DrawTextW(hdc, pManager->m_alertStrPeep.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            } break;
            case ALERT_MODE::TEXT_NOBODY: {
                DrawTextW(hdc, pManager->m_alertStrNobody.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            } break;
            case ALERT_MODE::TEXT_OCCLUDE: {
                DrawTextW(hdc, pManager->m_alertStrOcclude.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            } break;
            case ALERT_MODE::TEXT_NOCONNECT: {
                DrawTextW(hdc, pManager->m_alertStrNoconnect.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            } break;
            default:
                break;
            }

            if (!pManager->m_alertVersion.empty()) {
                RECT rcFooter = rect;
                rcFooter.top = rect.bottom - 50;  // 距离底部50像素
                rcFooter.left = rect.right - 200; // 距离右侧200像素

                // 使用小号字体
                HFONT hSmallFont = CreateFont(
                    -MulDiv(7, GetDeviceCaps(hdc, LOGPIXELSY), 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                    VARIABLE_PITCH, pManager->mAlertFont.c_str());
                HFONT hOldSmallFont = (HFONT)SelectObject(hdc, hSmallFont);

                // 绘制右下角文字（灰色，右对齐）
                SetTextColor(hdc, RGB(180, 180, 180)); // 灰色文字
                DrawTextW(hdc, pManager->m_alertVersion.c_str(), -1, &rcFooter, DT_RIGHT | DT_BOTTOM | DT_SINGLELINE);

                // 恢复原字体
                SelectObject(hdc, hOldSmallFont);
                DeleteObject(hSmallFont);
            }
        }

        SelectObject(hdc, hOldFont);
        DeleteObject(hFont);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CREATE: {  // Store a pointer to AlertWindowManager object
        LPCREATESTRUCT pCreate = reinterpret_cast<LPCREATESTRUCT>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreate->lpCreateParams));
        return 0;
    }
    default: {
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

/*static*/
BOOL CALLBACK AlertWindowManager::MonitorEnumProc(HMONITOR, HDC, LPRECT lprcMonitor, LPARAM dwData) {
    AlertWindowManager* manager = reinterpret_cast<AlertWindowManager*>(dwData);

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        L"AlertWindowClass",
        nullptr,
        WS_POPUP,
        lprcMonitor->left,
        lprcMonitor->top,
        lprcMonitor->right - lprcMonitor->left,
        lprcMonitor->bottom - lprcMonitor->top,
        NULL, NULL, manager->mHInstance, manager);  // Pass manager as lpCreateParams

    if (hwnd) {
        SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
        manager->mAlertWindows.push_back(hwnd);
        // Initially hide windows
        ShowWindow(hwnd, SW_HIDE);
    }
    return TRUE;
}

// ================ 兼容的 DPI 获取方法 ================
int AlertWindowManager::GetSystemDPI() {
    HDC screenDC = GetDC(nullptr);
    int dpi = 96; // 默认值
    if (screenDC) {
        dpi = GetDeviceCaps(screenDC, LOGPIXELSY);
        ReleaseDC(nullptr, screenDC);
    }
    return dpi;
}

void AlertWindowManager::onConfigUpdated(std::shared_ptr<MyMeta>& newMeta) {
    setAlertParam(newMeta);
}

// ================ 字体大小计算 ================
int AlertWindowManager::GetScaledFontSize(int baseSize) {
    static int systemDPI = GetSystemDPI();
    return -MulDiv(baseSize, systemDPI, 72);
}

// ================ 兼容的字体创建 ================
HFONT AlertWindowManager::CreateCompatibleFont(HDC hdc) {
    std::shared_lock<std::shared_mutex> readLock(m_paramMtx);

    int scaledSize = GetScaledFontSize(mAlertFontSize);

    // 尝试首选字体
    HFONT hFont = CreateFontW(
        scaledSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        VARIABLE_PITCH,
        mAlertFont.c_str()
    );

    // 如果失败，尝试备用字体
    if (!hFont) {
        const wchar_t* fallbacks[] = {
            L"Microsoft YaHei", L"SimHei", L"SimSun", L"Arial"
        };

        for (const wchar_t* font : fallbacks) {
            hFont = CreateFontW(
                scaledSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                VARIABLE_PITCH,
                font
            );
            if (hFont) break;
        }
    }

    // 最终回退
    if (!hFont) {
        hFont = CreateFontW(
            -MulDiv(24, GetDeviceCaps(hdc, LOGPIXELSY), 72),
            0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, 0, 0, L"Arial"
        );
    }

    return hFont;
}

void AlertWindowManager::setAlertParam(std::shared_ptr<MyMeta>& meta) {
    if (!m_isCfgListReg) {
        ConfigParser* cfg = ConfigParser::getInstance();
        cfg->registerListener("alertWindowSettings", this);
        m_isCfgListReg = true;
    }

    std::unique_lock<std::shared_mutex> writeLock(m_paramMtx);
    // 使用类型安全的默认值获取方法
    m_alertStrPhone = CommonUtils::Utf8ToWide(
        meta->getStringOrDefault("alert_string_phone", CommonUtils::WideToUtf8(m_alertStrPhone))
    );

    m_alertStrPeep = CommonUtils::Utf8ToWide(
        meta->getStringOrDefault("alert_string_peep", CommonUtils::WideToUtf8(m_alertStrPeep))
    );

    m_alertStrNobody = CommonUtils::Utf8ToWide(
        meta->getStringOrDefault("alert_string_nobody", CommonUtils::WideToUtf8(m_alertStrNobody))
    );

    m_alertStrOcclude = CommonUtils::Utf8ToWide(
        meta->getStringOrDefault("alert_string_occlude", CommonUtils::WideToUtf8(m_alertStrOcclude))
    );

    m_alertStrNoconnect = CommonUtils::Utf8ToWide(
        meta->getStringOrDefault("alert_string_noconnect", CommonUtils::WideToUtf8(m_alertStrNoconnect))
    );

    mAlertFont = CommonUtils::Utf8ToWide(
        meta->getStringOrDefault("alert_font", CommonUtils::WideToUtf8(mAlertFont))
    );

    mAlertFontSize = meta->getInt32OrDefault("alert_font_size", mAlertFontSize);

    // 日志输出保持不变
    MY_SPDLOG_DEBUG("告警参数更新: phone='{}', peep='{}', nobody='{}', occlude='{}' noconnect='{}', font='{}', size={}",
                   CommonUtils::WideToUtf8(m_alertStrPhone),
                   CommonUtils::WideToUtf8(m_alertStrPeep),
                   CommonUtils::WideToUtf8(m_alertStrNobody),
                   CommonUtils::WideToUtf8(m_alertStrOcclude),
                   CommonUtils::WideToUtf8(m_alertStrNoconnect),
                   CommonUtils::WideToUtf8(mAlertFont),
                   mAlertFontSize);
}
