
#ifndef UPDATE_MANAGER_H
#define UPDATE_MANAGER_H

#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>

class HttpClient;

struct UpdateInfo {
    std::string version;
    std::string downloadUrl;
    std::string checksum;
    std::string releaseNotes;
    bool forceUpdate = false;
    size_t fileSize = 0;
};

struct DownloadProgress {
    size_t totalBytes = 0;
    size_t downloadedBytes = 0;
    double percentage = 0.0;
    std::string status;
};

class UpdateManager {
public:
    using ProgressCallback = std::function<void(const DownloadProgress&)>;
    using CompletionCallback = std::function<void(bool success, const std::string& error)>;
    
    static UpdateManager* getInstance();
    
    // 检查更新
    bool checkForUpdates(const std::string& currentVersion);
    
    // 异步检查更新
    void checkForUpdatesAsync(const std::string& currentVersion, 
                             std::function<void(bool hasUpdate, const UpdateInfo&)> callback);
    
    // 下载更新
    bool downloadUpdate(const UpdateInfo& updateInfo, 
                       ProgressCallback progressCallback = nullptr);
    
    // 异步下载更新
    void downloadUpdateAsync(const UpdateInfo& updateInfo,
                            ProgressCallback progressCallback = nullptr,
                            CompletionCallback completionCallback = nullptr);
    
    // 安装更新
    bool installUpdate(const std::string& updateFilePath);
    
    // 验证更新文件
    bool verifyUpdateFile(const std::string& filePath, const std::string& expectedChecksum);
    
    // 配置热更新
    bool updateConfiguration();
    
    // 异步配置更新
    void updateConfigurationAsync(CompletionCallback callback = nullptr);
    
    // 设置更新服务器URL
    void setUpdateServerUrl(const std::string& url);
    
    // 设置下载目录
    void setDownloadDirectory(const std::string& dir);
    
    // 获取最新的更新信息
    const UpdateInfo& getLatestUpdateInfo() const { return latestUpdateInfo_; }
    
    // 取消当前下载
    void cancelDownload();
    
    // 检查是否有下载在进行
    bool isDownloading() const { return isDownloading_; }
    
    // 清理临时文件
    void cleanupTempFiles();
    
private:
    UpdateManager();
    ~UpdateManager();
    UpdateManager(const UpdateManager&) = delete;
    UpdateManager& operator=(const UpdateManager&) = delete;
    
    // 解析更新响应
    bool parseUpdateResponse(const std::string& response, UpdateInfo& updateInfo);
    
    // 比较版本号
    int compareVersions(const std::string& version1, const std::string& version2);
    
    // 计算文件校验和
    std::string calculateChecksum(const std::string& filePath);
    
    // 下载文件的内部实现
    bool downloadFileInternal(const std::string& url, const std::string& filePath,
                             ProgressCallback progressCallback);
    
    // 创建下载目录
    bool createDownloadDirectory();
    
    // 获取临时文件路径
    std::string getTempFilePath(const std::string& filename);
    
private:
    HttpClient* httpClient_;
    std::string updateServerUrl_;
    std::string downloadDirectory_;
    UpdateInfo latestUpdateInfo_;
    std::atomic<bool> isDownloading_;
    std::atomic<bool> cancelRequested_;
    std::unique_ptr<std::thread> downloadThread_;
};

#endif // UPDATE_MANAGER_H