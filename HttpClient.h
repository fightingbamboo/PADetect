/*
 * Copyright 2024 Sheng Han
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <string>
#include <vector>
#include <mutex>
#include <string_view>
#include <filesystem>
#include <json/json.h>
#include <curl/curl.h>

class HttpClient {
public:
    static HttpClient* getInstance();

    void setHttpClientParam(const std::string& computerName, const std::string& userName,
        const std::string& mac, const std::string& companyCode,
        const std::string& baseUrl, const std::string& version,
        const std::string& certPath);
    bool uploadPicData(const std::vector<uint8_t>& data);
    bool requestKey();
    bool requestUnKey();
    bool requestConfig();
    bool uploadFile(const std::filesystem::path& filePath);

    std::string getLicenseKey() const;
    std::string getLicenseUnKey() const;
    std::string getConfig() const;
    std::string getConfigCheckSums() const;

private:
    HttpClient();
    ~HttpClient();
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* output);
    bool performRequest(CURL* curl, const std::string& path, const char* data,
                       size_t dataSize, const std::vector<std::string>& extraHeaders);
    bool parseResponse(const std::string& response, const std::string& path);
    bool performFileRequest(CURL* curl, const std::string& path, const std::filesystem::path& filePath,
        const std::vector<std::string>& extraHeaders);
    struct curl_slist* buildCommonHeaders() const;

    std::string m_computerName{ "" };
    std::string m_userName = "";
    std::string m_macAddress{ "" };
    std::string m_companyCode{ "" };
    std::string m_baseUrl{ "" };
    std::string m_version{ "" };
    std::string m_certPath{ "" };
    std::string m_licenseKey{ "" };
    std::string m_licenseUnKey{ "" };
    std::string m_configCfg{ "" };
    std::string m_configChecksums{ "" };
    mutable std::mutex m_mutex;
private:
    static constexpr int MAX_RETRIES = 1;      // 最大重试次数
    static constexpr int CONNECT_TIMEOUT = 1; // 连接超时(秒)
    static constexpr int TRANSFER_TIMEOUT = 30;// 传输超时(秒)
};

#endif // HTTP_CLIENT_H




