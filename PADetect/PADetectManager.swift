//
//  PADetectManager.swift
//  PADetect macOS
//

import Foundation
import AVFoundation
import Combine
import AppKit

/// 摄像头权限状态
public enum CameraPermissionStatus {
    case notDetermined
    case denied
    case authorized
    case restricted
}

/// PADetect管理器 - Swift封装类
@MainActor
public class PADetectManager: ObservableObject {
    
    // MARK: - Published Properties
    
    @Published public var isDetectionRunning: Bool = false
    @Published public var detectionStatus: PADetectionStatus = PADetectionStatus(rawValue: 0)!
    @Published public var lastDetectionResult: PADetectionResult = PADetectionResult(lenCount: 0, phoneCount: 0, faceCount: 0, suspectedCount: 0)
    @Published public var currentAlert: PAAlertType? = nil
    @Published public var errorMessage: String? = nil
    @Published public var cameraPermissionStatus: CameraPermissionStatus = .notDetermined
    
    // MARK: - Private Properties
    
    private let bridge: PADetectBridge
    private var cancellables = Set<AnyCancellable>()
    private let alertWindowManager = AlertWindowManager.shared
    
    // MARK: - Configuration
    
    public struct Configuration {
        public let modelPath: String
        public let configPath: String
        public let pipelinePath: String
        public let device: String
        
        public init(modelPath: String, configPath: String, pipelinePath: String, device: String = "CPU") {
            self.modelPath = modelPath
            self.configPath = configPath
            self.pipelinePath = pipelinePath
            self.device = device
        }
    }
    
    public struct CameraSettings {
        public var cameraId: Int32
        public var width: Int32
        public var height: Int32
        
        public init(cameraId: Int32 = 0, width: Int32 = 640, height: Int32 = 480) {
            self.cameraId = cameraId
            self.width = width
            self.height = height
        }
    }
    
    // MARK: - Initialization
    
    public init() {
        self.bridge = PADetectBridge.sharedInstance()
        setupCallbacks()
        updateCameraPermissionStatus()
    }
    
    // MARK: - Camera Permission Methods
    
    /// 检查当前摄像头权限状态
    public func checkCameraPermission() -> CameraPermissionStatus {
        let status = AVCaptureDevice.authorizationStatus(for: .video)
        let permissionStatus: CameraPermissionStatus
        
        switch status {
        case .notDetermined:
            permissionStatus = .notDetermined
        case .denied:
            permissionStatus = .denied
        case .authorized:
            permissionStatus = .authorized
        case .restricted:
            permissionStatus = .restricted
        @unknown default:
            permissionStatus = .denied
        }
        
        DispatchQueue.main.async {
            self.cameraPermissionStatus = permissionStatus
        }
        
        return permissionStatus
    }
    
    /// 请求摄像头权限
    public func requestCameraPermission() async -> Bool {
        let currentStatus = checkCameraPermission()
        
        // 如果已经授权，直接返回true
        if currentStatus == .authorized {
            return true
        }
        
        // 如果权限被拒绝或受限，返回false
        if currentStatus == .denied || currentStatus == .restricted {
            await MainActor.run {
                self.errorMessage = "摄像头权限被拒绝。请在系统设置 > 隐私与安全性 > 摄像头中启用PADetect的摄像头权限。"
            }
            return false
        }
        
        // 请求权限
        return await withCheckedContinuation { continuation in
            AVCaptureDevice.requestAccess(for: .video) { granted in
                DispatchQueue.main.async {
                    if granted {
                        self.cameraPermissionStatus = .authorized
                        SwiftLogger.shared.info("Camera permission granted")
                    } else {
                        self.cameraPermissionStatus = .denied
                        self.errorMessage = "摄像头权限被拒绝。请在系统设置 > 隐私与安全性 > 摄像头中启用PADetect的摄像头权限。"
                        SwiftLogger.shared.warning("Camera permission denied")
                    }
                }
                continuation.resume(returning: granted)
            }
        }
    }
    
    /// 打开系统设置页面
    public func openSystemPreferences() {
        if let url = URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_Camera") {
            NSWorkspace.shared.open(url)
        }
    }
    
    /// 更新摄像头权限状态
    private func updateCameraPermissionStatus() {
        _ = checkCameraPermission()
    }
    
    // MARK: - Public Methods
    
    /// 初始化检测器
    public func initialize(with configuration: Configuration) async throws {
        return try await withCheckedThrowingContinuation { continuation in
            do {
                SwiftLogger.shared.debug("About to call bridge.initialize")
        SwiftLogger.shared.debug("Model path: \(configuration.modelPath)")
        SwiftLogger.shared.debug("Config path: \(configuration.configPath)")
        SwiftLogger.shared.debug("Pipeline path: \(configuration.pipelinePath)")
        SwiftLogger.shared.debug("Device: \(configuration.device)")
                
                try bridge.initialize(
                    withModelPath: configuration.modelPath,
                    configPath: configuration.configPath,
                    pipelinePath: configuration.pipelinePath,
                    device: configuration.device
                )
                SwiftLogger.shared.debug("bridge.initialize completed successfully")
                continuation.resume()
            } catch {
                SwiftLogger.shared.error("bridge.initialize failed: \(error.localizedDescription)")
                continuation.resume(throwing: error)
            }
        }
    }
    
    /// 加载配置文件
    public func loadConfig(from path: String) async throws {
        return try await withCheckedThrowingContinuation { continuation in
            do {
                try bridge.loadConfig(fromPath: path)
                continuation.resume()
            } catch {
                continuation.resume(throwing: error)
            }
        }
    }
    
    /// 加载服务器配置文件
    public func loadServerConfig(from path: String) async throws {
        return try await withCheckedThrowingContinuation { continuation in
            do {
                try bridge.loadServerConfig(fromPath: path)
                continuation.resume()
            } catch {
                continuation.resume(throwing: error)
            }
        }
    }
    
    /// 获取可用摄像头列表
    public func getAvailableCameras() -> [String] {
        return bridge.getAvailableCameras()
    }
    
    /// 设置摄像头参数
    public func setCameraSettings(_ settings: CameraSettings) throws {
        try bridge.setCameraId(
            settings.cameraId,
            width: settings.width,
            height: settings.height
        )
    }
    
    /// 重置检测结果
    private func resetDetectionResults() {
        lastDetectionResult = PADetectionResult(lenCount: 0, phoneCount: 0, faceCount: 0, suspectedCount: 0)
    }
    
    /// 开始检测
    public func startDetection() async throws {
        // 开始检测前重置累计数据
        resetDetectionResults()
        
        return try await withCheckedThrowingContinuation { continuation in
            do {
                try bridge.startDetection()
                continuation.resume()
            } catch {
                continuation.resume(throwing: error)
            }
        }
    }
    
    /// 停止检测
    public func stopDetection() {
        bridge.stopDetection()
    }
    
    /// 设置测试模式
    public func setTestMode(enabled: Bool, videoPath: String? = nil) {
        bridge.setTestMode(enabled, videoPath: videoPath)
    }
    
    /// 设置源预览
    public func setSourcePreview(enabled: Bool) {
        bridge.setSourcePreview(enabled)
    }
    
    /// 设置告警开关
    public func setAlertEnabled(_ enabled: Bool, for alertType: PAAlertType) {
        bridge.setAlertEnabled(enabled, for: alertType)
    }
    
    /// 获取告警开关状态
    public func getAlertEnabled(for alertType: PAAlertType) -> Bool {
        return bridge.getAlertEnabled(for: alertType)
    }
    
    /// 显示告警
    public func showAlert(for alertType: PAAlertType) {
        bridge.showAlert(for: alertType)
    }
    
    /// 隐藏告警
    public func hideAlert() {
        alertWindowManager.hideAlert()
        self.currentAlert = nil
    }
    

    
    /// 设置检测阈值
    public func setDetectionThreshold(_ threshold: Float, for alertType: PAAlertType) {
        bridge.setDetectionThreshold(threshold, for: alertType)
    }
    
    /// 设置告警间隔
    public func setAlertInterval(_ intervalMs: Int32) {
        bridge.setAlertInterval(intervalMs)
    }
    
    /// 设置捕获间隔
    public func setCaptureInterval(_ intervalMs: Int32) {
        bridge.setCaptureInterval(intervalMs)
    }
    
    /// 获取版本信息
    public var version: String {
        return bridge.getVersion()
    }
    
    /// 设置日志级别
    public func setLogLevel(_ level: Int) {
        bridge.setLogLevel(level)
    }
    
    // MARK: - Screen Lock Methods
    
    /// 设置无人脸锁屏功能开关
    public func setNoFaceLockEnabled(_ enabled: Bool) {
        bridge.setNoFaceLockEnabled(enabled)
    }
    
    /// 设置无人脸锁屏超时时间（毫秒）
    public func setNoFaceLockTimeout(_ timeoutMs: Int32) {
        bridge.setNoFaceLockTimeout(timeoutMs)
    }
    
    /// 手动触发锁屏
    public func triggerScreenLock() {
        bridge.triggerScreenLock()
    }
    
    // MARK: - Private Methods
    
    private func setupCallbacks() {
        // 设置检测结果回调
        bridge.setDetectionCallback { [weak self] (result: PADetectionResult) in
            Task { @MainActor in
                // 累加检测结果而不是直接赋值
                // if let self = self {
                //     self.lastDetectionResult = PADetectionResult(
                //         lenCount: self.lastDetectionResult.lenCount + result.lenCount,
                //         phoneCount: self.lastDetectionResult.phoneCount + result.phoneCount,
                //         faceCount: self.lastDetectionResult.faceCount + result.faceCount,
                //         suspectedCount: self.lastDetectionResult.suspectedCount + result.suspectedCount
                //     )
                // }
                if let self = self {
                    self.lastDetectionResult = PADetectionResult(
                        lenCount: result.lenCount,
                        phoneCount: result.phoneCount,
                        faceCount: result.faceCount,
                        suspectedCount: result.suspectedCount
                    )
                }
            }
        }
        
        // 设置告警回调
        bridge.setAlertCallback { [weak self] (alertType: PAAlertType) in
            Task { @MainActor in
                guard let self = self else { return }
                self.currentAlert = alertType
                let alertMode = alertType.toAlertMode()
                self.alertWindowManager.showAlert(mode: alertMode)
            }
        }
        
        // 设置状态回调
        bridge.setStatusCallback { [weak self] (status: PADetectionStatus, errorMessage: String?) in
            Task { @MainActor in
                self?.detectionStatus = status
                self?.isDetectionRunning = (status.rawValue == 1)
                self?.errorMessage = errorMessage
            }
        }
    }
}

// MARK: - Convenience Extensions

extension PADetectManager {
    
    /// 使用默认配置初始化
    public func initializeWithDefaults() async throws {
        SwiftLogger.shared.debug("Starting initializeWithDefaults...")
        
        // 检查摄像头权限状态但不阻塞初始化
        SwiftLogger.shared.debug("Checking camera permission status...")
        let _ = checkCameraPermission()
        SwiftLogger.shared.debug("Camera permission status checked, continuing with initialization...")
        let bundle = Bundle.main
        SwiftLogger.shared.debug("Bundle path: \(bundle.bundlePath)")
        
        guard let modelPath = bundle.path(forResource: "best_640", ofType: "mnn", inDirectory: "mnn_model") else {
            SwiftLogger.shared.warning("Model file not found in bundle")
            // 列出Resources目录内容进行调试
            if let resourcesPath = bundle.resourcePath {
                SwiftLogger.shared.debug("Resources path: \(resourcesPath)")
                 do {
                     let contents = try FileManager.default.contentsOfDirectory(atPath: resourcesPath)
                     SwiftLogger.shared.debug("Resources contents: \(contents.description)")
                 } catch {
                     SwiftLogger.shared.error("Error listing resources: \(error.localizedDescription)")
                 }
            }
            throw NSError(
                domain: "PADetectManager",
                code: -1,
                userInfo: [NSLocalizedDescriptionKey: "Required MNN model file not found in bundle"]
            )
        }
        
        SwiftLogger.shared.debug("Model found at: \(modelPath)")
        
        // MNN模型不需要额外的配置文件
        let config = Configuration(
            modelPath: modelPath,
            configPath: "", // MNN不需要配置文件
            pipelinePath: "", // MNN不需要pipeline文件
            device: "AUTO"
        )
        
        SwiftLogger.shared.debug("Calling initialize with config...")
        try await initialize(with: config)
        SwiftLogger.shared.debug("Initialize completed successfully")
    }
    
    /// 加载默认配置文件
    public func loadDefaultConfigs() async throws {
        let bundle = Bundle.main
        
        if let configPath = bundle.path(forResource: "config", ofType: "json") {
            try await loadConfig(from: configPath)
        }
        
        if let serverConfigPath = bundle.path(forResource: "serverConfig", ofType: "json") {
            try await loadServerConfig(from: serverConfigPath)
        }
    }
}

// MARK: - Alert Type Extensions

extension PAAlertType {
    
    public var localizedDescription: String {
        switch self.rawValue {
        case 0: // PAAlertTypePhone
            return "手机检测告警"
        case 1: // PAAlertTypePeep
            return "偷窥风险告警"
        case 2: // PAAlertTypeNobody
            return "无人办公告警"
        case 3: // PAAlertTypeOcclude
            return "摄像头遮挡告警"
        case 4: // PAAlertTypeNoConnect
            return "摄像头连接异常告警"
        case 5: // PAAlertTypeSuspect
            return "可疑行为告警"
        default:
            return "未知告警类型"
        }
    }
    
    public var iconName: String {
        switch self.rawValue {
        case 0: // PAAlertTypePhone
            return "iphone"
        case 1: // PAAlertTypePeep
            return "eye.slash"
        case 2: // PAAlertTypeNobody
            return "person.slash"
        case 3: // PAAlertTypeOcclude
            return "video.slash"
        case 4: // PAAlertTypeNoConnect
            return "wifi.slash"
        case 5: // PAAlertTypeSuspect
            return "exclamationmark.triangle"
        default:
            return "questionmark"
        }
    }

    func toAlertMode() -> AlertMode {
        switch self.rawValue {
        case 0: return .phone
        case 1: return .peep
        case 2: return .nobody
        case 3: return .occlude
        case 4: return .noConnect
        case 5: return .suspect
        default:
            fatalError("Unsupported PAAlertType: \(self)")
        }
    }
}

// MARK: - Detection Status Extensions

extension PADetectionStatus {
    
    public var description: String {
        switch self.rawValue {
        case 0: // PADetectionStatusStopped
            return "已停止"
        case 1: // PADetectionStatusRunning
            return "运行中"
        case 2: // PADetectionStatusError
            return "错误"
        default:
            return "未知状态"
        }
    }
    
    public var color: String {
        switch self.rawValue {
        case 0: // PADetectionStatusStopped
            return "gray"
        case 1: // PADetectionStatusRunning
            return "green"
        case 2: // PADetectionStatusError
            return "red"
        default:
            return "gray"
        }
    }
}