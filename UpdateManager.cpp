
#include "UpdateManager.h"
#include "HttpClient.h"
#include "MyLogger.hpp"
#include <json/json.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <curl/curl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

UpdateManager::UpdateManager() 
    : httpClient_(HttpClient::getInstance()), 
      isDownloading_(false),
      cancelRequested_(false) {
    
    // 设置默认下载目录
#ifdef __APPLE__
    downloadDirectory_ = std::string(getenv("HOME")) + "/Library/Application Support/PADetect/Updates";
#else
    downloadDirectory_ = "./updates";
#endif
    
    createDownloadDirectory();
}

UpdateManager::~UpdateManager() {
    cancelDownload();
    if (downloadThread_ && downloadThread_->joinable()) {
        downloadThread_->join();
    }
}

UpdateManager* UpdateManager::getInstance() {
    static UpdateManager instance;
    return &instance;
}

void UpdateManager::setUpdateServerUrl(const std::string& url) {
    updateServerUrl_ = url;
}

void UpdateManager::setDownloadDirectory(const std::string& dir) {
    downloadDirectory_ = dir;
    createDownloadDirectory();
}

bool UpdateManager::checkForUpdates(const std::string& currentVersion) {
    if (updateServerUrl_.empty()) {
        MY_SPDLOG_ERROR("Update server URL not set");
        return false;
    }
    
    // 简化实现：直接返回false，表示暂无更新
    // 实际实现需要调用服务器API检查更新
    MY_SPDLOG_INFO("Checking for updates... (not implemented yet)");
    return false;
}

void UpdateManager::checkForUpdatesAsync(const std::string& currentVersion,
                                       std::function<void(bool, const UpdateInfo&)> callback) {
    std::thread([this, currentVersion, callback]() {
        bool hasUpdate = checkForUpdates(currentVersion);
        if (callback) {
            callback(hasUpdate, latestUpdateInfo_);
        }
    }).detach();
}

bool UpdateManager::downloadUpdate(const UpdateInfo& updateInfo, ProgressCallback progressCallback) {
    if (isDownloading_) {
        MY_SPDLOG_WARN("Download already in progress");
        return false;
    }
    
    isDownloading_ = true;
    cancelRequested_ = false;
    
    std::string filename = "PADetect_" + updateInfo.version + ".dmg";
    std::string filePath = getTempFilePath(filename);
    
    bool success = downloadFileInternal(updateInfo.downloadUrl, filePath, progressCallback);
    
    isDownloading_ = false;
    return success && !cancelRequested_;
}

void UpdateManager::downloadUpdateAsync(const UpdateInfo& updateInfo,
                                      ProgressCallback progressCallback,
                                      CompletionCallback completionCallback) {
    if (downloadThread_ && downloadThread_->joinable()) {
        downloadThread_->join();
    }
    
    downloadThread_ = std::make_unique<std::thread>([this, updateInfo, progressCallback, completionCallback]() {
        bool success = downloadUpdate(updateInfo, progressCallback);
        if (completionCallback) {
            std::string error = success ? "" : "Download failed";
            completionCallback(success, error);
        }
    });
}

bool UpdateManager::installUpdate(const std::string& updateFilePath) {
    struct stat fileStat;
    if (stat(updateFilePath.c_str(), &fileStat) != 0) {
        MY_SPDLOG_ERROR("Update file not found: {}", updateFilePath);
        return false;
    }
    
#ifdef __APPLE__
    // macOS安装逻辑
    std::string command = "open \"" + updateFilePath + "\"";
    int result = system(command.c_str());
    
    if (result == 0) {
        MY_SPDLOG_INFO("Update installer launched successfully");
        return true;
    } else {
        MY_SPDLOG_ERROR("Failed to launch update installer");
        return false;
    }
#else
    MY_SPDLOG_ERROR("Install update not implemented for this platform");
    return false;
#endif
}

bool UpdateManager::verifyUpdateFile(const std::string& filePath, const std::string& expectedChecksum) {
    // 简化实现：暂时返回true
    MY_SPDLOG_INFO("Update file verification (simplified implementation)");
    return true;
}

bool UpdateManager::updateConfiguration() {
    if (updateServerUrl_.empty()) {
        MY_SPDLOG_ERROR("Update server URL not set");
        return false;
    }
    
    // 使用现有的HttpClient接口请求配置
    bool success = httpClient_->requestConfig();
    if (success) {
        std::string newConfig = httpClient_->getConfig();
        if (!newConfig.empty()) {
            // 保存新配置到文件
            std::ofstream configFile("./config.json");
            if (configFile.is_open()) {
                configFile << newConfig;
                configFile.close();
                MY_SPDLOG_INFO("Configuration updated successfully");
                return true;
            }
        }
    }
    
    MY_SPDLOG_ERROR("Failed to update configuration");
    return false;
}

void UpdateManager::updateConfigurationAsync(CompletionCallback callback) {
    std::thread([this, callback]() {
        bool success = updateConfiguration();
        if (callback) {
            std::string error = success ? "" : "Configuration update failed";
            callback(success, error);
        }
    }).detach();
}

void UpdateManager::cancelDownload() {
    cancelRequested_ = true;
}

void UpdateManager::cleanupTempFiles() {
    // 简化的清理实现
    MY_SPDLOG_INFO("Cleaning up temporary files...");
    
    DIR* dir = opendir(downloadDirectory_.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string filename = entry->d_name;
            if (filename.find("temp_") == 0 || filename.find("_backup") != std::string::npos) {
                std::string fullPath = downloadDirectory_ + "/" + filename;
                if (unlink(fullPath.c_str()) == 0) {
                    MY_SPDLOG_INFO("Removed temp file: {}", filename);
                }
            }
        }
        closedir(dir);
    }
}

bool UpdateManager::parseUpdateResponse(const std::string& response, UpdateInfo& updateInfo) {
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::istringstream responseStream(response);
    std::string parseErrors;
    
    if (!Json::parseFromStream(builder, responseStream, &root, &parseErrors)) {
        MY_SPDLOG_ERROR("Failed to parse update response: {}", parseErrors);
        return false;
    }
    
    if (!root.isMember("version") || !root.isMember("download_url")) {
        MY_SPDLOG_ERROR("Invalid update response format");
        return false;
    }
    
    updateInfo.version = root["version"].asString();
    updateInfo.downloadUrl = root["download_url"].asString();
    updateInfo.checksum = root.get("checksum", "").asString();
    updateInfo.releaseNotes = root.get("release_notes", "").asString();
    updateInfo.forceUpdate = root.get("force_update", false).asBool();
    updateInfo.fileSize = root.get("file_size", 0).asUInt64();
    
    return true;
}

int UpdateManager::compareVersions(const std::string& version1, const std::string& version2) {
    // 简单的版本比较实现
    if (version1 == version2) return 0;
    return version1 > version2 ? 1 : -1;
}

std::string UpdateManager::calculateChecksum(const std::string& filePath) {
    // 简化实现：返回空字符串
    return "";
}

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::ofstream* file = static_cast<std::ofstream*>(userp);
    file->write(static_cast<char*>(contents), totalSize);
    return totalSize;
}

static int ProgressCallbackFunc(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    UpdateManager::ProgressCallback* callback = static_cast<UpdateManager::ProgressCallback*>(clientp);
    
    if (callback && *callback && dltotal > 0) {
        DownloadProgress progress;
        progress.totalBytes = dltotal;
        progress.downloadedBytes = dlnow;
        progress.percentage = (double)dlnow / (double)dltotal * 100.0;
        progress.status = "Downloading...";
        
        (*callback)(progress);
    }
    
    return 0;
}

bool UpdateManager::downloadFileInternal(const std::string& url, const std::string& filePath,
                                       ProgressCallback progressCallback) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        MY_SPDLOG_ERROR("Failed to initialize CURL");
        return false;
    }
    
    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        MY_SPDLOG_ERROR("Failed to create download file: {}", filePath);
        curl_easy_cleanup(curl);
        return false;
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L); // 5分钟超时
    
    if (progressCallback) {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallbackFunc);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progressCallback);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    }
    
    CURLcode res = curl_easy_perform(curl);
    
    long responseCode;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
    
    curl_easy_cleanup(curl);
    file.close();
    
    if (res != CURLE_OK || responseCode != 200) {
        MY_SPDLOG_ERROR("Download failed: {} (HTTP {})", curl_easy_strerror(res), responseCode);
        unlink(filePath.c_str());
        return false;
    }
    
    MY_SPDLOG_INFO("Download completed: {}", filePath);
    return true;
}

bool UpdateManager::createDownloadDirectory() {
    struct stat st;
    if (stat(downloadDirectory_.c_str(), &st) != 0) {
        // 目录不存在，创建它
        std::string command = "mkdir -p \"" + downloadDirectory_ + "\"";
        int result = system(command.c_str());
        if (result != 0) {
            MY_SPDLOG_ERROR("Failed to create download directory: {}", downloadDirectory_);
            return false;
        }
    }
    return true;
}

std::string UpdateManager::getTempFilePath(const std::string& filename) {
    return downloadDirectory_ + "/" + filename;
}