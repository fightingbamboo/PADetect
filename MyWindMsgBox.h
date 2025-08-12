#ifndef MY_WIND_MSG_BOX_H
#define MY_WIND_MSG_BOX_H

#include "PlatformCompat.h"
#include "CommonUtils.h"

#if PLATFORM_MACOS
// C++ 兼容的 macOS 消息框函数声明
extern "C" void showMacOSMessageBox(const char* title, const char* message, unsigned int style);
#endif

class MyWindMsgBox {
public:
    MyWindMsgBox() = default;
    explicit MyWindMsgBox(std::string content,
                        std::string title = "错误提示",
                        UINT style = MB_OK | MB_ICONERROR);

    void setMsgBoxParam(std::string content,
                       std::string title = "错误提示",
                       UINT style = MB_OK | MB_ICONERROR);

    ~MyWindMsgBox();

    MyWindMsgBox(const MyWindMsgBox&) = delete;
    MyWindMsgBox& operator=(const MyWindMsgBox&) = delete;

private:
#if PLATFORM_WINDOWS
    std::wstring m_title;
    std::wstring m_content;
#else
    std::string m_title;
    std::string m_content;
#endif
    UINT m_style = MB_OK | MB_ICONERROR;
};

#endif // !MY_WIND_MSG_BOX_H




