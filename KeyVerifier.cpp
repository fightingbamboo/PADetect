#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/buffer.h>

#include "KeyVerifier.h"
#include "CommonUtils.h"
#include "HttpClient.h"
#include "MyLogger.hpp"
#include "obfuscate.h"
#include "PlatformCompat.h"
#include "DeviceInfo.h"

#define DEPLOY_PRIVATE 1

#if DEPLOY_PRIVATE
static constexpr const char* companyId = "ve42qymz";
#else
static constexpr const char* companyId = "rHNgHc24";
#endif

static const std::string keyPath = "key.txt";
static const std::string unKeyPath = "unKey.txt";

static const char* MY_KEY = AY_OBFUSCATE("k6N2pMk");
static const char* MY_SALT = AY_OBFUSCATE("QJ2ccMl");
static const char* MY_SEED = AY_OBFUSCATE("622");

/*********************************get mac hmac code*********************************/
class HmacMacHasher {
public:
    enum class HashAlgorithm {
        SHA256,
        SHA512
    };

    explicit HmacMacHasher(HashAlgorithm algo = HashAlgorithm::SHA256);
    ~HmacMacHasher();

    std::string calculateHash(const std::string& macAddress,
                            const std::string& key,
                            const std::string& salt);

private:
    const EVP_MD* getHashAlgorithm() const;
    std::string normalizeMacAddress(const std::string& mac) const;
    bool validateMacAddress(const std::string& mac) const;

    HashAlgorithm m_hash_algorithm;
};

#include <cctype>
#include <sstream>
#include <iomanip>
HmacMacHasher::HmacMacHasher(HashAlgorithm algo)
    : m_hash_algorithm(algo) {
    OpenSSL_add_all_digests();
}

HmacMacHasher::~HmacMacHasher() {
    EVP_cleanup();
}

std::string HmacMacHasher::calculateHash(
    const std::string& macAddress,
    const std::string& key,
    const std::string& salt) {

    if (!validateMacAddress(macAddress)) {
        MY_SPDLOG_ERROR("Invalid MAC address format: {}", macAddress);
        throw std::invalid_argument("Invalid MAC address format");
    }

    std::string normalizedMac = normalizeMacAddress(macAddress);
    std::string saltedInput = salt + normalizedMac;

    const EVP_MD* md = getHashAlgorithm();
    unsigned int hashLen = EVP_MD_size(md);
    std::vector<unsigned char> hash(hashLen);

    HMAC(md,
        key.data(), key.length(),
        reinterpret_cast<const unsigned char*>(saltedInput.data()), saltedInput.length(),
        hash.data(), &hashLen);

    std::stringstream ss;
    for (unsigned char byte : hash) {
        ss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(byte);
    }

    MY_SPDLOG_DEBUG("HMAC calculated for MAC: {}, algo: {}",
                   macAddress, m_hash_algorithm == HashAlgorithm::SHA256 ? "SHA256" : "SHA512");
    return ss.str();
}

const EVP_MD* HmacMacHasher::getHashAlgorithm() const {
    switch (m_hash_algorithm) {
    case HashAlgorithm::SHA256: return EVP_sha256();
    case HashAlgorithm::SHA512: return EVP_sha512();
    default: return EVP_sha256();
    }
}

std::string HmacMacHasher::normalizeMacAddress(const std::string& mac) const {
    std::string result;
    for (char c : mac) {
        if (c != '-' && c != ':') {
            result += std::tolower(c);
        }
    }
    return result;
}

bool HmacMacHasher::validateMacAddress(const std::string& mac) const {
    int hexCount = 0;
    int separatorCount = 0;

    for (char c : mac) {
        if (isxdigit(c)) {
            hexCount++;
        }
        else if (c == '-' || c == ':') {
            separatorCount++;
        }
        else {
            return false;
        }
    }

    return (hexCount == 12) && (separatorCount == 5);
}

/*********************************time insert random*********************************/
std::string vectorToString(const std::vector<size_t>& vec) {
    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < vec.size(); ++i) {
        ss << vec[i];
        if (i != vec.size() - 1) {
            ss << ", ";
        }
    }
    ss << "]";
    return ss.str();
}

#include <random>
#include <numeric>
#include <algorithm>
class RandomSequenceGenerator {
public:
    explicit RandomSequenceGenerator(uint64_t seed);
    ~RandomSequenceGenerator() = default;

    // 生成固定范围的随机序列
    std::string generateTimeLicense(const std::string& timeBase64Encode,
        const std::string& macHashCode) const;
    std::pair<std::string, std::string> extractLicense(const std::string& licenseStr, size_t macHashLen) const;

private:
    uint64_t m_seed;
private:
    RandomSequenceGenerator(const RandomSequenceGenerator&) = delete;
    RandomSequenceGenerator& operator=(const RandomSequenceGenerator&) = delete;
};

RandomSequenceGenerator::RandomSequenceGenerator(uint64_t seed)
    : m_seed(seed)
{

}

template <typename T>
std::string vectorToString(const std::vector<T>& vec, size_t maxItems = 16) {
    if (vec.empty())
        return "[]";

    std::ostringstream oss;
    oss << "[";

    // 打印前几个元素
    size_t count = (std::min)(vec.size(), maxItems);
    for (size_t i = 0; i < count; ++i) {
        if (i > 0)
            oss << ", ";
        oss << vec[i];
    }

    // 如果元素太多，添加省略号
    if (vec.size() > maxItems) {
        oss << ", ... (+" << (vec.size() - maxItems) << " more)";
    }

    oss << "] (size=" << vec.size() << ")";
    return oss.str();
}

template <typename RandomIt, typename RNG>
void cross_platform_shuffle(RandomIt first, RandomIt last, RNG&& rng) {
    if (first == last) return;

    using diff_t =
        typename std::iterator_traits<RandomIt>::difference_type;

    diff_t n = last - first;
    for (diff_t i = n - 1; i > 0; --i) {
        // 关键修改：直接取模确保可移植性
        diff_t j = static_cast<diff_t>(rng() % (i + 1));

        if (j != i) {
            std::iter_swap(first + i, first + j);
        }
    }
}

std::string RandomSequenceGenerator::generateTimeLicense(
    const std::string& timeBase64Encode, const std::string& macHashCode) const {
    if (timeBase64Encode.empty() || macHashCode.empty()) {
        MY_SPDLOG_ERROR("Invalid input strings");
        return "";
    }

    const size_t totalLen = timeBase64Encode.size() + macHashCode.size();

    // 生成随机插入位置
    std::vector<size_t> indices(totalLen);
    std::iota(indices.begin(), indices.end(), 0);

    std::mt19937 rng(m_seed);
    cross_platform_shuffle(indices.begin(), indices.end(), rng);

    // 取前N个位置并排序
    std::vector<size_t> insertPositions(
        indices.begin(), indices.begin() + timeBase64Encode.size());
    std::sort(insertPositions.begin(), insertPositions.end());
    //MY_SPDLOG_DEBUG("insertPositions : {}", fmt::join(insertPositions, ", "));
    MY_SPDLOG_DEBUG("insertPositions : {}", vectorToString(insertPositions));

    // 构建结果字符串
    std::string result;
    result.reserve(totalLen);

    size_t timeIdx = 0;
    size_t macIdx = 0;

    for (size_t pos = 0; pos < totalLen; ++pos) {
        if (timeIdx < insertPositions.size() && pos == insertPositions[timeIdx]) {
            result += timeBase64Encode[timeIdx++];
        }
        else {
            result += macHashCode[macIdx++];
        }
    }

    // 验证完全填充
    if (timeIdx != timeBase64Encode.size() || macIdx != macHashCode.size()) {
        MY_SPDLOG_ERROR("Incomplete fill: timeIdx={}, macIdx={}", timeIdx, macIdx);
        return "";
    }

    return result;
}

std::pair<std::string, std::string>
RandomSequenceGenerator::extractLicense(const std::string& licenseStr,
                                        size_t macHashLen) const {
    const size_t totalLen = licenseStr.size();

    // 验证输入有效性
    if (totalLen == 0 || macHashLen >= totalLen) {
        MY_SPDLOG_ERROR("Invalid input: license length={}, macHashLen={}", totalLen,
                        macHashLen);
        return { "", "" };
    }

    const size_t timeLen = totalLen - macHashLen;

    // 重新生成相同的随机位置序列
    std::vector<size_t> indices(80);
    std::iota(indices.begin(), indices.end(), 0);

    std::mt19937 rng(m_seed);
    cross_platform_shuffle(indices.begin(), indices.end(), rng);

    // 获取时间字符串的位置并排序
    std::vector<size_t> timePositions(indices.begin(), indices.begin() + 16);
    std::sort(timePositions.begin(), timePositions.end());
    //MY_SPDLOG_INFO("timePositions: {}", fmt::join(timePositions, ", "));
    MY_SPDLOG_DEBUG("insertPositions : {}", vectorToString(timePositions));

    // 准备提取结果
    std::string timeBase64;
    std::string macHash;
    timeBase64.reserve(timeLen);
    macHash.reserve(macHashLen);

    // 创建时间位置迭代器
    auto timePosIt = timePositions.begin();
    const auto timePosEnd = timePositions.end();

    // 遍历整个许可证字符串
    for (size_t i = 0; i < totalLen; ++i) {
        // 检查当前位置是否是时间字符串位置
        if (timePosIt != timePosEnd && i == *timePosIt) {
            // 提取时间字符
            timeBase64 += licenseStr[i];
            ++timePosIt;
        }
        else {
            // 提取MAC哈希字符
            macHash += licenseStr[i];
        }
    }

    // 验证长度
    if (timeBase64.size() != timeLen || macHash.size() != macHashLen) {
        MY_SPDLOG_ERROR("Extraction size mismatch: time={}/{} mac={}/{}",
                        timeBase64.size(), timeLen, macHash.size(), macHashLen);
        return { "", "" };
    }

    return { timeBase64, macHash };
}

/*static*/ std::string HashCalculator::CalculateSHA512(const std::string & input) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int lengthOfHash = 0;

    std::unique_ptr<EVP_MD_CTX, EVP_MD_CTX_Deleter> context(EVP_MD_CTX_new());
    if (!context) {
        throw std::runtime_error("Failed to create EVP_MD_CTX.");
    }

    if (EVP_DigestInit_ex(context.get(), EVP_sha512(), nullptr) != 1) {
        throw std::runtime_error("EVP_DigestInit_ex failed.");
    }

    if (EVP_DigestUpdate(context.get(), input.c_str(), input.length()) != 1) {
        throw std::runtime_error("EVP_DigestUpdate failed.");
    }

    if (EVP_DigestFinal_ex(context.get(), hash, &lengthOfHash) != 1) {
        throw std::runtime_error("EVP_DigestFinal_ex failed.");
    }

    std::stringstream ss;
    for (unsigned int i = 0; i < lengthOfHash; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

KeyVerifier::KeyVerifier(const std::string& filePath) {
    CommonUtils::FileHelper::getStrFromFile(filePath, mEmbeddedString);
}

bool KeyVerifier::Verify() {
    try {
        // get all macs available
        std::vector<std::string> macAddrVec = DeviceInfo::getInstance()->getMacAddresses();
        for (const std::string& mac : macAddrVec) {
            MY_SPDLOG_TRACE("MAC: {}", mac);

            // calculate current host's mac's hash
            std::unique_ptr<HmacMacHasher> sha256Hasher = std::make_unique<HmacMacHasher>
                (HmacMacHasher::HashAlgorithm::SHA256);
            std::string curMacHash = sha256Hasher->calculateHash(mac, MY_KEY, MY_SALT);

            // extract time and mac from remote server
            int64_t randomSeed = std::strtoll(MY_SEED, NULL, 10);
            std::unique_ptr<RandomSequenceGenerator> gen = std::make_unique<RandomSequenceGenerator>(randomSeed);
            auto [timeBase64Enc, macSha256Hash] = gen->extractLicense(mEmbeddedString, curMacHash.size());

            if (curMacHash == macSha256Hash) {
                // get current system clock
                auto now = std::chrono::system_clock::now();
                time_t now_timestamp = std::chrono::system_clock::to_time_t(now);

                // judge current system clock is expired or not
                timeBase64Enc = CommonUtils::Base64::decode(timeBase64Enc);
                int64_t expTime = std::stoll(timeBase64Enc);
                if (now_timestamp < expTime) {
                    MY_SPDLOG_INFO("Verification successful!, expire time: {}", expTime);
                    return true;
                }
            }
        }
        MY_SPDLOG_ERROR("Verification failed!");
        return false;
    }
    catch (const std::exception& e) {
        MY_SPDLOG_CRITICAL("Verification exception: {}", e.what());
        return false;
    }
}

constexpr uint32_t timeStampStrLen = 16;
constexpr uint32_t hashStrLen = 128;

void KeyVerifier::extractTimestampAndHash(const std::string& mEmbeddedString, std::string& extractedTimestamp,
    std::string& extractedHash, int32_t insertIdx) const {
    size_t totalLen = mEmbeddedString.length();
    extractedTimestamp.resize(timeStampStrLen, '\0');
    extractedHash.resize(hashStrLen, '\0');

    uint32_t timeIdx = 0, hashIdx = 0;
    for (uint32_t i = 0; i < totalLen; ++i) {
        if (i >= insertIdx && i < insertIdx + timeStampStrLen) {
            extractedTimestamp[timeIdx++] = mEmbeddedString[i];
        }
        else {
            extractedHash[hashIdx++] = mEmbeddedString[i];
        }
    }
    MY_SPDLOG_TRACE("extractedTimestamp: {}, extractedHash: {}, insertIdx: {}", extractedTimestamp, extractedHash, insertIdx);
}

std::string trim(const std::string& str) {
    auto first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    auto last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}





KeySubscriber::KeySubscriber() {

}

KeySubscriber::~KeySubscriber() {
}

void KeySubscriber::setHttpParam(std::shared_ptr<MyMeta>& meta) {
    const std::string mac = DeviceInfo::getInstance()->getFirstMacAddress();
    HttpClient* hc = HttpClient::getInstance();
    if (!hc) {
        throw std::runtime_error("get http client instance point failed");
    }

    const std::string computerName = DeviceInfo::getInstance()->getComputerName();
    const std::string userNmae = DeviceInfo::getInstance()->getUserName();
    if (computerName.empty()) {
        throw std::runtime_error("get computer name failed");
    }

    std::string baseUrl = meta->getStringOrDefault("base_url", "172.17.66.130:18000");
    std::string clientVer = meta->getStringOrDefault("client_version", "1.0.0");
    std::string certFilePath = meta->getStringOrDefault("cert_file_path", "ca-bundle.crt");

    MY_SPDLOG_DEBUG("computerName = {}, userNmae = {}, mac = {}, companyId = {}, baseUrl={}, clientVer={}, certFilePath={}",
        computerName, userNmae, mac, companyId, baseUrl, clientVer, certFilePath);

    hc->setHttpClientParam(computerName, userNmae, mac, companyId, baseUrl, clientVer, certFilePath);

    return;
}

bool KeySubscriber::subscribeForKey() {
    HttpClient* hc = HttpClient::getInstance();
    if (!hc->requestKey()) {
        return false;
    }
    CommonUtils::FileHelper::writeStrToFile(keyPath, hc->getLicenseKey());
    return true;
}

bool KeySubscriber::subscribeForUnKey() {
    HttpClient* hc = HttpClient::getInstance();
    if (!hc->requestUnKey()) {
        return false;
    }
    CommonUtils::FileHelper::writeStrToFile(unKeyPath, hc->getLicenseUnKey());
    return true;
}

#if 0
/*********************************test*********************************/
int main() {
    MySpdlog* myLog = MySpdlog::getInstance();
    myLog->init();
    myLog->setLogEnabled(true);
    myLog->setLogLevel(spdlog::level::level_enum::trace);
    std::vector<std::string> macAddresses = GetMacAddresses();
    if (macAddresses.empty()) {
        return 1;
    }
    for (const std::string& mac : macAddresses) {
        if (!mac.empty()) {
            MY_SPDLOG_DEBUG("MAC Address: {}", mac.c_str());
            break;
        }
    }

    std::string timeEncoded = "";
    try {
        // 获取Unix时间戳作为测试数据
        time_t timestamp = time(nullptr);
        std::string original = std::to_string(timestamp);

        MY_SPDLOG_INFO("Original: {}", original);

        // 编码测试
        std::string encoded = base64_encode(original);
        timeEncoded = encoded;
        MY_SPDLOG_INFO("Encoded: {}", encoded);

        // 解码测试
        std::string decoded = base64_decode(encoded);
        MY_SPDLOG_INFO("Decoded: {}", decoded);

        // 验证
        if (original == decoded) {
            MY_SPDLOG_INFO("Validation: SUCCESS");
        }
        else {
            MY_SPDLOG_ERROR("Validation: FAILED");
            return 1;
        }
    }
    catch (const std::exception& e) {
        MY_SPDLOG_ERROR("Error: {}", e.what());
        return 1;
    }

    std::string& firstMac = macAddresses[0];
    std::string sha256hashCode = "";
    try {
        MY_SPDLOG_INFO("Starting HMAC MAC address hashing test>>>>>>>>>");

        HmacMacHasher hasher(HmacMacHasher::HashAlgorithm::SHA256);

        std::string mac = "00-1A-2B-3C-4D-5E";
        std::string key = "secret_key_123";
        std::string salt = "dynamic_salt_xyz";

        MY_SPDLOG_DEBUG("Testing with MAC: {}, key: {}, salt: {}", mac, key, salt);

        std::string hash = hasher.calculateHash(mac, key, salt);
        sha256hashCode = hash;
        MY_SPDLOG_INFO("HMAC-SHA256 of {}: {}", mac, hash);

        HmacMacHasher sha512Hasher(HmacMacHasher::HashAlgorithm::SHA512);
        hash = sha512Hasher.calculateHash(mac, key, salt);
        MY_SPDLOG_INFO("HMAC-SHA512 of {}: {}", mac, hash);

        MY_SPDLOG_INFO("<<<<<<<<<Test completed successfully");
    }
    catch (const std::exception& e) {
        MY_SPDLOG_ERROR("Test failed: {}", e.what());
        return 1;
    }

    MY_SPDLOG_INFO("Begin test random>>>>>>>>>>>>>>>>>>");
    RandomSequenceGenerator gen(23156156156165);
    std::string license = gen.generateTimeLicense(timeEncoded, sha256hashCode);

    std::string extractTimeCode = "", extractMacCode = "";
    gen.extractTimeLicense(license, extractTimeCode, extractMacCode);

    std::string decodeTime = base64_decode(extractTimeCode);
    std::string mac = "00-1A-2B-3C-4D-5E";
    std::string key = "secret_key_123";
    std::string salt = "dynamic_salt_xyz";
    HmacMacHasher hasher(HmacMacHasher::HashAlgorithm::SHA256);
    std::string hash = hasher.calculateHash(mac, key, salt);

    if (hash == extractMacCode) {
        MY_SPDLOG_INFO("mac check ok");
    }

    MY_SPDLOG_DEBUG("decode time: {}", decodeTime);

    MY_SPDLOG_INFO("<<<<<<<<<<<<<<<<<<End test random");
    return 0;
}
#endif