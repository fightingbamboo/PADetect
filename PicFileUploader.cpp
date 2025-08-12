#include <filesystem>
#include <istream>
#include <fstream>
#include <vector>
#include <sstream>

#include "PicFileUploader.h"
#include "KeyVerifier.h"
#include "MyLogger.hpp"
#include "CommonUtils.h"

namespace fs = std::filesystem;

PicFileUploader::PicFileUploader()
{
}

PicFileUploader::~PicFileUploader()
{
}

void PicFileUploader::start()
{
    m_httpClient = HttpClient::getInstance();
    m_scanContinue.store(true);
    m_scanThd = std::move(std::thread(&PicFileUploader::scanThread, this));
}

void PicFileUploader::stop()
{
    m_scanContinue.store(false);
    m_scanCond.notify_all();
    if (m_scanThd.joinable()) { m_scanThd.join(); }
}

void PicFileUploader::scanThread()
{
    MY_SPDLOG_DEBUG(">>>");
#if ONLINE_MODE
    while (m_scanContinue.load()) {
        try {
            std::vector<fs::path> filesToUpload;
            {
                std::unique_lock<std::mutex> lock(m_scanMtx);
                for (const auto& entry : fs::directory_iterator("./data")) {
                    if (entry.is_regular_file()) {
                        filesToUpload.emplace_back(entry.path());
                    }
                }
            }
            for (const auto &file : filesToUpload) {
                if (m_httpClient->uploadFile(file)) {
                    fs::remove(file);
                    MY_SPDLOG_DEBUG("upload file: {} success", file.string());
                }
                else {
                    MY_SPDLOG_ERROR("upload file: {} false", file.string());
                }
            }
        }
        catch (const std::exception &e) {
            MY_SPDLOG_ERROR("scan upload file exception: {}", e.what());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(m_scanInterval));
    }
#endif
    MY_SPDLOG_DEBUG("<<<");
}

void PicFileUploader::writePic2Disk(const std::string& inFilePath, const std::vector<uint8_t> &pic_data)
{

    std::unique_lock<std::mutex> lock(m_scanMtx);
    std::ofstream out(inFilePath, std::ios::binary);
    if (out) {
        out.write(reinterpret_cast<const char*>(pic_data.data()), pic_data.size());
    }
    out.close();
    MY_SPDLOG_DEBUG("write pic file into : {}", inFilePath);

}

void PicFileUploader::setUploadParam(std::shared_ptr<MyMeta>& meta) {
    m_scanInterval = meta->getInt32OrDefault("upload_interval", m_scanInterval);

    MY_SPDLOG_DEBUG("上传参数更新: scan_interval={}", m_scanInterval);
}
