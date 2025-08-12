#ifndef SINGLETON_APP_H
#define SINGLETON_APP_H

#include "PlatformCompat.h"

#if PLATFORM_MACOS
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <string>
#endif

class SingletonApp {
public:

    // 获取单例实例
    static SingletonApp* getInstance() {
        static SingletonApp instance;
        return &instance;
    }

    // 检查当前是否是唯一实例
    bool isUniqueInstance() const {
        return m_isUniqueInstance;
    }

private:
    SingletonApp(const SingletonApp&) = delete;
    SingletonApp& operator=(const SingletonApp&) = delete;
    SingletonApp() :
#if PLATFORM_WINDOWS
        m_hMutex(nullptr),
#elif PLATFORM_MACOS
        m_lockFd(-1),
#else
        m_hMutex(nullptr),
#endif
        m_isUniqueInstance(false) {
#if PLATFORM_WINDOWS
        // 创建全局唯一命名的互斥体
        m_hMutex = CreateMutexA(nullptr, TRUE, "Global\\MyPAApp_Singleton_Mutex_3A3B3C");

        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            // 互斥体已存在 - 已有实例在运行
            if (m_hMutex) {
                CloseHandle(m_hMutex);
                m_hMutex = nullptr;
            }
            m_isUniqueInstance = false;
        }
        else {
            // 首次创建互斥体 - 当前是唯一实例
            m_isUniqueInstance = (m_hMutex != nullptr);
        }
#elif PLATFORM_MACOS
        // macOS 平台使用文件锁实现单例检测
        std::string lockFilePath = "/tmp/MyPAApp_Singleton_Lock";
        m_lockFd = open(lockFilePath.c_str(), O_CREAT | O_RDWR, 0666);
        
        if (m_lockFd != -1) {
            // 尝试获取排他锁
            if (flock(m_lockFd, LOCK_EX | LOCK_NB) == 0) {
                // 成功获取锁 - 当前是唯一实例
                m_isUniqueInstance = true;
            } else {
                // 无法获取锁 - 已有实例在运行
                close(m_lockFd);
                m_lockFd = -1;
                m_isUniqueInstance = false;
            }
        } else {
            // 无法创建锁文件
            m_isUniqueInstance = false;
        }
#endif
    }

    ~SingletonApp() {
#if PLATFORM_WINDOWS
        if (m_hMutex) {
            ReleaseMutex(m_hMutex);
            CloseHandle(m_hMutex);
        }
#elif PLATFORM_MACOS
        if (m_lockFd != -1) {
            // 释放文件锁并关闭文件描述符
            flock(m_lockFd, LOCK_UN);
            close(m_lockFd);
            // 删除锁文件
            unlink("/tmp/MyPAApp_Singleton_Lock");
        }
#endif
    }
private:
#if PLATFORM_WINDOWS
    HANDLE m_hMutex;
#elif PLATFORM_MACOS
    int m_lockFd;
#else
    void* m_hMutex;  // 占位符，保持兼容性
#endif
    bool m_isUniqueInstance;
};

#endif // SINGLETON_APP_H