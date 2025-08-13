//
//  PADetectBridge.h
//  PADetect macOS Bridge
//

#ifndef PADetectBridge_h
#define PADetectBridge_h

#import <Foundation/Foundation.h>
#import <CoreVideo/CoreVideo.h>
#import <AVFoundation/AVFoundation.h>

// 检测结果结构体
typedef struct {
    uint32_t lenCount;
    uint32_t phoneCount;
    uint32_t faceCount;
    uint32_t suspectedCount;
} PADetectionResult;

// 告警类型枚举
typedef NS_ENUM(NSInteger, PAAlertType) {
    PAAlertTypePhone = 0,
    PAAlertTypePeep,
    PAAlertTypeNobody,
    PAAlertTypeOcclude,
    PAAlertTypeNoConnect,
    PAAlertTypeSuspect
};

// 检测状态枚举
typedef NS_ENUM(NSInteger, PADetectionStatus) {
    PADetectionStatusStopped = 0,
    PADetectionStatusRunning,
    PADetectionStatusError
};

// 回调block定义
typedef void(^PADetectionCallback)(PADetectionResult result);
typedef void(^PAAlertCallback)(PAAlertType alertType);
typedef void(^PAStatusCallback)(PADetectionStatus status, NSString * _Nullable errorMessage);

// 主要的桥接类
@interface PADetectBridge : NSObject

// 单例模式
+ (instancetype)sharedInstance;

// 初始化和配置
- (BOOL)initializeWithModelPath:(NSString *)modelPath
                     configPath:(NSString *)configPath
                   pipelinePath:(NSString *)pipelinePath
                         device:(NSString *)device
                          error:(NSError **)error;

// 配置管理
- (BOOL)loadConfigFromPath:(NSString *)configPath error:(NSError **)error;
- (BOOL)loadServerConfigFromPath:(NSString *)serverConfigPath error:(NSError **)error;

// 检测控制
- (BOOL)startDetectionWithError:(NSError **)error;
- (void)stopDetection;
- (BOOL)isDetectionRunning;

// 摄像头设置
- (NSArray<NSString *> *)getAvailableCameras;
- (BOOL)setCameraId:(int32_t)cameraId
              width:(int32_t)width
             height:(int32_t)height
              error:(NSError **)error;

// 测试模式设置
- (void)setTestMode:(BOOL)enabled videoPath:(NSString * _Nullable)videoPath;
- (void)setSourcePreview:(BOOL)enabled;

// 告警设置
- (void)setAlertEnabled:(BOOL)enabled forType:(PAAlertType)alertType;
- (BOOL)getAlertEnabledForType:(PAAlertType)alertType;
- (void)showAlertForType:(PAAlertType)alertType;
- (void)hideAlert;
- (BOOL)isAlertShowing;

// 回调设置
- (void)setDetectionCallback:(PADetectionCallback _Nullable)callback;
- (void)setAlertCallback:(PAAlertCallback _Nullable)callback;
- (void)setStatusCallback:(PAStatusCallback _Nullable)callback;

// 手动检测单帧图像
- (PADetectionResult)detectInImage:(CVPixelBufferRef)pixelBuffer error:(NSError **)error;
- (PADetectionResult)detectInCGImage:(CGImageRef)image error:(NSError **)error;

// 参数配置
- (void)setDetectionThreshold:(float)threshold forType:(PAAlertType)alertType;
- (void)setAlertInterval:(int32_t)intervalMs;
- (void)setCaptureInterval:(int32_t)intervalMs;

// 版本信息
- (NSString *)getVersion;

// 日志级别设置
- (void)setLogLevel:(NSInteger)level;

// 锁屏相关方法
- (void)setNoFaceLockEnabled:(BOOL)enabled;
- (void)setNoFaceLockTimeout:(int32_t)timeoutMs;
- (void)triggerScreenLock;

@end

#endif /* PADetectBridge_h */