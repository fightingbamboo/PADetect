//
//  PADetectView.swift
//  PADetect macOS
//

import SwiftUI
import AVFoundation

/// PADetect主界面视图
struct PADetectView: View {
    
    @StateObject private var detectManager = PADetectManager()
    @State private var isInitialized = false
    @State private var showingAlert = false
    @State private var alertMessage = ""
    @State private var cameraSettings = PADetectManager.CameraSettings()
    @State private var availableCameras: [String] = []
    @State private var selectedCameraIndex = 0
    @State private var showingPermissionAlert = false
    
    // 告警开关状态
    @State private var phoneAlertEnabled = true
    @State private var peepAlertEnabled = true
    @State private var nobodyAlertEnabled = true
    @State private var occludeAlertEnabled = true
    @State private var noConnectAlertEnabled = true
    @State private var suspectAlertEnabled = true
    
    // 锁屏设置
    @State private var noFaceLockEnabled = false
    @State private var noFaceLockTimeout: Int32 = 5000 // 默认5秒
    
    // 测试模式
    @State private var testModeEnabled = false
    @State private var sourcePreviewEnabled = false
    
    var body: some View {
        NavigationView {
            VStack(spacing: 20) {
                // 标题栏
                headerView
                
                // 状态显示
                statusView
                
                // 检测结果显示
                detectionResultView
                
                // 控制按钮
                controlButtonsView
                
                // 设置面板
            settingsView
            
            // 锁屏设置面板
            screenLockSettingsView
                
                Spacer()
            }
            .padding()
            .navigationTitle("PADetect 智能检测")
            .onAppear {
                NSLog("[PADetect DEBUG] PADetectView onAppear called")
                refreshCameraList()
                initializeDetector()
                setupNotificationObservers()
            }
            .onDisappear {
                removeNotificationObservers()
            }
            .alert("提示", isPresented: $showingAlert) {
                Button("确定") { }
            } message: {
                Text(alertMessage)
            }
            .onChange(of: detectManager.errorMessage) { errorMessage in
                if let error = errorMessage {
                    alertMessage = error
                    showingAlert = true
                }
            }
        }
    }
    
    // MARK: - Header View
    
    private var headerView: some View {
        HStack {
            Image(systemName: "camera.viewfinder")
                .font(.largeTitle)
                .foregroundColor(.blue)
            
            VStack(alignment: .leading) {
                Text("PADetect 智能检测系统")
                    .font(.title2)
                    .fontWeight(.bold)
                
                Text("版本: \(detectManager.version)")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            
            Spacer()
            
            // 状态指示器
            HStack {
                Circle()
                    .fill(statusColor)
                    .frame(width: 12, height: 12)
                
                Text(detectManager.detectionStatus.description)
                    .font(.caption)
                    .fontWeight(.medium)
            }
        }
        .padding()
        .background(Color(.controlBackgroundColor))
        .cornerRadius(10)
    }
    
    // MARK: - Status View
    
    private var statusView: some View {
        HStack(spacing: 15) {
            StatusCard(
                title: "检测状态",
                value: detectManager.isDetectionRunning ? "运行中" : "已停止",
                icon: "play.circle",
                color: detectManager.isDetectionRunning ? .green : .gray
            )
            
            StatusCard(
                title: "摄像头权限",
                value: cameraPermissionStatusText,
                icon: cameraPermissionIcon,
                color: cameraPermissionColor
            )
            
            StatusCard(
                title: "当前告警",
                value: detectManager.currentAlert?.alertMode.localizedDescription ?? "无",
                icon: "checkmark.circle",
                color: detectManager.currentAlert != nil ? .red : .green
            )
        }
    }
    
    // MARK: - Detection Result View
    
    private var detectionResultView: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("检测结果")
                .font(.headline)
                .fontWeight(.semibold)
            
            HStack(spacing: 15) {
                DetectionResultCard(
                    title: "手机",
                    count: Int32(detectManager.lastDetectionResult.phoneCount),
                    icon: "iphone",
                    color: .blue
                )
                
                DetectionResultCard(
                    title: "人脸",
                    count: Int32(detectManager.lastDetectionResult.faceCount),
                    icon: "person.crop.circle",
                    color: .orange
                )
                
                DetectionResultCard(
                    title: "镜头",
                    count: Int32(detectManager.lastDetectionResult.lenCount),
                    icon: "camera.circle",
                    color: .purple
                )
                
                DetectionResultCard(
                    title: "可疑",
                    count: Int32(detectManager.lastDetectionResult.suspectedCount),
                    icon: "exclamationmark.triangle",
                    color: .red
                )
            }
        }
        .padding()
        .background(Color(.controlBackgroundColor))
        .cornerRadius(10)
    }
    
    // MARK: - Control Buttons View
    
    private var controlButtonsView: some View {
        HStack(spacing: 15) {
            Button(action: {
                if detectManager.isDetectionRunning {
                    detectManager.stopDetection()
                } else {
                    startDetection()
                }
            }) {
                HStack {
                    Image(systemName: detectManager.isDetectionRunning ? "stop.circle" : "play.circle")
                    Text(detectManager.isDetectionRunning ? "停止检测" : "开始检测")
                }
                .frame(maxWidth: .infinity)
            }
            .buttonStyle(.borderedProminent)
            .disabled(!isInitialized || detectManager.cameraPermissionStatus != .authorized)
            
            Button("重新初始化") {
                    initializeDetector()
                }
                .buttonStyle(.bordered)
                
                Button("测试告警弹窗") {
                    testAlertPopup()
                }
                .buttonStyle(.bordered)
                .foregroundColor(.orange)
        }
    }
    
    // MARK: - Settings View
    
    private var settingsView: some View {
        VStack(alignment: .leading, spacing: 15) {
            Text("设置")
                .font(.headline)
                .fontWeight(.semibold)
            
            // 告警设置
            alertSettingsView
            
            // 摄像头设置
            cameraSettingsView
            
            // 测试模式设置
//            testModeSettingsView
        }
        .padding()
        .background(Color(.controlBackgroundColor))
        .cornerRadius(10)
    }
    
    // MARK: - Alert Settings View
    
    private var alertSettingsView: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("告警设置")
                .font(.subheadline)
                .fontWeight(.medium)
            
            LazyVGrid(columns: Array(repeating: GridItem(.flexible()), count: 2), spacing: 8) {
                AlertToggle(title: "手机检测", isOn: $phoneAlertEnabled, alertType: PAAlertType(rawValue: 0)!)
                AlertToggle(title: "偷窥检测", isOn: $peepAlertEnabled, alertType: PAAlertType(rawValue: 1)!)
                AlertToggle(title: "无人检测", isOn: $nobodyAlertEnabled, alertType: PAAlertType(rawValue: 2)!)
                AlertToggle(title: "遮挡检测", isOn: $occludeAlertEnabled, alertType: PAAlertType(rawValue: 3)!)
                AlertToggle(title: "连接检测", isOn: $noConnectAlertEnabled, alertType: PAAlertType(rawValue: 4)!)
                AlertToggle(title: "可疑行为", isOn: $suspectAlertEnabled, alertType: PAAlertType(rawValue: 5)!)
            }
        }
        .onChange(of: phoneAlertEnabled) { enabled in
            detectManager.setAlertEnabled(enabled, for: PAAlertType(rawValue: 0)!)
        }
        .onChange(of: peepAlertEnabled) { enabled in
            detectManager.setAlertEnabled(enabled, for: PAAlertType(rawValue: 1)!)
        }
        .onChange(of: nobodyAlertEnabled) { enabled in
            detectManager.setAlertEnabled(enabled, for: PAAlertType(rawValue: 2)!)
        }
        .onChange(of: occludeAlertEnabled) { enabled in
            detectManager.setAlertEnabled(enabled, for: PAAlertType(rawValue: 3)!)
        }
        .onChange(of: noConnectAlertEnabled) { enabled in
            detectManager.setAlertEnabled(enabled, for: PAAlertType(rawValue: 4)!)
        }
        .onChange(of: suspectAlertEnabled) { enabled in
            detectManager.setAlertEnabled(enabled, for: PAAlertType(rawValue: 5)!)
        }
    }
    
    // MARK: - Camera Settings View
    
    private var cameraSettingsView: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("摄像头设置")
                .font(.subheadline)
                .fontWeight(.medium)
            
            // 权限状态显示
            if detectManager.cameraPermissionStatus != .authorized {
                HStack {
                    Image(systemName: cameraPermissionIcon)
                        .foregroundColor(cameraPermissionColor)
                    
                    Text("摄像头权限: \(cameraPermissionStatusText)")
                        .foregroundColor(cameraPermissionColor)
                    
                    Spacer()
                    
                    if detectManager.cameraPermissionStatus == .denied {
                        Button("打开设置") {
                            detectManager.openSystemPreferences()
                        }
                        .buttonStyle(.borderedProminent)
                        .controlSize(.small)
                    } else if detectManager.cameraPermissionStatus == .notDetermined {
                        Button("请求权限") {
                            requestCameraPermission()
                        }
                        .buttonStyle(.borderedProminent)
                        .controlSize(.small)
                    }
                }
                .padding(.vertical, 4)
                .padding(.horizontal, 8)
                .background(cameraPermissionColor.opacity(0.1))
                .cornerRadius(6)
            }
            
            VStack(spacing: 8) {
                HStack {
                    Text("摄像头:")
                    Picker("选择摄像头", selection: $selectedCameraIndex) {
                        ForEach(0..<availableCameras.count, id: \.self) { index in
                            Text(availableCameras[index])
                                .tag(index)
                        }
                    }
                    .pickerStyle(.menu)
                    .frame(maxWidth: .infinity)
                    .disabled(detectManager.cameraPermissionStatus != .authorized)
                    
                    Button("刷新") {
                        refreshCameraList()
                    }
                    .buttonStyle(.bordered)
                    .disabled(detectManager.cameraPermissionStatus != .authorized)
                }
                
                HStack {
                    Text("分辨率:")
                    TextField("640", value: $cameraSettings.width, format: .number)
                        .textFieldStyle(.roundedBorder)
                        .frame(width: 80)
                        .disabled(detectManager.cameraPermissionStatus != .authorized)
                    Text("×")
                    TextField("480", value: $cameraSettings.height, format: .number)
                        .textFieldStyle(.roundedBorder)
                        .frame(width: 80)
                        .disabled(detectManager.cameraPermissionStatus != .authorized)
                    
                    Spacer()
                    
                    Button("应用") {
                        applyCameraSettings()
                    }
                    .buttonStyle(.bordered)
                    .disabled(detectManager.cameraPermissionStatus != .authorized)
                }
            }
        }
    }
    
    // MARK: - Test Mode Settings View
    
    private var testModeSettingsView: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("测试模式")
                .font(.subheadline)
                .fontWeight(.medium)
            
            HStack {
                Toggle("启用测试模式", isOn: $testModeEnabled)
                    .onChange(of: testModeEnabled) { enabled in
                        detectManager.setTestMode(enabled: enabled)
                    }
                
                Spacer()
                
                Toggle("源预览", isOn: $sourcePreviewEnabled)
                    .onChange(of: sourcePreviewEnabled) { enabled in
                        detectManager.setSourcePreview(enabled: enabled)
                    }
            }
        }
    }
    
    // MARK: - Screen Lock Settings View
    
    private var screenLockSettingsView: some View {
        VStack(alignment: .leading, spacing: 15) {
            Text("锁屏设置")
                .font(.headline)
                .fontWeight(.semibold)
            
            VStack(alignment: .leading, spacing: 10) {
                HStack {
                    Image(systemName: "lock.circle")
                        .foregroundColor(noFaceLockEnabled ? .blue : .secondary)
                        .frame(width: 16)
                    
                    Text("无人脸自动锁屏")
                        .font(.subheadline)
                    
                    Spacer()
                    
                    Toggle("", isOn: $noFaceLockEnabled)
                        .toggleStyle(.switch)
                        .onChange(of: noFaceLockEnabled) { enabled in
                            detectManager.setNoFaceLockEnabled(enabled)
                        }
                }
                
                if noFaceLockEnabled {
                    HStack {
                        Text("锁屏延时:")
                            .font(.subheadline)
                        
                        TextField("5000", value: $noFaceLockTimeout, format: .number)
                            .textFieldStyle(.roundedBorder)
                            .frame(width: 80)
                            .onChange(of: noFaceLockTimeout) { timeout in
                                detectManager.setNoFaceLockTimeout(timeout)
                            }
                        
                        Text("毫秒")
                            .font(.caption)
                            .foregroundColor(.secondary)
                        
                        Spacer()
                        
                        Button("立即锁屏") {
                            detectManager.triggerScreenLock()
                        }
                        .buttonStyle(.bordered)
                        .controlSize(.small)
                    }
                    .padding(.leading, 20)
                }
            }
        }
        .padding()
        .background(Color(.controlBackgroundColor))
        .cornerRadius(10)
    }
    
    // MARK: - Private Computed Properties
    
    private var statusColor: Color {
        switch detectManager.detectionStatus {
        case .running:
            return .green
        case .stopped:
            return .gray
        case .error:
            return .red
        @unknown default:
            return .gray
        }
    }
    
    // MARK: - Camera Permission Computed Properties
    
    private var cameraPermissionStatusText: String {
        switch detectManager.cameraPermissionStatus {
        case .notDetermined:
            return "未确定"
        case .denied:
            return "已拒绝"
        case .authorized:
            return "已授权"
        case .restricted:
            return "受限制"
        }
    }
    
    private var cameraPermissionIcon: String {
        switch detectManager.cameraPermissionStatus {
        case .notDetermined:
            return "questionmark.circle"
        case .denied:
            return "xmark.circle"
        case .authorized:
            return "checkmark.circle"
        case .restricted:
            return "exclamationmark.circle"
        }
    }
    
    private var cameraPermissionColor: Color {
        switch detectManager.cameraPermissionStatus {
        case .notDetermined:
            return .orange
        case .denied:
            return .red
        case .authorized:
            return .green
        case .restricted:
            return .yellow
        }
    }
    
    // MARK: - Methods
    
    private func initializeDetector() {
        NSLog("[PADetect DEBUG] initializeDetector called")
        Task {
            do {
                NSLog("[PADetect DEBUG] About to call initializeWithDefaults")
                try await detectManager.initializeWithDefaults()
                NSLog("[PADetect DEBUG] initializeWithDefaults completed")
                try await detectManager.loadDefaultConfigs()
                NSLog("[PADetect DEBUG] loadDefaultConfigs completed")
                await MainActor.run {
                    isInitialized = true
                    
                    // 从配置文件中读取告警开关的实际状态并同步到UI
                    phoneAlertEnabled = detectManager.getAlertEnabled(for: .phone)
                    peepAlertEnabled = detectManager.getAlertEnabled(for: .peep)
                    nobodyAlertEnabled = detectManager.getAlertEnabled(for: .nobody)
                    occludeAlertEnabled = detectManager.getAlertEnabled(for: .occlude)
                    noConnectAlertEnabled = detectManager.getAlertEnabled(for: .noConnect)
                    suspectAlertEnabled = detectManager.getAlertEnabled(for: .suspect)
                    
                    NSLog("[PADetect DEBUG] Alert states synced from config: phone=\(phoneAlertEnabled), peep=\(peepAlertEnabled), nobody=\(nobodyAlertEnabled), occlude=\(occludeAlertEnabled), noConnect=\(noConnectAlertEnabled), suspect=\(suspectAlertEnabled)")
                    
                    alertMessage = "检测器初始化成功"
                    showingAlert = true
                    NSLog("[PADetect DEBUG] Initialization successful")
                }
            } catch {
                NSLog("[PADetect DEBUG] Initialization failed: %@", error.localizedDescription)
                await MainActor.run {
                    isInitialized = false
                    alertMessage = "初始化失败: \(error.localizedDescription)"
                    showingAlert = true
                }
            }
        }
    }
    
    private func startDetection() {
        Task {
            do {
                try await detectManager.startDetection()
            } catch {
                await MainActor.run {
                    alertMessage = "启动检测失败: \(error.localizedDescription)"
                    showingAlert = true
                }
            }
        }
    }
    
    private func refreshCameraList() {
        availableCameras = detectManager.getAvailableCameras()
        if availableCameras.isEmpty {
            availableCameras = ["No Camera Found"]
        }
        // 确保选中的索引有效
        if selectedCameraIndex >= availableCameras.count {
            selectedCameraIndex = 0
        }
    }
    
    private func applyCameraSettings() {
        do {
            // 使用选中的摄像头索引
            cameraSettings.cameraId = Int32(selectedCameraIndex)
            try detectManager.setCameraSettings(cameraSettings)
            alertMessage = "摄像头设置已应用: \(availableCameras[selectedCameraIndex])"
            showingAlert = true
        } catch {
            alertMessage = "设置摄像头失败: \(error.localizedDescription)"
            showingAlert = true
        }
    }
    
    private func requestCameraPermission() {
        Task {
            let granted = await detectManager.requestCameraPermission()
            await MainActor.run {
                if granted {
                    alertMessage = "摄像头权限已授权"
                    showingAlert = true
                    refreshCameraList()
                } else {
                    alertMessage = "摄像头权限被拒绝。请在系统设置 > 隐私与安全性 > 摄像头中启用PADetect的摄像头权限。"
                    showingAlert = true
                }
            }
        }
    }
    
    private func testAlertPopup() {
        // 测试多显示器全屏弹窗
        let alertTypes: [PAAlertType] = [
            PAAlertType(rawValue: 0)!, // phone
            PAAlertType(rawValue: 1)!, // peep
            PAAlertType(rawValue: 2)!, // nobody
            PAAlertType(rawValue: 3)!, // occlude
            PAAlertType(rawValue: 4)!, // noConnect
            PAAlertType(rawValue: 5)!  // suspect
        ]
        let randomType = alertTypes.randomElement() ?? PAAlertType(rawValue: 0)!
        
        NSLog("[PADetect DEBUG] 测试告警弹窗，类型: \(randomType.alertMode.localizedDescription)")
        
        // 显示弹窗3秒后自动隐藏
        detectManager.showAlert(for: randomType)
        
        DispatchQueue.main.asyncAfter(deadline: .now() + 3.0) {
            self.detectManager.hideAlert()
            NSLog("[PADetect DEBUG] 自动隐藏告警弹窗")
        }
        
        // 显示提示信息
        alertMessage = "已触发\(randomType.alertMode.localizedDescription)测试，弹窗将在3秒后自动关闭"
        showingAlert = true
    }
    
    // MARK: - Notification Observers
    
    private func setupNotificationObservers() {
         // 监听来自PADetectBridge的显示全屏弹窗通知
         NotificationCenter.default.addObserver(
             forName: NSNotification.Name("ShowFullScreenAlert"),
             object: nil,
             queue: .main
         ) { notification in
             NSLog("[PADetect DEBUG] 收到显示全屏弹窗通知")
             
             if let userInfo = notification.userInfo,
                let alertTypeRaw = userInfo["alertType"] as? Int,
                let version = userInfo["version"] as? String {
                 
                 // 将整数转换为AlertMode
                 let mode: AlertMode
                 switch alertTypeRaw {
                 case 0: mode = .phone
                 case 1: mode = .peep
                 case 2: mode = .nobody
                 case 3: mode = .occlude
                 case 4: mode = .noConnect
                 case 5: mode = .suspect
                 default: mode = .phone
                 }
                 
                 NSLog("[PADetect DEBUG] 弹窗类型: \(mode), 版本: \(version)")
                 AlertWindowManager.shared.showAlert(mode: mode, version: version)
             }
         }
         
         // 监听来自PADetectBridge的隐藏全屏弹窗通知
         NotificationCenter.default.addObserver(
             forName: NSNotification.Name("HideFullScreenAlert"),
             object: nil,
             queue: .main
         ) { notification in
             NSLog("[PADetect DEBUG] 收到隐藏全屏弹窗通知")
             AlertWindowManager.shared.hideAlert()
         }
         
         // 监听AlertWindowManager的状态通知（用于调试）
         NotificationCenter.default.addObserver(
             forName: NSNotification.Name("PADetectAlertDidShow"),
             object: nil,
             queue: .main
         ) { notification in
             NSLog("[PADetect DEBUG] 全屏弹窗已显示")
         }
         
         NotificationCenter.default.addObserver(
             forName: NSNotification.Name("PADetectAlertDidHide"),
             object: nil,
             queue: .main
         ) { notification in
             NSLog("[PADetect DEBUG] 全屏弹窗已隐藏")
         }
     }
    
    private func removeNotificationObservers() {
         NotificationCenter.default.removeObserver(self, name: NSNotification.Name("ShowFullScreenAlert"), object: nil)
         NotificationCenter.default.removeObserver(self, name: NSNotification.Name("HideFullScreenAlert"), object: nil)
         NotificationCenter.default.removeObserver(self, name: NSNotification.Name("PADetectAlertDidShow"), object: nil)
         NotificationCenter.default.removeObserver(self, name: NSNotification.Name("PADetectAlertDidHide"), object: nil)
     }
}

// MARK: - Supporting Views

struct StatusCard: View {
    let title: String
    let value: String
    let icon: String
    let color: Color
    
    var body: some View {
        VStack(spacing: 8) {
            Image(systemName: icon)
                .font(.title2)
                .foregroundColor(color)
            
            Text(title)
                .font(.caption)
                .foregroundColor(.secondary)
            
            Text(value)
                .font(.headline)
                .fontWeight(.semibold)
                .multilineTextAlignment(.center)
        }
        .frame(maxWidth: .infinity)
        .padding()
        .background(Color(.controlBackgroundColor))
        .cornerRadius(8)
    }
}

struct DetectionResultCard: View {
    let title: String
    let count: Int32
    let icon: String
    let color: Color
    
    var body: some View {
        VStack(spacing: 6) {
            Image(systemName: icon)
                .font(.title3)
                .foregroundColor(color)
            
            Text(title)
                .font(.caption2)
                .foregroundColor(.secondary)
            
            Text("\(count)")
                .font(.title2)
                .fontWeight(.bold)
                .foregroundColor(count > 0 ? color : .secondary)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 8)
        .background(count > 0 ? color.opacity(0.1) : Color.clear)
        .cornerRadius(6)
    }
}

struct AlertToggle: View {
    let title: String
    @Binding var isOn: Bool
    let alertType: PAAlertType
    
    private func getIconName(for alertType: PAAlertType) -> String {
        switch alertType.rawValue {
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
    
    var body: some View {
        HStack {
            Image(systemName: getIconName(for: alertType))
                .foregroundColor(isOn ? .blue : .secondary)
                .frame(width: 16)
            
            Text(title)
                .font(.caption)
            
            Spacer()
            
            Toggle("", isOn: $isOn)
                .toggleStyle(.switch)
                .scaleEffect(0.8)
        }
        .padding(.horizontal, 8)
        .padding(.vertical, 4)
    }
}

// MARK: - Preview

struct PADetectView_Previews: PreviewProvider {
    static var previews: some View {
        PADetectView()
            .frame(width: 800, height: 600)
    }
}
