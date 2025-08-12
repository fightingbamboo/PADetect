#ifndef MNN_DETECTOR_H
#define MNN_DETECTOR_H

#include <MNN/Interpreter.hpp>
#include <MNN/MNNDefine.h>
#include <MNN/Tensor.hpp>
#include <MNN/ImageProcess.hpp>
#include <opencv2/opencv.hpp>
#include <vector>
#include <memory>
#include <string>
#include <filesystem>


// 检测结果结构体
struct Detection {
    cv::Rect box;       // 边界框
    float conf;         // 置信度
    int class_id;       // 类别ID
};

class MNNDetector {
public:
    // 构造函数
    MNNDetector(const std::string& model_path,
        const std::vector<std::string>& classes = {});

    // 析构函数
    ~MNNDetector();

    // 执行检测（预处理->推理->后处理->坐标转换）
    std::vector<Detection> detect(cv::Mat& frame, bool visualize = false);

private:
    void PreprocessImage(const cv::Mat& src);
    void infer();
    std::vector<Detection> postprocess(const cv::Mat& src);
    void visualize_results(cv::Mat& frame, const std::vector<Detection>& detections);

private:
    // MNN相关组件
    std::shared_ptr<MNN::Interpreter> interpreter;
    std::shared_ptr<MNN::CV::ImageProcess> m_pretreat;
    MNN::Session* session;
    MNN::Tensor* input_tensor;
    MNN::Tensor* output_tensor;


    // 模型参数
    cv::Size model_input_size;

    cv::Mat m_processed;   // 预处理后的预处理图像缓存
    cv::Mat m_resized;     // 预处理后的缩放图像缓存
    cv::Rect m_targetROI;
    float m_scaleFactor;   // 缩放比例
    int m_padTop;          // 上边填充
    int m_padLeft;         // 左边填充
    int m_padBottom;       // 下边填充
    int m_padRight;        // 右边填充
    cv::Size m_newSize;          // 缩放后模型输入尺寸
    bool m_preprocessInitialized = false; // 标记是否已初始化预处理参数
    const float m_mean[3] = { 0.0f, 0.0f, 0.0f }; // RGB
    const float m_std[3] = { 1.0 / 255.0f, 1.0 / 255.0f, 1.0 / 255.0f };
    cv::Size m_targetSize = cv::Size(640, 640);

    // 后处理参数
    std::vector<std::string> class_names;
    float m_score_threshold = 0.5f;
    float m_iouThreshold = 0.45f;
};

#endif // MNN_DETECTOR_H

