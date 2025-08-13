

#include <iostream>
#include "MNNDetector.h"

MNNDetector::MNNDetector(const std::string& model_path,
    const std::vector<std::string>& classes)
    : class_names(classes) {

    printf("[MNNDetector DEBUG] Starting MNNDetector constructor with model: %s\n", model_path.c_str());
    
    // 1. 加载模型
    printf("[MNNDetector DEBUG] Loading MNN model...\n");
    interpreter = std::shared_ptr<MNN::Interpreter>(
        MNN::Interpreter::createFromFile(model_path.c_str()),
        MNN::Interpreter::destroy
        );
    
    if (!interpreter) {
        printf("[MNNDetector ERROR] Failed to load MNN model from: %s\n", model_path.c_str());
        throw std::runtime_error("Failed to load MNN model");
    }
    printf("[MNNDetector DEBUG] MNN model loaded successfully\n");

    // 创建缓存目录
    printf("[MNNDetector DEBUG] Creating cache directory...\n");
    
    // 使用用户主目录下的缓存目录，避免只读文件系统问题
    std::string homeDir = std::getenv("HOME") ? std::getenv("HOME") : "/tmp";
    const std::string cacheDir = homeDir + "/.padetect_cache/gpu_cache/";
    
    try {
        if (!std::filesystem::exists(cacheDir)) {
            std::filesystem::create_directories(cacheDir);
        }
        auto cachePath = std::filesystem::path(cacheDir) / "cachefile";
        std::string cacheStr = cachePath.string();
        interpreter->setCacheFile(cacheStr.c_str());
        printf("[MNNDetector DEBUG] Cache directory created and set at: %s\n", cacheDir.c_str());
    } catch (const std::filesystem::filesystem_error& e) {
        printf("[MNNDetector WARNING] Failed to create cache directory: %s, continuing without cache\n", e.what());
        // 继续执行，不使用缓存
    }

    // 2. 配置会话，使用OpenCL加速
    printf("[MNNDetector DEBUG] Configuring OpenCL session...\n");
    MNN::ScheduleConfig config;
    config.type = MNN_FORWARD_OPENCL;
    MNN::BackendConfig backend_config;
    backend_config.precision = MNN::BackendConfig::Precision_High;
    config.backendConfig = &backend_config;

    // 3. 创建会话
    printf("[MNNDetector DEBUG] Creating MNN session...\n");
    session = interpreter->createSession(config);
    
    if (!session) {
        printf("[MNNDetector ERROR] Failed to create MNN session\n");
        throw std::runtime_error("Failed to create MNN session");
    }
    printf("[MNNDetector DEBUG] MNN session created successfully\n");

    // 4. 获取输入输出张量
    input_tensor = interpreter->getSessionInput(session, "images");
    output_tensor = interpreter->getSessionOutput(session, "output0");

    // 5. 验证模型输入尺寸
    std::vector<int> input_shape = input_tensor->shape();
    if (input_shape.size() != 4 || input_shape[0] != 1 || input_shape[1] != 3) {
        throw std::runtime_error("Invalid input dimensions");
    }
    m_targetSize = cv::Size(input_shape[3], input_shape[2]); // 宽x高

    // 6. 初始化预处理
    m_pretreat = std::shared_ptr<MNN::CV::ImageProcess>(
        MNN::CV::ImageProcess::create(
            MNN::CV::BGR,
            MNN::CV::RGB,
            m_mean, 3,
            m_std, 3
        ),
        MNN::CV::ImageProcess::destroy
        );

    std::cout << "Detector initialized - Input: "
        << m_targetSize.width << "x" << m_targetSize.height << "\n";
}

MNNDetector::~MNNDetector() {
    
    interpreter->updateCacheFile(session);
    std::cout << "update cache file" << std::endl;
}

void MNNDetector::PreprocessImage(const cv::Mat& src) {
    // 仅在初始化时计算一次缩放参数和内存分配
    if (!m_preprocessInitialized) {
        // 计算缩放比例
        m_scaleFactor = std::min(
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

        // 预分配处理后的图像缓存区 - 确保连续内存
        m_processed = cv::Mat(m_targetSize.height, m_targetSize.width, src.type(), cv::Scalar(144, 144, 144));

        // 验证内存连续性
        CV_Assert(m_processed.isContinuous() && "m_processed must be continuous memory");

        // 预计算目标ROI区域
        m_targetROI = cv::Rect(m_padLeft, m_padTop, m_newSize.width, m_newSize.height);

        m_preprocessInitialized = true;
    }

    // 获取目标ROI区域引用
    cv::Mat roi = m_processed(m_targetROI);

    // 直接将源图像缩放到ROI区域
    cv::resize(src, roi, m_newSize, 0, 0, cv::INTER_LINEAR);

    m_pretreat->convert(m_processed.data, m_targetSize.width, m_targetSize.height, 0, input_tensor);
}

void MNNDetector::infer() {
    interpreter->runSession(session);
}

std::vector<Detection> MNNDetector::postprocess(const cv::Mat& src) {
    // 1. 获取输出数据
    MNN::Tensor output_host(output_tensor, output_tensor->getDimensionType());
    output_tensor->copyToHostTensor(&output_host);
    float* output_data = output_host.host<float>();

    // 2. 解析输出形状 [1, num_boxes, 85]
    auto output_shape = output_tensor->shape();
    const int num_boxes = output_shape[1];
    const int num_classes = output_shape[2] - 5; // 85 - 5 = 80

    // 3. 存储检测结果
    std::vector<Detection> detections;

    for (int i = 0; i < num_boxes; i++) {
        float* box_data = output_data + i * (num_classes + 5);
        float conf = box_data[4];
        if (conf < m_score_threshold) continue;

        // 获取类别
        float* class_probs = box_data + 5;
        int class_id = std::max_element(class_probs, class_probs + num_classes) - class_probs;
        float class_conf = class_probs[class_id];
        float confidence = conf * class_conf;

        if (confidence < m_score_threshold) { continue; }

        // 解析边界框 (中心点+宽高格式)
        float cx = box_data[0];
        float cy = box_data[1];
        float w = box_data[2];
        float h = box_data[3];
#if 0
        float x1 = ((cx - w / 2.0f) - (float)(m_padRight - m_padLeft)) / m_scaleFactor;
        float y1 = ((cy - h / 2.0f) - (float)(m_padBottom - m_padTop)) / m_scaleFactor;
        float x2 = ((cx + w / 2.0f) - (float)(m_padRight - m_padLeft)) / m_scaleFactor;
        float y2 = ((cy + h / 2.0f) - (float)(m_padBottom - m_padTop)) / m_scaleFactor;
#else
        float x1 = (cx - w / 2.0f - m_padLeft) / m_scaleFactor;
        float y1 = (cy - h / 2.0f - m_padTop) / m_scaleFactor;
        float x2 = (cx + w / 2.0f - m_padLeft) / m_scaleFactor;
        float y2 = (cy + h / 2.0f - m_padTop) / m_scaleFactor;
#endif
        x1 = (std::max)(0.0f, x1);
        y1 = (std::max)(0.0f, y1);
        x2 = (std::min)(x2, (float)(src.cols) - 1.f);
        y2 = (std::min)(y2, (float)(src.rows) - 1.f);

        detections.push_back({
            cv::Rect(x1, y1, x2 - x1, y2 - y1),
            confidence,
            class_id
            }
        );
    }

    // 4. NMS处理
    std::vector<Detection> results;
    std::vector<cv::Rect> boxes;
    std::vector<float> scores;
    std::vector<int> indices;

    for (const auto& det : detections) {
        // 准备数据用于NMS
#if 0
        boxes.push_back(cv::Rect(
            static_cast<int>(det.box.x - det.box.width / 2),
            static_cast<int>(det.box.y - det.box.height / 2),
            static_cast<int>(det.box.width),
            static_cast<int>(det.box.height)
        ));
#else
        boxes.emplace_back(det.box);
#endif
        scores.emplace_back(det.conf);
    }

    cv::dnn::NMSBoxes(boxes, scores, m_score_threshold, m_iouThreshold, indices);

    for (int idx : indices) {
        results.emplace_back(detections[idx]);
    }

    return results;
}

void MNNDetector::visualize_results(cv::Mat& frame, const std::vector<Detection>& detections) {
    const std::vector<cv::Scalar> colors = {
        {0, 0, 255}, {0, 255, 0}, {255, 0, 0},
        {0, 255, 255}, {255, 0, 255}
    };

    for (const auto& det : detections) {
        if (det.box.area() <= 0) continue;  // 脤酶鹿媒脦脼脨搂戮脴脨脦

        cv::Scalar color = colors[det.class_id % colors.size()];
        cv::rectangle(frame, det.box, color, 2);

        // 麓麓陆篓卤锚脟漏
        std::string label = class_names.empty() ?
            "Class " + std::to_string(det.class_id) :
            class_names[det.class_id];
        label += cv::format(" %.2f", det.conf);

        // 录脝脣茫脦脛卤戮脦禄脰脙
        int baseline = 0;
        cv::Size text_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.6, 1, &baseline);
        cv::Point text_origin(det.box.x, det.box.y - 5);

        // 脠路卤拢脦脛卤戮脦禄脰脙脭脷脥录脧帽脛脷
        if (text_origin.y < 10) text_origin.y = det.box.y + 20;

        // 禄忙脰脝脦脛卤戮卤鲁戮掳
        cv::rectangle(frame,
            cv::Rect(text_origin.x, text_origin.y - text_size.height,
                text_size.width, text_size.height + 5),
            color, cv::FILLED);

        // 禄忙脰脝脦脛卤戮
        cv::putText(frame, label,
            cv::Point(text_origin.x, text_origin.y),
            cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 1);
    }
}

std::vector<Detection> MNNDetector::detect(cv::Mat& frame, bool visualize) {

    // 麓娄脌铆脕梅鲁脤
    PreprocessImage(frame);
    infer();
    auto detections = postprocess(frame);

    // 驴脡脩隆碌脛驴脡脢脫禄炉
    if (visualize) {
        visualize_results(frame, detections);
    }

    return detections;
}

