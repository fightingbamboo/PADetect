#ifndef YOLO_V3_DETECTOR_H
#define YOLO_V3_DETECTOR_H

#include <opencv2/opencv.hpp>
#include <openvino/openvino.hpp>
#include <json/json.h>
#include <iostream>
#include <memory>
#include <iomanip>
#include <shared_mutex>

#include "MyMeta.h"
#include "ConfigParser.h"


class YOLOv3Detector : public IConfigUpdateListener {
public:
    static YOLOv3Detector* getInstance() {
        static YOLOv3Detector instance;
        return &instance;
    }

    bool Initialize(const std::string& model_path, const std::string& config_path,
        const std::string& pipeline_path, const std::string& device = "CPU");

    void detect(const cv::Mat& frame, uint32_t& lenCnt, uint32_t& phoneCnt,
        uint32_t& faceCnt, uint32_t& suspectedCnt);

    void setDetectParam(std::shared_ptr<MyMeta> &meta);

    void setImgDebugMode(bool imgDebugMode = true);
    void onConfigUpdated(std::shared_ptr<MyMeta>& newMeta);

private:
    YOLOv3Detector() = default;
    ~YOLOv3Detector() = default;
    YOLOv3Detector(const YOLOv3Detector&) = delete;
    YOLOv3Detector& operator=(const YOLOv3Detector&) = delete;

    void ParseConfig(const Json::Value& root);
    void ParsePipeline(const Json::Value& root);
    void PreprocessImage(const cv::Mat& src, cv::Mat& dst,
        float& scaleFactor, int& padTop, int& padLeft) const;
    void PreprocessImage(const cv::Mat& src); // 修改后的预处理函数

    cv::Mat m_processed;   // 预分配的预处理图像缓冲区
    cv::Mat m_resized;     // 预分配的缩放图像缓冲区
    cv::Rect m_targetROI;
    float m_scaleFactor = 0.0;   // 缓存缩放因子
    int m_padTop = 0;          // 缓存顶部填充
    int m_padLeft = 0;         // 缓存左侧填充
    int m_padBottom = 0;       // 缓存底部填充
    int m_padRight = 0;        // 缓存右侧填充
    cv::Size m_newSize;          // 缩放后尺寸（新增）
    bool m_preprocessInitialized = false; // 标记是否已初始化预处理参数

    // OpenVINO相关成员
    ov::Core m_core;
    std::shared_ptr<ov::Model> m_model;
    ov::CompiledModel m_compiled_model;
    ov::InferRequest m_infer_request;
    bool m_initialized = false;

    // 预处理参数
    std::vector<float> m_mean = { 123.675f, 116.28f, 103.53f };
    std::vector<float> m_std = { 58.395f, 57.12f, 57.375f };
    cv::Size m_targetSize = cv::Size(320, 320);

    // 后处理参数
    float m_score_threshold = 0.05f;
    int m_keep_top_k = 100;

    // 设备信息
    std::string m_device;

    // param relate
    std::shared_mutex m_paramMtx;
    float m_scoreFilterLenHigh{ 0.66f }, m_scoreFilterLenLow{ 0.36f },
        m_scoreFilterPhoneHigh{ 0.93f }, m_scoreFilterPhoneLow{ 0.83f }, m_scoreFilterFace{ 0.36f };
    int32_t m_labelFilterLen{ 1 }, m_labelFilterPhone{ 2 }, m_labelFilterFace{ 0 };

    bool m_imgDebugMode{ false };
};

#endif // YOLO_V3_DETECTOR_H