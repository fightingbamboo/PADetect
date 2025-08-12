
#include <opencv2/opencv.hpp>
#include <chrono>
#include <fstream>

#include "MNNDetector.h"

void redirectStreams() {
    // 重定向标准输出到output.txt（追加模式）
    if (!freopen("output.txt", "w", stdout)) {
        perror("Failed to redirect stdout");
    }

    // 重定向标准错误到error.txt（追加模式）
    if (!freopen("error.txt", "w", stderr)) {
        perror("Failed to redirect stderr");
        // 注意：此时stderr尚未重定向，错误仍会输出到原终端
        fprintf(stdout, "Stderr redirect failed\n"); // 会写入output.txt
    }
}

int main() {
    redirectStreams();
    // 配置参数
    const std::string model_path = "best_640.mnn";
    const cv::Size camera_res(640, 480);  // 预期摄像头分辨率
#if 0
    const std::vector<std::string> classes = {
        "person", "bicycle", "car", "motorcycle", "airplane",
        "bus", "train", "truck", "boat", "traffic light",
        "fire hydrant", "stop sign", "parking meter", "bench", "bird",
        "cat", "dog", "horse", "sheep", "cow",
        "elephant", "bear", "zebra", "giraffe", "backpack",
        "umbrella", "handbag", "tie", "suitcase", "frisbee",
        "skis", "snowboard", "sports ball", "kite", "baseball bat",
        "baseball glove", "skateboard", "surfboard", "tennis racket", "bottle",
        "wine glass", "cup", "fork", "knife", "spoon",
        "bowl", "banana", "apple", "sandwich", "orange",
        "broccoli", "carrot", "hot dog", "pizza", "donut",
        "cake", "chair", "couch", "potted plant", "bed",
        "dining table", "toilet", "tv", "laptop", "mouse",
        "remote", "keyboard", "cell phone", "microwave", "oven",
        "toaster", "sink", "refrigerator", "book", "clock",
        "vase", "scissors", "teddy bear", "hair drier", "toothbrush"
    };
#else
    const std::vector<std::string> classes = {
        "face", "lens", "phone"
    };
#endif
    // 初始化检测器
    try
    {
        MNNDetector detector(model_path, classes);


    // 打开摄像头
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "Error opening camera" << std::endl;
        return -1;
    }
    cap.set(cv::CAP_PROP_FRAME_WIDTH, camera_res.width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, camera_res.height);

    // 性能统计
    int frame_count = 0;
    float total_fps = 0;
    const int warmup_frames = 10;

    cv::Mat frame;
    while (true) {
        cap >> frame;
        if (frame.empty()) break;

        auto start = std::chrono::high_resolution_clock::now();

        // 执行检测（包含可视化）
        auto detections = detector.detect(frame, true);

        auto end = std::chrono::high_resolution_clock::now();
        float fps = 1000.0 / std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        // 跳过预热帧
        if (frame_count++ > warmup_frames) {
            total_fps += fps;
        }

        // 显示FPS
        cv::putText(frame, cv::format("FPS: %.2f", fps),
            cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX,
            0.7, cv::Scalar(0, 255, 0), 2);

        cv::imshow("MNN Detection", frame);
        if (cv::waitKey(300) == 27) break;  // ESC退出
    }

    }
    catch (const std::exception& e)
    {
        std::cerr << "catch failed: " << e.what() << std::endl;
        std::getchar();
        return 1;
    }

    std::getchar();
    return 0;
}

