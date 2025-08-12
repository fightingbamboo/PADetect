#include "YOLOv3Detector.h"
#include "MyLogger.hpp"
#include <fstream>
#include <sstream>
#include <cmath>
#include <filesystem>

bool YOLOv3Detector::Initialize(const std::string& model_path,
    const std::string& config_path,
    const std::string& pipeline_path,
    const std::string& device) {
    if (m_initialized) return true;

    m_device = device;
    m_initialized = false;

    // 解析配置文件
    Json::Value config_root;
    std::ifstream config_file(config_path);
    if (!config_file.is_open()) {
        MY_SPDLOG_CRITICAL("Could not open config file");
        return false;
    }
    config_file >> config_root;
    ParseConfig(config_root);

    // 解析pipeline文件
    Json::Value pipeline_root;
    std::ifstream pipeline_file(pipeline_path);
    if (!pipeline_file.is_open()) {
        MY_SPDLOG_CRITICAL("Could not open pipeline file");
        return false;
    }
    pipeline_file >> pipeline_root;
    ParsePipeline(pipeline_root);

    try {
        // 设置GPU缓存目录
        const std::string cacheDir = "cache/gpu_cache";
        if (!std::filesystem::exists(cacheDir)) {
            std::filesystem::create_directories(cacheDir);
        }
        m_core.set_property("GPU", ov::cache_dir(cacheDir));

        // 读取模型
        m_model = m_core.read_model(model_path);

        // 配置预处理
        ov::preprocess::PrePostProcessor ppp(m_model);

        // 配置输入张量参数 (修正1: 正确设置NHWC布局)
        ppp.input().tensor()
            .set_element_type(ov::element::u8)
            .set_layout("NHWC")
            .set_color_format(ov::preprocess::ColorFormat::BGR);

        // 配置输入预处理步骤 (修正2: 添加BGR到RGB转换)
        ppp.input().preprocess()
            .convert_element_type(ov::element::f32)
            .convert_color(ov::preprocess::ColorFormat::RGB)  // BGR转RGB
            .mean(m_mean)
            .scale(m_std)
            .convert_layout("NCHW");

        // 配置模型输入布局
        ppp.input().model().set_layout("NCHW");

        // 配置输出
        ppp.output("dets").tensor().set_element_type(ov::element::f32);
        ppp.output("labels").tensor().set_element_type(ov::element::i64);

        // 应用预处理
        m_model = ppp.build();

        // 编译模型 - 使用指定设备
        try {
            // 设备自适应逻辑 (修正3: 简化设备选择)
            if (m_device == "AUTO" || m_device == "GPU") {
                try {
                    m_compiled_model = m_core.compile_model(m_model, "GPU");
                    MY_SPDLOG_INFO("Model compiled for GPU");
                }
                catch (const ov::Exception& e) {
                    MY_SPDLOG_ERROR("GPU device failed: {}", e.what());
                    m_compiled_model = m_core.compile_model(m_model, "CPU");
                    MY_SPDLOG_WARN("Fallback to CPU");
                }
            }
            else {
                m_compiled_model = m_core.compile_model(m_model, m_device);
            }
        }
        catch (const ov::Exception& e) {
            MY_SPDLOG_ERROR("Compilation error: {}", e.what());
            return false;
        }

        // 创建推理请求
        m_infer_request = m_compiled_model.create_infer_request();

        m_initialized = true;
        return true;
    }
    catch (const std::exception& e) {
        MY_SPDLOG_CRITICAL("Initialization error: {}", e.what());
        return false;
    }
}

void YOLOv3Detector::ParseConfig(const Json::Value& root) {
    // 解析后处理参数
    if (root["codebase_config"].isMember("post_processing")) {
        const Json::Value& post_processing = root["codebase_config"]["post_processing"];
        m_score_threshold = post_processing["score_threshold"].asFloat();
        m_keep_top_k = post_processing["keep_top_k"].asInt();
    }
}

void YOLOv3Detector::ParsePipeline(const Json::Value& root) {
    // 解析预处理参数
    const Json::Value& tasks = root["pipeline"]["tasks"];
    for (const auto& task : tasks) {
        if (task["name"].asString() == "Preprocess") {
            const Json::Value& transforms = task["transforms"];
            for (const auto& transform : transforms) {
                std::string type = transform["type"].asString();
                if (type == "Resize") {
                    const Json::Value& size = transform["size"];
                    m_targetSize.width = size[0].asInt();
                    m_targetSize.height = size[1].asInt();
                }
                else if (type == "Normalize") {
                    // 解析归一化参数
                    const Json::Value& mean = transform["mean"];
                    const Json::Value& std_val = transform["std"];
                    for (int i = 0; i < 3; i++) {
                        m_mean[i] = mean[i].asFloat();
                        m_std[i] = std_val[i].asFloat();
                    }
                }
            }
            break;
        }
    }
}

void YOLOv3Detector::PreprocessImage(const cv::Mat& src, cv::Mat& dst,
    float& scaleFactor, int& padTop, int& padLeft) const {
    // 计算缩放比例
    float scale = (std::min)(static_cast<float>(m_targetSize.width) / src.cols,
        static_cast<float>(m_targetSize.height) / src.rows);
    scaleFactor = scale;

    // 保持宽高比缩放
    cv::Size newSize(static_cast<int>(src.cols * scale),
        static_cast<int>(src.rows * scale));
    cv::Mat resized;
    cv::resize(src, resized, newSize, 0, 0, cv::INTER_LINEAR);

    // 计算填充
    padTop = (m_targetSize.height - newSize.height) / 2;
    padLeft = (m_targetSize.width - newSize.width) / 2;
    int padBottom = m_targetSize.height - resized.rows - padTop;
    int padRight = m_targetSize.width - resized.cols - padLeft;

    // 应用填充 (使用黑色边框)
    cv::copyMakeBorder(resized, dst, padTop, padBottom, padLeft, padRight,
        cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
}

void YOLOv3Detector::PreprocessImage(const cv::Mat& src) {
    // 懒初始化：首次调用时计算参数并分配内存
    if (!m_preprocessInitialized) {
        // 计算缩放比例
        m_scaleFactor = (std::min) (
            static_cast<float>(m_targetSize.width) / src.cols,
            static_cast<float>(m_targetSize.height) / src.rows
        );

        // 计算缩放后的尺寸
        m_newSize = cv::Size(
            static_cast<int>(src.cols * m_scaleFactor),
            static_cast<int>(src.rows * m_scaleFactor)
        );

        // 计算填充
        m_padTop = (m_targetSize.height - m_newSize.height) / 2;
        m_padLeft = (m_targetSize.width - m_newSize.width) / 2;
        m_padBottom = m_targetSize.height - m_newSize.height - m_padTop;
        m_padRight = m_targetSize.width - m_newSize.width - m_padLeft;

        // 分配最终处理后的图像缓冲区 - 确保连续内存
        m_processed = cv::Mat(m_targetSize, src.type(), cv::Scalar(144, 144, 144) );

        // 验证内存连续性
        // CV_Assert(m_processed.isContinuous() && "m_processed must be continuous memory");

        // 预计算目标ROI区域
        m_targetROI = cv::Rect(m_padLeft, m_padTop, m_newSize.width, m_newSize.height);

        m_preprocessInitialized = true;
    }

    // 获取目标ROI区域引用
    cv::Mat roi = m_processed(m_targetROI);

    // 直接将源图像缩放到ROI区域
    cv::resize(src, roi, m_newSize, 0, 0, cv::INTER_LINEAR);
}

// 标签映射
const std::map<int, std::string> labelMap = {
    {0, "face"},
    {1, "len"},
    {2, "phone"}
};

// 颜色映射 (BGR格式)
const std::map<int, cv::Scalar> colorMap = {
    {0, cv::Scalar(0, 255, 0)},    // 绿色 - face
    {1, cv::Scalar(0, 165, 255)},  // 橙色 - len
    {2, cv::Scalar(255, 0, 0)}     // 红色 - phone
};

void YOLOv3Detector::detect(const cv::Mat& frame, uint32_t& lenCnt, uint32_t& phoneCnt,
        uint32_t& faceCnt, uint32_t& suspectedCnt) {
    if (!m_initialized) {
        throw std::runtime_error("Detector not initialized");
    }

    try {
        // 预处理图像 (包含缩放和填充)
        PreprocessImage(frame);

        // 创建输入张量 (修正4: 直接使用OpenCV数据)
        ov::Tensor input_tensor = ov::Tensor(
            ov::element::u8,
            ov::Shape{ 1, static_cast<size_t>(m_processed.rows),
                     static_cast<size_t>(m_processed.cols), 3 },
            m_processed.data
        );

        // 设置输入并推理
        m_infer_request.set_input_tensor(input_tensor);
        m_infer_request.infer();

        // 获取输出
        ov::Tensor dets_tensor = m_infer_request.get_tensor("dets");
        ov::Tensor labels_tensor = m_infer_request.get_tensor("labels");

        // 解析输出张量
        auto dets_shape = dets_tensor.get_shape();
        size_t num_dets = dets_shape[1];
        size_t det_size = dets_shape[2];

        const float* dets = dets_tensor.data<const float>();
        const int64_t* labels = labels_tensor.data<const int64_t>();

        // 处理后处理结果
        lenCnt = 0, phoneCnt = 0, faceCnt = 0, suspectedCnt = 0;
        std::vector<cv::Rect> phones, lens;
        cv::Rect bbox(0, 0, 0, 0);
        float score = 0.0f;
        int64_t label = 0;

        for (size_t i = 0; i < num_dets; i++) {
            score = dets[i * det_size + 4];

            if (score < m_score_threshold) continue;
            if (i >= m_keep_top_k) break;

            label = labels[i];

            // 边界框坐标 (修正5: 添加填充偏移和缩放处理)
            float x1 = dets[i * det_size + 0];
            float y1 = dets[i * det_size + 1];
            float x2 = dets[i * det_size + 2];
            float y2 = dets[i * det_size + 3];

            // 去除填充偏移
            x1 = (std::max)(0.0f, x1 - m_padLeft);
            y1 = (std::max)(0.0f, y1 - m_padTop);
            x2 = (std::max)(0.0f, x2 - m_padLeft);
            y2 = (std::max)(0.0f, y2 - m_padTop);

            // 缩放回原始图像尺寸
            x1 = x1 / m_scaleFactor;
            y1 = y1 / m_scaleFactor;
            x2 = x2 / m_scaleFactor;
            y2 = y2 / m_scaleFactor;

            // 限制在图像边界内
            x1 = (std::clamp)(x1, 0.0f, static_cast<float>(frame.cols));
            y1 = (std::clamp)(y1, 0.0f, static_cast<float>(frame.rows));
            x2 = (std::clamp)(x2, 0.0f, static_cast<float>(frame.cols));
            y2 = (std::clamp)(y2, 0.0f, static_cast<float>(frame.rows));

            int width = static_cast<int>(x2 - x1);
            int height = static_cast<int>(y2 - y1);
            // 跳过无效框
            if (width <= 0 || height <= 0) continue;

            // update bbox
            bbox.x = static_cast<int>(x1);
            bbox.y = static_cast<int>(y1);
            bbox.width = width;
            bbox.height = height;

            // 业务逻辑计数
            {
                std::shared_lock<std::shared_mutex> readLock(m_paramMtx);
                if (label == m_labelFilterLen) {
                    if (score >= m_scoreFilterLenHigh) {
                        lenCnt++;
                    }
                    else if (score >= m_scoreFilterLenLow) {
                        suspectedCnt++;
                        lens.push_back(bbox);
                    }
                }
                else if (label == m_labelFilterPhone) {
                    if (score >= m_scoreFilterPhoneHigh) {
                        phoneCnt++;
                    }
                    else if (score >= m_scoreFilterPhoneLow) {
                        suspectedCnt++;
                        phones.push_back(bbox);
                    }
                }
                else if (label == m_labelFilterFace) {
                    if (score >= m_scoreFilterFace) {
                        faceCnt++;
                    }
                }
            }

            if (m_imgDebugMode) {
                // 绘制检测结果
                std::string labelText = labelMap.count(label) ? labelMap.at(label) : "Unknown";
                cv::Scalar color = colorMap.count(label) ? colorMap.at(label) : cv::Scalar(0, 255, 255);

                cv::rectangle(frame, bbox, color, 2);
                std::ostringstream label_ss;
                label_ss << labelText << ": " << std::fixed << std::setprecision(2) << score;

                cv::putText(frame, label_ss.str(),
                    cv::Point(bbox.x, bbox.y - 5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
            }

        }
        // 检测摄像头在手机框内的情况
        for (const auto& phone : phones) {
            for (const auto& camera : lens) {
                if ((phone & camera).area() > 0) {
                    phoneCnt++;
                }
            }
        }

        if (m_imgDebugMode) {
            // 显示计数信息
            std::ostringstream info;
            info << "Lens: " << lenCnt << " | Phones: " << phoneCnt
                << " | Faces: " << faceCnt << " | Suspected: " << suspectedCnt;
            cv::putText(frame, info.str(), cv::Point(10, 30),
                cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 255), 2);

            // 显示帧
            cv::imshow("debug", frame);
        }
    }
    catch (const std::exception& e) {
        MY_SPDLOG_ERROR("Detection error: {}", e.what());
    }

}

void YOLOv3Detector::setDetectParam(std::shared_ptr<MyMeta>& meta) {
    if (!m_isCfgListReg) {
        ConfigParser* cfg = ConfigParser::getInstance();
        cfg->registerListener("inferenceSettings", this);
        m_isCfgListReg = true;
    }

    std::unique_lock<std::shared_mutex> writeLock(m_paramMtx);
    // 使用类型安全的默认值获取方法
    m_scoreFilterLenHigh = static_cast<float>(
        meta->getDoubleOrDefault("score_filter_len_high", m_scoreFilterLenHigh)
    );

    m_scoreFilterLenLow = static_cast<float>(
        meta->getDoubleOrDefault("score_filter_len_low", m_scoreFilterLenLow)
    );

    m_scoreFilterPhoneHigh = static_cast<float>(
        meta->getDoubleOrDefault("score_filter_phone_high", m_scoreFilterPhoneHigh)
    );

    m_scoreFilterPhoneLow = static_cast<float>(
        meta->getDoubleOrDefault("score_filter_phone_low", m_scoreFilterPhoneLow)
    );

    m_scoreFilterFace = static_cast<float>(
        meta->getDoubleOrDefault("score_filter_face", m_scoreFilterFace)
    );

    // 标签过滤使用整型默认值获取
    m_labelFilterLen = meta->getInt32OrDefault("label_filter_len", m_labelFilterLen);
    m_labelFilterPhone = meta->getInt32OrDefault("label_filter_phone", m_labelFilterPhone);
    m_labelFilterFace = meta->getInt32OrDefault("label_filter_face", m_labelFilterFace);

    // 日志输出保持不变
    MY_SPDLOG_DEBUG("检测参数更新 - 分数阈值: len_high={:.2f}, len_low={:.2f}, phone_high={:.2f}, phone_low={:.2f}, face={:.2f}",
                   m_scoreFilterLenHigh, m_scoreFilterLenLow,
                   m_scoreFilterPhoneHigh, m_scoreFilterPhoneLow,
                   m_scoreFilterFace);

    MY_SPDLOG_DEBUG("检测参数更新 - 标签过滤: len={}, phone={}, face={}",
                   m_labelFilterLen, m_labelFilterPhone, m_labelFilterFace);
}

void YOLOv3Detector::setImgDebugMode(bool imgDebugMode) {
    m_imgDebugMode = imgDebugMode;
}

void YOLOv3Detector::onConfigUpdated(std::shared_ptr<MyMeta>& newMeta) {
    setDetectParam(newMeta);
}
