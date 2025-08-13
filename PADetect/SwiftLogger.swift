//
//  SwiftLogger.swift
//  PADetect macOS
//

import Foundation
import os.log

/// Swift日志包装器，提供统一的日志输出接口
public class SwiftLogger {
    
    /// 共享实例
    public static let shared = SwiftLogger()
    
    /// 系统日志对象
    private let logger: Logger
    
    /// 日志文件路径
    private let logDirectory: URL
    private let logFileURL: URL
    
    /// 文件写入队列
    private let fileQueue = DispatchQueue(label: "com.padetect.logger.file", qos: .utility)
    
    /// 私有初始化器
    private init() {
        //self.logger = Logger(subsystem: Bundle.main.bundleIdentifier ?? "com.padetect.app", category: "PADetect")
        
        // 设置日志目录
        let projectPath = "/Users/bamboo/Documents/PADetect"
        self.logDirectory = URL(fileURLWithPath: projectPath).appendingPathComponent("logs")
        
        // 创建日志目录
        try? FileManager.default.createDirectory(at: logDirectory, withIntermediateDirectories: true, attributes: nil)
        
        // 设置日志文件名（按日期）
        let formatter = DateFormatter()
        formatter.dateFormat = "yyyy-MM-dd"
        let dateString = formatter.string(from: Date())
        self.logFileURL = logDirectory.appendingPathComponent("PADetect_\(dateString).log")
    }
    
    /// 调试级别日志
    /// - Parameter message: 日志消息
    public func debug(_ message: String) {
        //logger.debug("\(message, privacy: .public)")
        writeToFile(level: "DEBUG", message: message)
    }
    
    /// 信息级别日志
    /// - Parameter message: 日志消息
    public func info(_ message: String) {
       //logger.info("\(message, privacy: .public)")
        writeToFile(level: "INFO", message: message)
    }
    
    /// 警告级别日志
    /// - Parameter message: 日志消息
    public func warning(_ message: String) {
        //logger.warning("\(message, privacy: .public)")
        writeToFile(level: "WARNING", message: message)
    }
    
    /// 错误级别日志
    /// - Parameter message: 日志消息
    public func error(_ message: String) {
        //logger.error("\(message, privacy: .public)")
        writeToFile(level: "ERROR", message: message)
    }
    
    /// 严重错误级别日志
    /// - Parameter message: 日志消息
    public func critical(_ message: String) {
        //logger.critical("\(message, privacy: .public)")
        writeToFile(level: "CRITICAL", message: message)
    }
    
    /// 写入日志到文件
    /// - Parameters:
    ///   - level: 日志级别
    ///   - message: 日志消息
    private func writeToFile(level: String, message: String) {
        fileQueue.async { [weak self] in
            guard let self = self else { return }
            
            let timestamp = ISO8601DateFormatter().string(from: Date())
            let logEntry = "[\(timestamp)] [\(level)] \(message)\n"
            
            if let data = logEntry.data(using: .utf8) {
                if FileManager.default.fileExists(atPath: self.logFileURL.path) {
                    // 文件存在，追加内容
                    if let fileHandle = try? FileHandle(forWritingTo: self.logFileURL) {
                        fileHandle.seekToEndOfFile()
                        fileHandle.write(data)
                        fileHandle.closeFile()
                    }
                } else {
                    // 文件不存在，创建新文件
                    try? data.write(to: self.logFileURL)
                }
            }
        }
    }
}

// MARK: - 便捷宏定义

/// 调试日志宏
public func SWIFT_LOG_DEBUG(_ message: String) {
    SwiftLogger.shared.debug(message)
}

/// 信息日志宏
public func SWIFT_LOG_INFO(_ message: String) {
    SwiftLogger.shared.info(message)
}

/// 警告日志宏
public func SWIFT_LOG_WARNING(_ message: String) {
    SwiftLogger.shared.warning(message)
}

/// 错误日志宏
public func SWIFT_LOG_ERROR(_ message: String) {
    SwiftLogger.shared.error(message)
}

/// 严重错误日志宏
public func SWIFT_LOG_CRITICAL(_ message: String) {
    SwiftLogger.shared.critical(message)
}