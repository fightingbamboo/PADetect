
#ifndef DEVICE_INFO_H
#define DEVICE_INFO_H

#include <string>
#include <vector>

class DeviceInfo {
public:
    static DeviceInfo* getInstance();
    
    // 获取计算机名称
    std::string getComputerName() const;
    
    // 获取设备UUID
    std::string getDeviceUUID() const;
    
    // 获取用户名
    std::string getUserName() const;
    
    // 获取MAC地址列表
    std::vector<std::string> getMacAddresses() const;
    
    // 获取第一个MAC地址
    std::string getFirstMacAddress() const;
    
    // 获取系统版本
    std::string getSystemVersion() const;
    
    // 获取设备唯一标识（组合UUID和MAC）
    std::string getDeviceIdentifier() const;
    
private:
    DeviceInfo() = default;
    ~DeviceInfo() = default;
    DeviceInfo(const DeviceInfo&) = delete;
    DeviceInfo& operator=(const DeviceInfo&) = delete;
    
    // 平台特定的UUID获取实现
    std::string getSystemUUID() const;
};

#endif // DEVICE_INFO_H