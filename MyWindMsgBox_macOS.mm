//
//  MyWindMsgBox_macOS.mm
//  PADetect macOS Message Box Implementation
//

#include "MyWindMsgBox.h"

#if PLATFORM_MACOS

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import "PADetect/PADetectBridge.h"

// C++ 兼容的 macOS 消息框函数实现
extern "C" void showMacOSMessageBox(const char* title, const char* message, unsigned int style) {
    @autoreleasepool {
        NSString *titleStr = [NSString stringWithUTF8String:title];
        NSString *messageStr = [NSString stringWithUTF8String:message];
        
        dispatch_async(dispatch_get_main_queue(), ^{
            NSAlert *alert = [[NSAlert alloc] init];
            alert.messageText = titleStr;
            alert.informativeText = messageStr;
            
            // 根据Windows MessageBox样式设置NSAlert样式
            // MB_OK = 0x00000000L
            // MB_ICONERROR = 0x00000010L
            // MB_ICONWARNING = 0x00000030L
            // MB_ICONINFORMATION = 0x00000040L
            
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
}

#endif // PLATFORM_MACOS