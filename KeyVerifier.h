#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <string>
#include <memory>
#include <vector>
#include <algorithm>
#include <random>
#include <set>
#include <stdexcept>
#include "PlatformCompat.h"
// Windows-specific includes
#define NOMINMAX
#if PLATFORM_WINDOWS
#include <Windows.h>
#include <iphlpapi.h>
#endif
// OpenSSL specific includes
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

#pragma comment(lib, "iphlpapi.lib")

#include "MyMeta.h"

class HashCalculator {
public:
    static std::string CalculateSHA512(const std::string& input);
private:
    struct EVP_MD_CTX_Deleter {
        void operator()(EVP_MD_CTX* ctx) const {
            if (ctx != nullptr) {
                EVP_MD_CTX_free(ctx);
            }
        }
    };
};

class KeyVerifier {
public:
    explicit KeyVerifier(const std::string& filePath);
    bool Verify();
private:
    void extractTimestampAndHash(const std::string& mEmbeddedString, std::string& extractedTimestamp, std::string& extractedHash, int32_t seed) const;
    std::string mEmbeddedString;
    static constexpr int32_t mInsertIdx = 10;
};

class KeySubscriber {
public:
    explicit KeySubscriber();
    ~KeySubscriber();
    void setHttpParam(std::shared_ptr<MyMeta>& meta);
    bool subscribeForKey();
    bool subscribeForUnKey();
};
