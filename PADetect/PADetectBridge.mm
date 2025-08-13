//
//  PADetectBridge.mm
//  PADetect macOS Bridge
//

#import "PADetectBridge.h"
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>
#import <AppKit/AppKit.h>
#include "../PADetectCore.h"

// 不再需要C函数声明，直接使用C++接口

// 错误域定义
NSString * const PADetectErrorDomain = @"com.padetect.error";

// 错误代码
typedef NS_ENUM(NSInteger, PADetectErrorCode) {
    PADetectErrorCodeInitializationFailed = 1000,
    PADetectErrorCodeConfigLoadFailed,
    PADetectErrorCodeDetectionFailed,
    PADetectErrorCodeCameraFailed,
    PADetectErrorCodeInvalidParameter
};

@interface PADetectBridge () {
    ::PADetectCore* _core; // 使用C++类指针
}

@property (nonatomic, strong) dispatch_queue_t callbackQueue;
@property (nonatomic, copy) PADetectionCallback detectionCallback;
@property (nonatomic, copy) PAAlertCallback alertCallback;
@property (nonatomic, copy) PAStatusCallback statusCallback;

@end

@implementation PADetectBridge

#pragma mark - Singleton

+ (instancetype)sharedInstance {
    static PADetectBridge *sharedInstance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedInstance = [[self alloc] init];
    });
    return sharedInstance;
}

#pragma mark - Lifecycle

- (instancetype)init {
    self = [super init];
    if (self) {
        self.callbackQueue = dispatch_queue_create("com.padetect.callback", DISPATCH_QUEUE_SERIAL);
        
        // 获取C++核心业务逻辑类实例
        _core = ::PADetectCore::getInstance();
    }
    return self;
}

- (void)dealloc {
    [self stopDetection];
    
    // 清除所有回调以避免循环引用
    if (_core) {
        _core->setDetectionCallback(nullptr);
        _core->setAlertCallback(nullptr);
        _core->setStatusCallback(nullptr);
    }
    
    _detectionCallback = nil;
    _alertCallback = nil;
    _statusCallback = nil;
}

#pragma mark - Initialization

- (BOOL)initializeWithModelPath:(NSString *)modelPath
                     configPath:(NSString *)configPath
                   pipelinePath:(NSString *)pipelinePath
                         device:(NSString *)device
                          error:(NSError **)error {
    
    if (!_core) {
        if (error) {
            *error = [NSError errorWithDomain:PADetectErrorDomain
                                         code:PADetectErrorCodeInitializationFailed
                                     userInfo:@{NSLocalizedDescriptionKey: @"Core instance not available"}];
        }
        return NO;
    }
    
    // 设置模型路径
    if (modelPath) {
        std::string modelPathStr = [modelPath UTF8String];
        bool pathSet = _core->setModelPath(modelPathStr);
        if (!pathSet) {
            if (error) {
                *error = [NSError errorWithDomain:PADetectErrorDomain
                                             code:PADetectErrorCodeInitializationFailed
                                         userInfo:@{NSLocalizedDescriptionKey: @"Failed to set model path"}];
            }
            return NO;
        }
    }
    
    // 调用C++方法进行初始化
    bool success = _core->initialize();
    if (!success) {
        if (error) {
            *error = [NSError errorWithDomain:PADetectErrorDomain
                                         code:PADetectErrorCodeInitializationFailed
                                     userInfo:@{NSLocalizedDescriptionKey: @"Core initialization failed"}];
        }
        return NO;
    }
    
    return YES;
}

#pragma mark - Configuration

- (BOOL)loadConfigFromPath:(NSString *)configPath error:(NSError **)error {
    if (!_core) {
        if (error) {
            *error = [NSError errorWithDomain:PADetectErrorDomain
                                         code:PADetectErrorCodeConfigLoadFailed
                                     userInfo:@{NSLocalizedDescriptionKey: @"Core instance not available"}];
        }
        return NO;
    }
    
    // 使用C++接口，这里需要扩展C++接口来支持配置加载
    // 暂时返回YES，实际实现需要添加相应的C++方法
    return YES;
}

- (BOOL)loadServerConfigFromPath:(NSString *)serverConfigPath error:(NSError **)error {
    if (!_core) {
        if (error) {
            *error = [NSError errorWithDomain:PADetectErrorDomain
                                         code:PADetectErrorCodeConfigLoadFailed
                                     userInfo:@{NSLocalizedDescriptionKey: @"Core instance not available"}];
        }
        return NO;
    }
    
    // 使用C++接口，这里需要扩展C++接口来支持服务器配置加载
    // 暂时返回YES，实际实现需要添加相应的C++方法
    return YES;
}

#pragma mark - Detection Control

- (BOOL)startDetectionWithError:(NSError **)error {
    if (!_core) {
        if (error) {
            *error = [NSError errorWithDomain:PADetectErrorDomain
                                         code:PADetectErrorCodeInitializationFailed
                                     userInfo:@{NSLocalizedDescriptionKey: @"Core instance not available"}];
        }
        return NO;
    }
    
    _core->startDetection();
    return YES;
}

- (void)stopDetection {
    if (_core) {
        _core->stopDetection();
    }
}

- (BOOL)isDetectionRunning {
    if (_core) {
        return _core->isDetectionRunning();
    }
    return NO;
}

#pragma mark - Camera Settings

- (NSArray<NSString *> *)getAvailableCameras {
    NSMutableArray<NSString *> *cameraNames = [NSMutableArray array];
    
    @autoreleasepool {
        NSArray<AVCaptureDevice *> *devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
        
        for (AVCaptureDevice *device in devices) {
            [cameraNames addObject:device.localizedName];
        }
        
        // 如果没有找到摄像头，添加默认项
        if (cameraNames.count == 0) {
            [cameraNames addObject:@"No Camera Found"];
        }
    }
    
    return [cameraNames copy];
}

- (BOOL)setCameraId:(int32_t)cameraId
              width:(int32_t)width
             height:(int32_t)height
              error:(NSError **)error {
    
    if (!_core) {
        if (error) {
            *error = [NSError errorWithDomain:PADetectErrorDomain
                                         code:PADetectErrorCodeInvalidParameter
                                     userInfo:@{NSLocalizedDescriptionKey: @"Core instance not available"}];
        }
        return NO;
    }
    
    bool success = _core->setCameraSettings(cameraId, width, height);
    
    if (!success && error) {
        *error = [NSError errorWithDomain:PADetectErrorDomain
                                     code:PADetectErrorCodeInvalidParameter
                                 userInfo:@{NSLocalizedDescriptionKey: @"Cannot change camera settings while detection is running"}];
    }
    
    return success;
}

#pragma mark - Test Mode

- (void)setTestMode:(BOOL)enabled videoPath:(NSString *)videoPath {
    if (_core) {
        _core->setTestMode(enabled, videoPath ? [videoPath UTF8String] : "");
    }
}

- (void)setSourcePreview:(BOOL)enabled {
    // 使用C++接口，这里需要扩展C++接口来支持源预览设置
    // 暂时不实现，实际实现需要添加相应的C++方法
}

#pragma mark - Alert Management

- (void)setAlertEnabled:(BOOL)enabled forType:(PAAlertType)alertType {
    if (_core) {
        _core->setAlertEnabled(enabled, static_cast<::AlertType>(alertType));
    }
}

- (void)showAlertForType:(PAAlertType)alertType {
    if (_core) {
        _core->showAlert(static_cast<::AlertType>(alertType));
    }
    
    // 通过通知机制通知Swift层显示全屏弹窗
    dispatch_async(dispatch_get_main_queue(), ^{
        NSString *version = [self getVersion];
        NSDictionary *userInfo = @{
            @"alertType": @(alertType),
            @"version": version
        };
        
        [[NSNotificationCenter defaultCenter] postNotificationName:@"ShowFullScreenAlert"
                                                            object:nil
                                                          userInfo:userInfo];
    });
}

- (void)hideAlert {
    if (_core) {
        _core->hideAlert();
    }
    
    // 通过通知机制通知Swift层隐藏全屏弹窗
    dispatch_async(dispatch_get_main_queue(), ^{
        [[NSNotificationCenter defaultCenter] postNotificationName:@"HideFullScreenAlert"
                                                            object:nil
                                                          userInfo:nil];
    });
}

- (BOOL)isAlertShowing {
    if (_core) {
        return _core->isAlertShowing();
    }
    return NO;
}

- (void)showMessageBoxWithTitle:(NSString *)title
                        message:(NSString *)message
                          style:(NSUInteger)style {
    dispatch_async(dispatch_get_main_queue(), ^{
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = title;
        alert.informativeText = message;
        
        // 根据Windows MessageBox样式设置NSAlert样式
        if (style & 0x10) { // MB_ICONERROR
            alert.alertStyle = NSAlertStyleCritical;
        } else if (style & 0x30) { // MB_ICONWARNING
            alert.alertStyle = NSAlertStyleWarning;
        } else {
            alert.alertStyle = NSAlertStyleInformational;
        }
        
        [alert addButtonWithTitle:@"确定"];
        [alert runModal];
    });
}

#pragma mark - Callbacks

- (void)setDetectionCallback:(PADetectionCallback)callback {
    _detectionCallback = callback;
    
    if (_core && callback) {
        // 将Objective-C block转换为C++ std::function
        auto cppCallback = [self](const DetectionResult& result) {
            // 转换C++结构体到Objective-C结构体
            PADetectionResult objcResult;
            objcResult.lenCount = result.lenCount;
            objcResult.phoneCount = result.phoneCount;
            objcResult.faceCount = result.faceCount;
            objcResult.suspectedCount = result.suspectedCount;
            
            // 在主线程调用Objective-C回调
            dispatch_async(dispatch_get_main_queue(), ^{
                if (self->_detectionCallback) {
                    self->_detectionCallback(objcResult);
                }
            });
        };
        
        // 设置C++回调
        _core->setDetectionCallback(cppCallback);
    } else if (_core) {
        // 清除回调
        _core->setDetectionCallback(nullptr);
    }
}

- (void)setAlertCallback:(PAAlertCallback)callback {
    _alertCallback = callback;
    
    if (_core && callback) {
        // 将Objective-C block转换为C++ std::function
        auto cppCallback = [self](::AlertType alertType) {
            // 转换C++枚举到Objective-C枚举
            PAAlertType objcAlertType = static_cast<PAAlertType>(alertType);
            
            // 在主线程调用Objective-C回调
            dispatch_async(dispatch_get_main_queue(), ^{
                if (self->_alertCallback) {
                    self->_alertCallback(objcAlertType);
                }
            });
        };
        
        // 设置C++回调
        _core->setAlertCallback(cppCallback);
    } else if (_core) {
        // 清除回调
        _core->setAlertCallback(nullptr);
    }
}

- (void)setStatusCallback:(PAStatusCallback)callback {
    _statusCallback = callback;
    
    if (_core && callback) {
        // 将Objective-C block转换为C++ std::function
        auto cppCallback = [self](::DetectionStatus status, const std::string& errorMessage) {
            // 转换C++枚举到Objective-C枚举
            PADetectionStatus objcStatus = static_cast<PADetectionStatus>(status);
            NSString* objcErrorMessage = errorMessage.empty() ? nil : [NSString stringWithUTF8String:errorMessage.c_str()];
            
            // 在主线程调用Objective-C回调
            dispatch_async(dispatch_get_main_queue(), ^{
                if (self->_statusCallback) {
                    self->_statusCallback(objcStatus, objcErrorMessage);
                }
            });
        };
        
        // 设置C++回调
        _core->setStatusCallback(cppCallback);
    } else if (_core) {
        // 清除回调
        _core->setStatusCallback(nullptr);
    }
}

#pragma mark - Manual Detection

- (PADetectionResult)detectInImage:(CVPixelBufferRef)pixelBuffer error:(NSError **)error {
    PADetectionResult result = {0, 0, 0, 0};
    
    // TODO: 实现CVPixelBuffer到OpenCV Mat的转换
    // 需要配置OpenCV库后实现
    if (error) {
        *error = [NSError errorWithDomain:PADetectErrorDomain
                                     code:PADetectErrorCodeDetectionFailed
                                 userInfo:@{NSLocalizedDescriptionKey: @"Manual detection not implemented yet"}];
    }
    
    return result;
}

- (PADetectionResult)detectInCGImage:(CGImageRef)image error:(NSError **)error {
    PADetectionResult result = {0, 0, 0, 0};
    
    if (!image) {
        if (error) {
            *error = [NSError errorWithDomain:PADetectErrorDomain
                                         code:PADetectErrorCodeInvalidParameter
                                     userInfo:@{NSLocalizedDescriptionKey: @"Invalid CGImage"}];
        }
        return result;
    }
    
    // TODO: 实现CGImage到OpenCV Mat的转换
    if (error) {
        *error = [NSError errorWithDomain:PADetectErrorDomain
                                     code:PADetectErrorCodeDetectionFailed
                                 userInfo:@{NSLocalizedDescriptionKey: @"Manual detection not implemented yet"}];
    }
    
    return result;
}

#pragma mark - Parameter Configuration

- (void)setDetectionThreshold:(float)threshold forType:(PAAlertType)alertType {
    // 使用C++接口，这里需要扩展C++接口来支持检测阈值设置
    // 暂时不实现，实际实现需要添加相应的C++方法
}

- (void)setAlertInterval:(int32_t)intervalMs {
    // 使用C++接口，这里需要扩展C++接口来支持告警间隔设置
    // 暂时不实现，实际实现需要添加相应的C++方法
}

- (void)setCaptureInterval:(int32_t)intervalMs {
    // 使用C++接口，这里需要扩展C++接口来支持捕获间隔设置
    // 暂时不实现，实际实现需要添加相应的C++方法
}

#pragma mark - Version and Logging

- (NSString *)getVersion {
    if (_core) {
        std::string version = _core->getVersion();
        return [NSString stringWithUTF8String:version.c_str()];
    }
    return @"1.0.7";
}

- (void)setLogLevel:(NSInteger)level {
    if (_core) {
        _core->setLogLevel((int)level);
    }
}

#pragma mark - Screen Lock Methods

- (void)setNoFaceLockEnabled:(BOOL)enabled {
    if (_core) {
        _core->setNoFaceLockEnabled(enabled);
    }
}

- (void)setNoFaceLockTimeout:(int32_t)timeoutMs {
    if (_core) {
        _core->setNoFaceLockTimeout(timeoutMs);
    }
}

- (void)triggerScreenLock {
    // 在macOS上触发锁屏
    dispatch_async(dispatch_get_main_queue(), ^{
        // 使用AppleScript触发锁屏
        NSAppleScript *script = [[NSAppleScript alloc] initWithSource:@"tell application \"System Events\" to keystroke \"q\" using {control down, command down}"];
        [script executeAndReturnError:nil];
    });
}

@end

// C函数桥接，供C++代码调用
extern "C" {
    ::PADetectCore* getPADetectCoreInstance() {
        // 返回C++单例实例
        return ::PADetectCore::getInstance();
    }
}