#include "MyWindMsgBox.h"
#include "MyLogger.hpp"

MyWindMsgBox::MyWindMsgBox(std::string content,
                          std::string title,
                          UINT style)
    : m_style(style)
{
    setMsgBoxParam(std::move(content), std::move(title));
}

void MyWindMsgBox::setMsgBoxParam(std::string content,
                                 std::string title,
                                 UINT style) {
#if PLATFORM_WINDOWS
    m_content = CommonUtils::Utf8ToWide(content);
    m_title = CommonUtils::Utf8ToWide(title);
#else
    m_content = content;
    m_title = title;
#endif
    m_style = style;
}

MyWindMsgBox::~MyWindMsgBox() {
    if (!m_content.empty() || !m_title.empty()) {
#if PLATFORM_WINDOWS
        MessageBoxW(nullptr, m_content.c_str(),
                   m_title.c_str(), m_style);
#elif PLATFORM_MACOS
        // 在 macOS 下使用 SwiftUI 消息框
        showMacOSMessageBox(m_title.c_str(), m_content.c_str(), m_style);
#else
        // 其他平台使用MyLogger输出
        MY_SPDLOG_INFO("[{}] {}", m_title, m_content);
#endif
    }
}