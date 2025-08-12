#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

#include "PlatformCompat.h"
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <sstream>
#include <codecvt>
#include <locale>


#include <algorithm>
#include <fstream>
#include <utility>

namespace CommonUtils {
    std::wstring Utf8ToWide(const std::string& utf8);
    std::string WideToUtf8(const std::wstring& wide);
    std::string string2Lower(const std::string& str);
    
    class Base64 {
    public:
        static std::string encode(const std::string& input);
        static std::string decode(const std::string& input);
    };

    class FileGuard {
    public:
        FileGuard(const std::string& fileName, std::ios::openmode mode);
        ~FileGuard();
        std::fstream& get();
    private:
        std::fstream file;
    };

    class FileHelper {
    public:
        static void writeStrToFile(const std::string& filePath, const std::string& content);
        static void getStrFromFile(const std::string& filePath, std::string& content);
    };
}



#endif // !COMMON_UTILS_H



