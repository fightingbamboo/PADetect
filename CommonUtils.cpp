#include "CommonUtils.h"
#include "MyLogger.hpp"
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

namespace CommonUtils {
#if PLATFORM_WINDOWS
    std::wstring Utf8ToWide(const std::string& utf8) {
        if (utf8.empty()) return L"";

        const int count = MultiByteToWideChar(
            CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);

        if (count <= 0) return L"";

        std::wstring wide;
        wide.resize(count - 1);
        MultiByteToWideChar(
            CP_UTF8, 0, utf8.c_str(), -1,
            &wide[0], count);

        return wide;
    }

    std::string WideToUtf8(const std::wstring& wide) {
        if (wide.empty()) return "";

        const int utf8_size = WideCharToMultiByte(
            CP_UTF8, 0, wide.c_str(), -1,
            nullptr, 0, nullptr, nullptr);

        if (utf8_size == 0) return "";

        std::string utf8(utf8_size, 0);
        WideCharToMultiByte(
            CP_UTF8, 0, wide.c_str(), -1,
            &utf8[0], utf8_size, nullptr, nullptr);
        utf8.resize(utf8_size - 1);

        return utf8;
    }
#else
    std::wstring Utf8ToWide(const std::string& utf8) {
        if (utf8.empty()) return L"";
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return converter.from_bytes(utf8);
    }

    std::string WideToUtf8(const std::wstring& wide) {
        if (wide.empty()) return "";
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return converter.to_bytes(wide);
    }
#endif

    std::string string2Lower(const std::string& str) {
        std::string lowerStr = str;
        std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
        return lowerStr;
    }

    /*static*/
    std::string Base64::encode(const std::string& input) {
        BIO* bio = nullptr, * b64 = nullptr;
        BUF_MEM* bufferPtr = nullptr;

        try {
            b64 = BIO_new(BIO_f_base64());
            if (!b64) {
                MY_SPDLOG_ERROR("BIO_new failed in base64_encode");
                throw std::runtime_error("BIO_new failed");
            }

            bio = BIO_new(BIO_s_mem());
            if (!bio) {
                BIO_free(b64);
                MY_SPDLOG_ERROR("BIO_new failed in base64_encode");
                throw std::runtime_error("BIO_new failed");
            }

            bio = BIO_push(b64, bio);
            BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

            if (BIO_write(bio, input.c_str(), input.length()) <= 0) {
                BIO_free_all(bio);
                MY_SPDLOG_ERROR("BIO_write failed in base64_encode");
                throw std::runtime_error("BIO_write failed");
            }

            if (BIO_flush(bio) != 1) {
                BIO_free_all(bio);
                MY_SPDLOG_ERROR("BIO_flush failed in base64_encode");
                throw std::runtime_error("BIO_flush failed");
            }

            BIO_get_mem_ptr(bio, &bufferPtr);
            std::string encoded(bufferPtr->data, bufferPtr->length);
            BIO_free_all(bio);

            MY_SPDLOG_DEBUG("Base64 encoded successfully");
            return encoded;
        }
        catch (...) {
            if (bio) BIO_free_all(bio);
            else if (b64) BIO_free(b64);
            throw;
        }
    }

    /*static*/
    std::string Base64::decode(const std::string& input) {
        BIO* bio = nullptr, * b64 = nullptr;
        std::vector<char> buffer(input.length());

        try {
            b64 = BIO_new(BIO_f_base64());
            if (!b64) {
                MY_SPDLOG_ERROR("BIO_new failed in base64_decode");
                throw std::runtime_error("BIO_new failed");
            }

            bio = BIO_new_mem_buf(input.c_str(), input.length());
            if (!bio) {
                BIO_free(b64);
                MY_SPDLOG_ERROR("BIO_new_mem_buf failed in base64_decode");
                throw std::runtime_error("BIO_new_mem_buf failed");
            }

            bio = BIO_push(b64, bio);
            BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

            int length = BIO_read(bio, buffer.data(), buffer.size());
            if (length < 0) {
                BIO_free_all(bio);
                MY_SPDLOG_ERROR("BIO_read failed in base64_decode");
                throw std::runtime_error("BIO_read failed");
            }

            BIO_free_all(bio);
            std::string decoded(buffer.data(), length);

            MY_SPDLOG_DEBUG("Base64 decoded successfully, input size: {}, output size: {}",
                           input.length(), decoded.length());
            return decoded;
        }
        catch (...) {
            if (bio) BIO_free_all(bio);
            else if (b64) BIO_free(b64);
            throw;
        }
    }

    FileGuard::FileGuard(const std::string& fileName, std::ios::openmode mode) : file(fileName, mode) {
        if (!file.is_open()) {
            throw std::runtime_error("Error opening file: " + fileName);
        }
    }

    std::fstream& FileGuard::get() {
        return file;
    }

    FileGuard::~FileGuard() {
        if (file.is_open()) {
            file.close();
        }
    }

    void FileHelper::writeStrToFile(const std::string& filePath, const std::string& content) {
        try {
            FileGuard outFileGuard(filePath, std::ios::out | std::ios::trunc);
            outFileGuard.get() << content;
            if (!outFileGuard.get().good()) {
                throw std::runtime_error("Failed to write data to file.");
            }
        }
        catch (const std::exception& e) {
            throw e;
        }
    }

    void FileHelper::getStrFromFile(const std::string& filePath, std::string& content) {
        try {
            FileGuard guard(filePath, std::ios::in);
            std::fstream& fileStream = guard.get();
            if (!std::getline(fileStream, content)) {
                if (fileStream.eof()) { //文件为空
                    return;
                }
                else { // 读取失败或
                    throw std::runtime_error("Failed to read from file");
                }
            }
        }
        catch (const std::runtime_error& e) {
            throw e;
        }
    }

}