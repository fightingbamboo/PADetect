
#include "DeviceInfo.h"
#include "PlatformCompat.h"
#include <iostream>
#include <sstream>
#include <cstring>

#if PLATFORM_WINDOWS
#include <Windows.h>
#include <iphlpapi.h>
#include <lmcons.h>
#pragma comment(lib, "iphlpapi.lib")
#elif PLATFORM_MACOS
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysctl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#endif

DeviceInfo* DeviceInfo::getInstance() {
    static DeviceInfo instance;
    return &instance;
}

std::string DeviceInfo::getComputerName() const {
#if PLATFORM_WINDOWS
    wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    
    if (GetComputerNameW(buffer, &size)) {
        // Convert wide string to UTF-8
        int utf8Size = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, nullptr, 0, nullptr, nullptr);
        if (utf8Size > 0) {
            std::string result(utf8Size - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, buffer, -1, &result[0], utf8Size, nullptr, nullptr);
            return result;
        }
    }
    return "Unknown";
#elif PLATFORM_MACOS
    char hostname[256];
    size_t size = sizeof(hostname);
    if (sysctlbyname("kern.hostname", hostname, &size, nullptr, 0) == 0) {
        return std::string(hostname);
    }
    
    // 备用方案
    struct utsname unameData;
    if (uname(&unameData) == 0) {
        return std::string(unameData.nodename);
    }
    
    return "Unknown";
#else
    return "Unknown";
#endif
}

std::string DeviceInfo::getDeviceUUID() const {
    return getSystemUUID();
}

std::string DeviceInfo::getUserName() const {
#if PLATFORM_WINDOWS
    wchar_t username[UNLEN + 1] = { 0 };
    DWORD size = UNLEN + 1;
    
    if (GetUserNameW(username, &size)) {
        // Convert wide string to UTF-8
        int utf8Size = WideCharToMultiByte(CP_UTF8, 0, username, -1, nullptr, 0, nullptr, nullptr);
        if (utf8Size > 0) {
            std::string result(utf8Size - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, username, -1, &result[0], utf8Size, nullptr, nullptr);
            return result;
        }
    }
    return "Unknown";
#elif PLATFORM_MACOS
    char* username = getenv("USER");
    if (username) {
        return std::string(username);
    }
    return "Unknown";
#else
    return "Unknown";
#endif
}

std::vector<std::string> DeviceInfo::getMacAddresses() const {
    std::vector<std::string> macAddresses;
    
#if PLATFORM_WINDOWS
    ULONG bufferSize = 0;
    DWORD retval = GetAdaptersInfo(nullptr, &bufferSize);
    
    if (retval == ERROR_BUFFER_OVERFLOW) {
        std::unique_ptr<BYTE[]> buffer(new BYTE[bufferSize]);
        PIP_ADAPTER_INFO adapterInfo = reinterpret_cast<PIP_ADAPTER_INFO>(buffer.get());
        
        retval = GetAdaptersInfo(adapterInfo, &bufferSize);
        if (retval == NO_ERROR) {
            while (adapterInfo) {
                if (adapterInfo->AddressLength >= 6) {
                    char macStr[18];
                    snprintf(macStr, sizeof(macStr), "%02X-%02X-%02X-%02X-%02X-%02X",
                             adapterInfo->Address[0], adapterInfo->Address[1], adapterInfo->Address[2],
                             adapterInfo->Address[3], adapterInfo->Address[4], adapterInfo->Address[5]);
                    
                    std::string macAddress(macStr);
                    // 过滤掉全零MAC地址
                    if (macAddress != "00-00-00-00-00-00") {
                        macAddresses.push_back(macAddress);
                    }
                }
                adapterInfo = adapterInfo->Next;
            }
        }
    }
#elif PLATFORM_MACOS
    struct ifaddrs *ifap, *ifaptr;
    if (getifaddrs(&ifap) == 0) {
        for (ifaptr = ifap; ifaptr != nullptr; ifaptr = ifaptr->ifa_next) {
            if (ifaptr->ifa_addr && ifaptr->ifa_addr->sa_family == AF_LINK) {
                struct sockaddr_dl* sdl = (struct sockaddr_dl*)ifaptr->ifa_addr;
                if (sdl->sdl_alen == 6) {
                    unsigned char* mac = (unsigned char*)LLADDR(sdl);
                    char macStr[18];
                    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
                            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                    
                    std::string macAddress(macStr);
                    // 过滤掉全零MAC地址
                    if (macAddress != "00:00:00:00:00:00") {
                        macAddresses.push_back(macAddress);
                    }
                }
            }
        }
        freeifaddrs(ifap);
    }
#endif
    
    return macAddresses;
}

std::string DeviceInfo::getFirstMacAddress() const {
    auto macAddresses = getMacAddresses();
    if (!macAddresses.empty()) {
        return macAddresses[0];
    }
#if PLATFORM_WINDOWS
    return "00-00-00-00-00-00";
#else
    return "00:00:00:00:00:00";
#endif
}

std::string DeviceInfo::getSystemVersion() const {
#if PLATFORM_WINDOWS
    OSVERSIONINFOW osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOW));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
    
    // Note: GetVersionExW is deprecated, but still works for basic version info
    #pragma warning(push)
    #pragma warning(disable: 4996)
    if (GetVersionExW(&osvi)) {
        std::stringstream ss;
        ss << "Windows " << osvi.dwMajorVersion << "." << osvi.dwMinorVersion 
           << " Build " << osvi.dwBuildNumber;
        return ss.str();
    }
    #pragma warning(pop)
    
    return "Windows Unknown";
#elif PLATFORM_MACOS
    char version[256];
    size_t size = sizeof(version);
    if (sysctlbyname("kern.version", version, &size, nullptr, 0) == 0) {
        std::string versionStr(version);
        // 提取版本号部分
        size_t pos = versionStr.find(":");
        if (pos != std::string::npos) {
            return versionStr.substr(0, pos);
        }
        return versionStr;
    }
    
    struct utsname unameData;
    if (uname(&unameData) == 0) {
        return std::string(unameData.release);
    }
    
    return "Unknown";
#else
    return "Unknown";
#endif
}

std::string DeviceInfo::getDeviceIdentifier() const {
    std::string uuid = getDeviceUUID();
    std::string mac = getFirstMacAddress();
    
    // 组合UUID和MAC地址作为设备唯一标识
    return uuid + "-" + mac;
}

std::string DeviceInfo::getSystemUUID() const {
#if PLATFORM_WINDOWS
    // Windows平台获取机器GUID
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, 
                               L"SOFTWARE\\Microsoft\\Cryptography", 
                               0, KEY_READ, &hKey);
    
    if (result == ERROR_SUCCESS) {
        wchar_t buffer[256];
        DWORD bufferSize = sizeof(buffer);
        DWORD type;
        
        result = RegQueryValueExW(hKey, L"MachineGuid", nullptr, &type, 
                                 (LPBYTE)buffer, &bufferSize);
        RegCloseKey(hKey);
        
        if (result == ERROR_SUCCESS && type == REG_SZ) {
            // Convert wide string to UTF-8
            int utf8Size = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, nullptr, 0, nullptr, nullptr);
            if (utf8Size > 0) {
                std::string uuid(utf8Size - 1, '\0');
                WideCharToMultiByte(CP_UTF8, 0, buffer, -1, &uuid[0], utf8Size, nullptr, nullptr);
                return uuid;
            }
        }
    }
    
    // 备用方案：基于MAC地址生成
    std::string mac = getFirstMacAddress();
    if (mac != "00-00-00-00-00-00") {
        return "MAC-" + mac;
    }
    
    return "UNKNOWN-UUID";
#elif PLATFORM_MACOS
    io_registry_entry_t ioRegistryRoot = IORegistryEntryFromPath(kIOMainPortDefault, "IOService:/");
    CFStringRef uuidCf = (CFStringRef) IORegistryEntryCreateCFProperty(ioRegistryRoot,
                                                                        CFSTR(kIOPlatformUUIDKey),
                                                                        kCFAllocatorDefault, 0);
    IOObjectRelease(ioRegistryRoot);
    
    if (uuidCf) {
        char buffer[256];
        Boolean result = CFStringGetCString(uuidCf, buffer, sizeof(buffer), kCFStringEncodingUTF8);
        CFRelease(uuidCf);
        
        if (result) {
            return std::string(buffer);
        }
    }
    
    // 如果无法获取系统UUID，生成一个基于MAC地址的标识
    std::string mac = getFirstMacAddress();
    if (mac != "00:00:00:00:00:00") {
        // 简单的MAC地址转换为UUID格式
        std::string uuid = mac;
        std::replace(uuid.begin(), uuid.end(), ':', '-');
        return "MAC-" + uuid;
    }
    
    return "UNKNOWN-UUID";
#else
    // 其他平台的备用方案
    std::string mac = getFirstMacAddress();
    if (!mac.empty() && mac != "00:00:00:00:00:00" && mac != "00-00-00-00-00-00") {
        return "MAC-" + mac;
    }
    
    return "UNKNOWN-UUID";
#endif
}