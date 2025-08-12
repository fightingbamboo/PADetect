#include <algorithm>
#include <cctype>
#include <chrono>
#include <thread>
#include <sstream>
#include <filesystem>

#include "HttpClient.h"
#include "CommonUtils.h"
#include "MyLogger.hpp"

// HttpClient实现
HttpClient::HttpClient() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

HttpClient::~HttpClient() {
    curl_global_cleanup();
}

HttpClient* HttpClient::getInstance()
{
    static HttpClient instance;
    return &instance;
}

void HttpClient::setHttpClientParam(const std::string& computerName, const std::string& userName,
    const std::string& mac, const std::string& companyCode,
    const std::string& baseUrl, const std::string& version,
    const std::string& certPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_computerName = computerName;
    m_userName = userName;
    m_macAddress = mac;
    m_companyCode = companyCode;
    m_baseUrl = baseUrl;
    m_version = version;
    m_certPath = certPath;
}

bool HttpClient::uploadPicData(const std::vector<uint8_t>& data) {
    std::vector<std::string> headers = {
        "Content-Type: application/octet-stream",
        "Expect:"
    };

    CURL* curl = curl_easy_init();
    if (!curl) {
        MY_SPDLOG_ERROR("Failed to initialize CURL handle");
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    bool result = performRequest(curl, "/client/risk/upload",
        reinterpret_cast<const char*>(data.data()),
        data.size(), headers);
    curl_easy_cleanup(curl);
    return result;
}

bool HttpClient::requestKey() {
    CURL* curl = curl_easy_init();
    if (!curl) {
        MY_SPDLOG_ERROR("Failed to initialize CURL handle");
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    bool result = performRequest(curl, "/client/activate", nullptr, 0, {});
    if (result) {
        MY_SPDLOG_INFO("License key acquired: {}", m_licenseKey);
    }
    curl_easy_cleanup(curl);
    return result;
}

bool HttpClient::requestUnKey() {
    CURL* curl = curl_easy_init();
    if (!curl) {
        MY_SPDLOG_ERROR("Failed to initialize CURL handle");
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    bool result = performRequest(curl, "/client/unActivate", nullptr, 0, {});
    if (result) {
        MY_SPDLOG_INFO("License key acquired: {}", m_licenseUnKey);
    }
    curl_easy_cleanup(curl);
    return result;
}

bool HttpClient::requestConfig() {
    CURL* curl = curl_easy_init();
    if (!curl) {
        MY_SPDLOG_ERROR("Failed to initialize CURL handle");
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    bool result = performRequest(curl, "/client/getCfg", nullptr, 0, {});
    if (result) {
        MY_SPDLOG_INFO("Config acquired: {}", m_configCfg);
    }
    curl_easy_cleanup(curl);
    return result;
}

std::string HttpClient::getLicenseKey() const
{
    return m_licenseKey;
}

std::string HttpClient::getLicenseUnKey() const
{
    return m_licenseUnKey;
}

std::string HttpClient::getConfig() const
{
    return m_configCfg;
}

std::string HttpClient::getConfigCheckSums() const
{
    return m_configChecksums;
}

size_t HttpClient::writeCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t total = size * nmemb;
    output->append(static_cast<char*>(contents), total);
    return total;
}

bool HttpClient::performRequest(CURL* curl, const std::string& path, const char* data,
                              size_t dataSize, const std::vector<std::string>& extraHeaders) {
    std::string url = m_baseUrl + path;
    std::string response;
    long httpCode = 0;

    struct curl_slist* headers = buildCommonHeaders();

    for (const auto& h : extraHeaders) {
        headers = curl_slist_append(headers, h.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    bool isHTTPS = false;
    size_t pos = url.find("://");
    if (pos != std::string::npos) {
        std::string scheme = url.substr(0, pos);
        std::transform(scheme.begin(), scheme.end(), scheme.begin(), ::tolower);
        isHTTPS = (scheme == "https");
    }
    else {
        MY_SPDLOG_WARN("Invalid URL format: {}", url);
    }

    if (isHTTPS) {
        if (!m_certPath.empty()) {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(curl, CURLOPT_CAINFO, m_certPath.c_str());
            MY_SPDLOG_DEBUG("Using SSL verification with cert: {}", m_certPath);
        }
        else {
            MY_SPDLOG_ERROR("HTTPS requires certificate file");
            return false;
        }
    }
    else {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        MY_SPDLOG_DEBUG("Disabled SSL verification for HTTP");
    }

    if (data && dataSize > 0) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, dataSize);
    }

    char errorBuffer[CURL_ERROR_SIZE] = { 0 };
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);

    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CONNECT_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, TRANSFER_TIMEOUT);

    int retryCount = 0;
    bool shouldRetry = false;

    do {
        shouldRetry = false;
        CURLcode res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

        // 错误分类处理
        if (res != CURLE_OK) {
            MY_SPDLOG_WARN("CURL error: {} ({}), attempt {}/{}",
                          curl_easy_strerror(res), errorBuffer, retryCount + 1, MAX_RETRIES);

            // 可重试的错误类型
            if (res == CURLE_COULDNT_CONNECT ||     // 连接失败
                res == CURLE_OPERATION_TIMEDOUT ||  // 超时
                res == CURLE_SEND_ERROR ||          // 发送中断
                res == CURLE_RECV_ERROR) {          // 接收中断
                shouldRetry = true;
                retryCount++;

                // 指数退避重试
                //std::this_thread::sleep_for(std::chrono::seconds(1 << retryCount));
            }
            else {
                // 不可恢复的错误
                MY_SPDLOG_ERROR("Non-recoverable error: {} ({})",
                               curl_easy_strerror(res), errorBuffer);
                curl_slist_free_all(headers);
                return false;
            }
        }
    } while (shouldRetry && retryCount < MAX_RETRIES);
    // 处理HTTP协议级错误
    if (httpCode != 200) {
        MY_SPDLOG_ERROR("HTTP error: {}, Response: {}", httpCode, response);

        // 特殊处理401/403等认证错误
        if (httpCode == 401 || httpCode == 403) {
            MY_SPDLOG_CRITICAL("Authentication failure, check credentials");
        }
        return false;
    }

    return parseResponse(response, path);
}

bool HttpClient::parseResponse(const std::string& response, const std::string& path) {
    Json::Value root;
    JSONCPP_STRING errors;
    Json::CharReaderBuilder readerBuilder;

    std::unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());
    const char* begin = response.data();
    const char* end = begin + response.size();

    if (!reader->parse(begin, end, &root, &errors)) {
        MY_SPDLOG_ERROR("JSON parse error: {}", errors);
        return false;
    }

    int code = root.get("code", -1).asInt();
    if (code != 0) {
        MY_SPDLOG_ERROR("API error: {}", root.get("msg", "").asString());
        return false;
    }

    if (path == "/client/activate") {
        m_licenseKey = root["data"]["key"].asString();
    }
    else if (path == "/client/getCfg") {
        m_configCfg = root["data"]["cfg"].asString();
        m_configChecksums = root["data"]["checksums"].asString();
    }
    else if (path == "/client/unActivate") {
        m_licenseUnKey = root["data"]["unKey"].asString();
    }

    return true;
}

bool HttpClient::uploadFile(const std::filesystem::path& filePath) {
    // 设置 HTTP 请求头
    std::vector<std::string> headers = {
        "Content-Type: application/octet-stream",
        "Expect:"
    };

    // 初始化 CURL
    CURL* curl = curl_easy_init();
    if (!curl) {
        MY_SPDLOG_ERROR("Failed to initialize CURL handle");
        return false;
    }

    // 加锁，确保线程安全
    std::lock_guard<std::mutex> lock(m_mutex);

    // 调用私有函数处理文件上传
    bool result = performFileRequest(curl, "/client/risk/upload", filePath, headers);

    // 清理 CURL
    curl_easy_cleanup(curl);
    return result;
}

static FILE* openFile(const std::filesystem::path& filePath, const char* mode) {
#ifdef _WIN32
    return _wfopen(filePath.c_str(), L"rb"); // Windows 使用宽字符版本
#else
    return fopen(filePath.c_str(), mode); // Linux/Mac 使用标准版本
#endif
}

bool HttpClient::performFileRequest(CURL* curl, const std::string& path, const std::filesystem::path& filePath,
    const std::vector<std::string>& extraHeaders) {
    // 构造完整的 URL
    std::string url = m_baseUrl + path;
    std::string response;
    long httpCode = 0;

    struct curl_slist* headers = buildCommonHeaders();

    // 添加额外的请求头
    for (const auto& h : extraHeaders) {
        headers = curl_slist_append(headers, h.c_str());
    }

    // 设置 CURL 选项
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // 处理 HTTPS
    bool isHTTPS = false;
    size_t pos = url.find("://");
    if (pos != std::string::npos) {
        std::string scheme = url.substr(0, pos);
        std::transform(scheme.begin(), scheme.end(), scheme.begin(), ::tolower);
        isHTTPS = (scheme == "https");
    }
    else {
        MY_SPDLOG_WARN("Invalid URL format: {}", url);
    }

    // 设置 SSL 验证
    if (isHTTPS) {
        if (!m_certPath.empty()) {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(curl, CURLOPT_CAINFO, m_certPath.c_str());
            MY_SPDLOG_DEBUG("Using SSL verification with cert: {}", m_certPath);
        }
        else {
            MY_SPDLOG_ERROR("HTTPS requires certificate file");
            return false;
        }
    }
    else {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        MY_SPDLOG_TRACE("Disabled SSL verification for HTTP");
    }

    // 打开文件
    FILE* file = openFile(filePath.string().c_str(), "rb");
    if (!file) {
        MY_SPDLOG_ERROR("Failed to open file: {}", filePath.string());
        curl_slist_free_all(headers);
        return false;
    }

    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    // 设置文件上传选项
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, fread);
    curl_easy_setopt(curl, CURLOPT_READDATA, file);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(fileSize));

    // 设置错误缓冲区
    char errorBuffer[CURL_ERROR_SIZE] = { 0 };
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);

    // 设置超时
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CONNECT_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, TRANSFER_TIMEOUT);

    // 重试逻辑
    int retryCount = 0;
    bool shouldRetry = false;
    do {
        shouldRetry = false;
        CURLcode res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

        if (res != CURLE_OK) {
            MY_SPDLOG_WARN("CURL error: {} ({}), attempt {}/{}",
                          curl_easy_strerror(res), errorBuffer, retryCount + 1, MAX_RETRIES);

            // 可重试的错误
            if (res == CURLE_COULDNT_CONNECT ||
                res == CURLE_OPERATION_TIMEDOUT ||
                res == CURLE_SEND_ERROR ||
                res == CURLE_RECV_ERROR) {
                shouldRetry = true;
                retryCount++;
            }
            else {
                // 不可恢复的错误
                MY_SPDLOG_ERROR("Non-recoverable error: {} ({})",
                               curl_easy_strerror(res), errorBuffer);
                fclose(file);
                curl_slist_free_all(headers);
                return false;
            }
        }
    } while (shouldRetry && retryCount < MAX_RETRIES);

    // 关闭文件
    fclose(file);

    // 处理 HTTP 错误
    if (httpCode != 200) {
        MY_SPDLOG_ERROR("HTTP error: {}, Response: {}", httpCode, response);
        if (httpCode == 401 || httpCode == 403) {
            MY_SPDLOG_CRITICAL("Authentication failure, check credentials");
        }
        curl_slist_free_all(headers);
        return false;
    }

    // 解析响应
    curl_slist_free_all(headers);
    return parseResponse(response, path);
}

curl_slist* HttpClient::buildCommonHeaders() const {
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("x-version: " + m_version).c_str());
    headers = curl_slist_append(headers, ("x-computer-name: " + m_computerName).c_str());
    headers = curl_slist_append(headers, ("x-user-name: " + m_userName).c_str());
    headers = curl_slist_append(headers, ("x-mac: " + m_macAddress).c_str());
    headers = curl_slist_append(headers, ("x-company-code: " + m_companyCode).c_str());
    headers = curl_slist_append(headers, ("x-ca-timestamp: " + std::to_string(timestamp)).c_str());
    return headers;
}
