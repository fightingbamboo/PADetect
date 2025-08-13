//
//  AlertWindowManager.swift
//  PADetect macOS
//

import SwiftUI
import AppKit
import Foundation

/// 告警模式枚举
enum AlertMode: Int, CaseIterable {
    case phone = 0
    case peep
    case nobody
    case occlude
    case noConnect
    case suspect
    
    var localizedDescription: String {
        switch self {
        case .phone:
            return "禁止 拍照"
        case .peep:
            return "存在偷窥风险 请检测周边"
        case .nobody:
            return "无人办公"
        case .occlude:
            return "摄像头遮挡"
        case .noConnect:
            return "摄像头异常 请检查线束连接"
        case .suspect:
            return "可疑行为检测"
        }
    }
    
    var backgroundColor: NSColor {
        switch self {
        case .phone:
            return NSColor.systemRed.withAlphaComponent(0.9)
        case .peep:
            return NSColor.systemOrange.withAlphaComponent(0.9)
        case .nobody:
            return NSColor.systemBlue.withAlphaComponent(0.9)
        case .occlude:
            return NSColor.systemPurple.withAlphaComponent(0.9)
        case .noConnect:
            return NSColor.systemYellow.withAlphaComponent(0.9)
        case .suspect:
            return NSColor.systemRed.withAlphaComponent(0.9)
        }
    }
}

/// 全屏告警窗口
class AlertWindow: NSWindow {
    private let alertMode: AlertMode
    private let alertText: String
    private let versionText: String
    private var hostingView: NSHostingView<AlertContentView>?
    
    init(screen: NSScreen, alertMode: AlertMode, versionText: String = "") {
        self.alertMode = alertMode
        self.alertText = alertMode.localizedDescription
        self.versionText = versionText
        
        super.init(
            contentRect: screen.frame,
            styleMask: [.borderless],
            backing: .buffered,
            defer: false
        )
        
        // 设置窗口所在的屏幕
        self.setFrame(screen.frame, display: true)
        
        setupWindow()
        setupContent()
    }
    
    deinit {
        NSLog("[AlertWindow] 开始释放窗口")
        
        // 清除 delegate 引用，避免悬空指针
        self.delegate = nil
        
        // 清理 hostingView
        if let hostingView = self.hostingView {
            hostingView.removeFromSuperview()
            self.hostingView = nil
        }
        
        NSLog("[AlertWindow] 窗口已释放")
    }
    
    private func setupWindow() {
        // 设置窗口属性
        self.level = .screenSaver  // 最高层级，覆盖所有内容
        self.backgroundColor = alertMode.backgroundColor
        self.isOpaque = false
        self.hasShadow = false
        self.ignoresMouseEvents = false
        self.canHide = false
        self.hidesOnDeactivate = false
        self.collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary]
        
        // 关键：防止窗口在关闭时自动释放，避免双重释放崩溃
        self.isReleasedWhenClosed = false
        
        // 确保窗口覆盖整个屏幕
        if let screen = self.screen {
            self.setFrame(screen.frame, display: true)
        }
    }
    
    private func setupContent() {
        // 创建内容视图
        let contentView = AlertContentView(
            alertText: alertText,
            versionText: versionText,
            backgroundColor: alertMode.backgroundColor,
            onDismiss: { [weak self] in
                DispatchQueue.main.async {
                    AlertWindowManager.shared.hideAlert()
                }
            }
        )
        
        // 使用 NSHostingView 来承载 SwiftUI 视图
        let hostingView = NSHostingView(rootView: contentView)
        hostingView.frame = self.frame
        hostingView.autoresizingMask = [.width, .height]
        
        self.hostingView = hostingView
        self.contentView = hostingView
    }
}

/// 告警内容视图
struct AlertContentView: View {
    let alertText: String
    let versionText: String
    let backgroundColor: NSColor
    let onDismiss: () -> Void
    
    var body: some View {
        ZStack {
            // 背景色
            Color(backgroundColor)
                .ignoresSafeArea(.all)
            
            VStack(spacing: 30) {
                Spacer()
                
                // 主要告警文本
                Text(alertText)
                    .font(.system(size: 72, weight: .bold, design: .default))
                    .foregroundColor(.white)
                    .multilineTextAlignment(.center)
                    .shadow(color: .black.opacity(0.5), radius: 4, x: 2, y: 2)
                
                // 版本信息（如果有）
                if !versionText.isEmpty {
                    Text(versionText)
                        .font(.system(size: 24, weight: .medium))
                        .foregroundColor(.white.opacity(0.8))
                        .multilineTextAlignment(.center)
                }
                
                Spacer()
                
                // 底部提示信息
                Text("按 ESC 键或点击任意位置关闭")
                    .font(.system(size: 18, weight: .regular))
                    .foregroundColor(.white.opacity(0.7))
                    .padding(.bottom, 50)
            }
            .padding(40)
        }
        .onTapGesture {
            // 点击关闭
            onDismiss()
        }
    }
}

/// 多显示器告警窗口管理器
class AlertWindowManager: ObservableObject {
    static let shared = AlertWindowManager()
    
    private var alertWindows: [AlertWindow] = []
    private var isShowing: Bool = false
    private var currentAlertMode: AlertMode = .phone
    private var versionText: String = ""
    private let windowQueue = DispatchQueue(label: "com.padetect.windowmanager", qos: .userInitiated)
    
    private init() {
        setupKeyboardMonitoring()
    }
    
    deinit {
        hideAlert()
    }
    
    // MARK: - Public Methods
    
    /// 显示告警
    func showAlert(mode: AlertMode = .phone, version: String = "") {
        DispatchQueue.main.async { [weak self] in
            guard let self = self else { return }
            
            // 如果已经在显示，先隐藏
            if self.isShowing {
                self.hideAlert()
            }
            
            self.currentAlertMode = mode
            self.versionText = version
            
            // 获取所有屏幕
            let screens = NSScreen.screens
            
            // 为每个屏幕创建告警窗口
            for screen in screens {
                let alertWindow = AlertWindow(
                    screen: screen,
                    alertMode: mode,
                    versionText: version
                )
                
                self.alertWindows.append(alertWindow)
                
                // 显示窗口
                alertWindow.makeKeyAndOrderFront(nil)
                alertWindow.orderFrontRegardless()
            }
            
            self.isShowing = true
            
            NSLog("[AlertWindowManager] 显示告警窗口，模式: \(mode.localizedDescription)，屏幕数量: \(screens.count)")
        }
    }
    
    /// 隐藏告警
    func hideAlert() {
        // 确保在主线程上执行
        if Thread.isMainThread {
            performHideAlert()
        } else {
            DispatchQueue.main.sync {
                performHideAlert()
            }
        }
    }
    
    private func performHideAlert() {
        guard isShowing else { return }
        
        let windowCount = alertWindows.count
        
        // 先设置状态，避免重复调用
        isShowing = false
        
        // 创建窗口副本，避免在迭代过程中修改原数组
        let windowsToClose = alertWindows
        alertWindows.removeAll()
        
        // 安全地关闭窗口：先隐藏，再关闭
        for window in windowsToClose {
            // 清除可能的 delegate 引用，避免悬空指针
            window.delegate = nil
            window.orderOut(nil)
        }
        
        // 延迟关闭窗口，确保所有 UI 操作完成
        DispatchQueue.main.async {
            for window in windowsToClose {
                window.close()
            }
        }
        
        NSLog("[AlertWindowManager] 隐藏告警窗口，关闭了 \(windowCount) 个窗口")
    }
    
    /// 检查是否正在显示告警
    func isAlertShowing() -> Bool {
        return isShowing
    }
    
    /// 设置告警模式
    func setAlertMode(_ mode: AlertMode) {
        currentAlertMode = mode
    }
    
    /// 设置版本信息
    func setVersionText(_ version: String) {
        versionText = version
    }
    
    // MARK: - Private Methods
    
    /// 设置键盘监听
    private func setupKeyboardMonitoring() {
        NSEvent.addGlobalMonitorForEvents(matching: .keyDown) { [weak self] event in
            // ESC 键关闭告警
            if event.keyCode == 53 { // ESC key
                self?.hideAlert()
            }
        }
        
        NSEvent.addLocalMonitorForEvents(matching: .keyDown) { [weak self] event in
            // ESC 键关闭告警
            if event.keyCode == 53 { // ESC key
                self?.hideAlert()
                return nil // 消费事件
            }
            return event
        }
    }
}

// MARK: - PAAlertType 扩展

extension PAAlertType {
    var alertMode: AlertMode {
        switch self {
        case .phone:
            return .phone
        case .peep:
            return .peep
        case .nobody:
            return .nobody
        case .occlude:
            return .occlude
        case .noConnect:
            return .noConnect
        case .suspect:
            return .suspect
        @unknown default:
            return .phone
        }
    }
}