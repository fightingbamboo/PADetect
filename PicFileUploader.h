#ifndef PIC_FILE_UPLOADER_H
#define PIC_FILE_UPLOADER_H

#include <mutex>
#include <memory>
#include <condition_variable>
#include <atomic>
#include <thread>

#include "HttpClient.h"
#include "MyMeta.h"

class PicFileUploader
{
public:
	static PicFileUploader* getInstance() {
		static PicFileUploader instance;
		return &instance;
	};
	void start();
	void stop();
	void writePic2Disk(const std::string& inFilePath, const std::vector<uint8_t>& pic_data);
	void setUploadParam(std::shared_ptr<MyMeta> &meta);

private:
	PicFileUploader();
	~PicFileUploader();
	void scanThread();
	PicFileUploader(const PicFileUploader&) = delete;
	PicFileUploader& operator=(const PicFileUploader&) = delete;
private:
	std::string m_scanPath{ "picDataDir" };
	std::atomic_bool m_scanContinue{ false };
	//std::queue<std::string> m_picPathVec{};
	HttpClient* m_httpClient{ nullptr };
	int32_t m_scanInterval{ 60000 }; //ms

	std::thread m_scanThd;
	std::mutex m_scanMtx;
	std::condition_variable m_scanCond;
};


#endif // !PIC_FILE_UPLOADER_H



