/*
 * Copyright 2024 Sheng Han
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef __APPLE__
#include "screenShot.hpp"

#if !USE_LEGACY_CAPTURE
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <Foundation/Foundation.h>

bool ScreenShotMacOs::initScreenCaptureKit() {
    @autoreleasepool {
        // 检查屏幕录制权限
        if (@available(macOS 11.0, *)) {
            CGRequestScreenCaptureAccess();
        }
        
        captureSemaphore = dispatch_semaphore_create(0);
        if (!captureSemaphore) {
            NSLog(@"Failed to create capture semaphore");
            return false;
        }
        
        lastCapturedImage = nil;
        
        [SCShareableContent getShareableContentWithCompletionHandler:^(SCShareableContent* content, NSError* error) {
            if (error) {
                NSLog(@"Failed to get shareable content: %@", error.localizedDescription);
                dispatch_semaphore_signal(captureSemaphore);
                return;
            }
            
            for (SCDisplay* display in content.displays) {
                if (display.displayID == displayID) {
                    currentDisplay = [display retain];
                    break;
                }
            }
            
            if (!currentDisplay && content.displays.count > 0) {
                currentDisplay = [content.displays.firstObject retain];
                displayID = currentDisplay.displayID;
                width = (int)currentDisplay.width;
                height = (int)currentDisplay.height;
            }
            
            if (currentDisplay) {
                contentFilter = [[SCContentFilter alloc] initWithDisplay:currentDisplay excludingWindows:@[]];
                
                streamConfig = [[SCStreamConfiguration alloc] init];
                streamConfig.width = currentDisplay.width;
                streamConfig.height = currentDisplay.height;
                streamConfig.pixelFormat = kCVPixelFormatType_32BGRA;
                streamConfig.showsCursor = YES;
            }
            
            dispatch_semaphore_signal(captureSemaphore);
        }];
        
        dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC);
        if (dispatch_semaphore_wait(captureSemaphore, timeout) != 0) {
            NSLog(@"Timeout waiting for ScreenCaptureKit initialization");
            return false;
        }
        
        return currentDisplay != nil;
    }
}

void ScreenShotMacOs::captureWithScreenCaptureKit(uint8_t* buffer) {
    @autoreleasepool {
        if (!currentDisplay || !contentFilter || !streamConfig) {
            NSLog(@"ScreenCaptureKit not properly initialized");
            memset(buffer, 0, width * height * 4);
            return;
        }
        
        // 检查屏幕录制权限
        if (@available(macOS 14.0, *)) {
            if (!CGPreflightScreenCaptureAccess()) {
                NSLog(@"Screen recording permission not granted");
                memset(buffer, 0, width * height * 4);
                return;
            }
        }
        
        [SCScreenshotManager captureImageWithFilter:contentFilter configuration:streamConfig completionHandler:^(CGImageRef imageRef, NSError* error) {
            if (error) {
                NSLog(@"Screenshot capture failed: %@", error.localizedDescription);
                if (error.code == -3801) {
                    NSLog(@"Screen recording permission denied");
                }
                memset(buffer, 0, width * height * 4);
            } else if (imageRef) {
                convertCGImageToBuffer(imageRef, buffer);
            } else {
                NSLog(@"No image captured");
                memset(buffer, 0, width * height * 4);
            }
            
            dispatch_semaphore_signal(captureSemaphore);
        }];
        
        dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC);
        long result = dispatch_semaphore_wait(captureSemaphore, timeout);
        if (result != 0) {
            NSLog(@"Timeout waiting for screenshot capture (waited 3 seconds)");
            memset(buffer, 0, width * height * 4);
            // 重置信号量状态
            while (dispatch_semaphore_wait(captureSemaphore, DISPATCH_TIME_NOW) == 0) {
                // 清空可能的信号
            }
        }
    }
}

void ScreenShotMacOs::convertCGImageToBuffer(CGImageRef imageRef, uint8_t* buffer) {
    if (!imageRef) {
        memset(buffer, 0, width * height * 4);
        return;
    }
    
    size_t imgWidth = CGImageGetWidth(imageRef);
    size_t imgHeight = CGImageGetHeight(imageRef);
    size_t bytesPerRow = CGImageGetBytesPerRow(imageRef);
    
    CFDataRef pixelDataRef = CGDataProviderCopyData(CGImageGetDataProvider(imageRef));
    if (pixelDataRef) {
        const uint8_t* pixelData = CFDataGetBytePtr(pixelDataRef);
        size_t totalBytes = bytesPerRow * imgHeight;
        
        if (pixelData && totalBytes <= (size_t)(width * height * 4)) {
            memcpy(buffer, pixelData, totalBytes);
        } else {
            memset(buffer, 0, width * height * 4);
        }
        
        CFRelease(pixelDataRef);
    } else {
        memset(buffer, 0, width * height * 4);
    }
}

bool ScreenShotMacOs::isScreenCaptureKitAvailable() {
    if (@available(macOS 12.3, *)) {
        return [SCShareableContent class] != nil && [SCScreenshotManager class] != nil;
    }
    return false;
}

#endif // !USE_LEGACY_CAPTURE
#endif // __APPLE__