#ifdef __APPLE__

#include "../desktop_integration.h"
#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreFoundation/CoreFoundation.h>
#import <Carbon/Carbon.h>
#include <vector>
#include <iostream>
#include <unistd.h>

// Global state
static NSWindow* g_desktopWindow = nil;
static MonitorInfo g_selectedMonitor = {0, 0, 0, 0};
static bool g_currentMouseState[5] = {false};
static bool g_previousMouseState[5] = {false};

// Custom window class for desktop background
@interface DesktopWindow : NSWindow
@end

@implementation DesktopWindow

- (BOOL)canBecomeKeyWindow {
    return NO;
}

- (BOOL)canBecomeMainWindow {
    return NO;
}

- (BOOL)acceptsFirstResponder {
    return NO;
}

- (NSWindowLevel)level {
    return kCGDesktopWindowLevel;
}

@end

namespace DesktopIntegration {

bool Initialize() {
    // Hide the app from the dock since this is a wallpaper replacement
    ProcessSerialNumber psn = { 0, kCurrentProcess };
    TransformProcessType(&psn, kProcessTransformToUIElementApplication);
    
    return true;
}

void Cleanup() {
    if (g_desktopWindow) {
        [g_desktopWindow close];
        g_desktopWindow = nil;
    }
}

std::vector<MonitorInfo> EnumerateMonitors() {
    std::vector<MonitorInfo> monitors;
    
    NSArray<NSScreen*>* screens = [NSScreen screens];
    
    for (NSScreen* screen in screens) {
        NSRect frame = [screen frame];
        
        MonitorInfo monitor;
        monitor.x = (int)frame.origin.x;
        monitor.y = (int)frame.origin.y;
        monitor.width = (int)frame.size.width;
        monitor.height = (int)frame.size.height;
        
        monitors.push_back(monitor);
    }
    
    return monitors;
}

MonitorInfo GetWallpaperTarget(int monitorIndex) {
    std::vector<MonitorInfo> monitors = EnumerateMonitors();
    
    if (monitorIndex < 0 || monitorIndex >= static_cast<int>(monitors.size())) {
        // Return combined desktop area
        MonitorInfo combined = {0, 0, 0, 0};
        
        if (!monitors.empty()) {
            int minX = monitors[0].x;
            int minY = monitors[0].y;
            int maxX = monitors[0].x + monitors[0].width;
            int maxY = monitors[0].y + monitors[0].height;
            
            for (const auto& monitor : monitors) {
                minX = std::min(minX, monitor.x);
                minY = std::min(minY, monitor.y);
                maxX = std::max(maxX, monitor.x + monitor.width);
                maxY = std::max(maxY, monitor.y + monitor.height);
            }
            
            combined.x = minX;
            combined.y = minY;
            combined.width = maxX - minX;
            combined.height = maxY - minY;
        }
        
        return combined;
    } else {
        return monitors[monitorIndex];
    }
}

void ConfigureWallpaperWindow(void* windowHandle, const MonitorInfo& monitor) {
    NSWindow* raylibWindow = (__bridge NSWindow*)windowHandle;
    if (!raylibWindow) {
        return;
    }
    
    g_selectedMonitor = monitor;
    
    // Configure the raylib window for desktop background behavior
    [raylibWindow setLevel:kCGDesktopWindowLevel];
    [raylibWindow setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces | 
                                        NSWindowCollectionBehaviorStationary |
                                        NSWindowCollectionBehaviorIgnoresCycle];
    
    // Use logical coordinates directly - raylib handles high DPI internally
    NSRect screenFrame = NSMakeRect(monitor.x, monitor.y, monitor.width, monitor.height);
    
    // Position and size the window
    [raylibWindow setFrame:screenFrame display:YES];
    
    // Make the window non-interactive for desktop integration
    [raylibWindow setIgnoresMouseEvents:NO]; // We still want mouse events for interaction
    [raylibWindow setAcceptsMouseMovedEvents:YES];
    
    // hide the app from the dock
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    
    g_desktopWindow = raylibWindow;
}

bool IsMonitorOccluded(const MonitorInfo& monitor, double threshold) {
    @autoreleasepool {
        // First check if Mission Control is active - if so, always render
        // Mission Control is detected by the presence of a "Dock - (null)" window
        CFArrayRef windowList = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
        
        if (windowList == NULL) {
            //std::cout << "No window list" << std::endl;
            return false;
        }
        
        bool missionControlActive = false;
        bool hasFullscreenWindow = false;
        int occludedPixels = 0;
        int totalPixels = monitor.width * monitor.height;
        
        // Get our own process ID for comparison
        pid_t ourPID = getpid();
        
        CFIndex windowCount = CFArrayGetCount(windowList);
        
        for (CFIndex i = 0; i < windowCount; i++) {
            CFDictionaryRef windowInfo = (CFDictionaryRef)CFArrayGetValueAtIndex(windowList, i);
            
            // Get window owner info
            CFStringRef ownerName = (CFStringRef)CFDictionaryGetValue(windowInfo, kCGWindowOwnerName);
            CFStringRef windowName = (CFStringRef)CFDictionaryGetValue(windowInfo, kCGWindowName);
            CFNumberRef ownerPIDRef = (CFNumberRef)CFDictionaryGetValue(windowInfo, kCGWindowOwnerPID);
            
            pid_t ownerPID = 0;
            if (ownerPIDRef) {
                CFNumberGetValue(ownerPIDRef, kCFNumberIntType, &ownerPID);
            }

            // Check for Mission Control (Dock window with null name)
            if (ownerName && windowName) {
                NSString* ownerStr = (__bridge NSString*)ownerName;
                NSString* windowStr = (__bridge NSString*)windowName;
                
                if ([ownerStr isEqualToString:@"Dock"] && 
                    ([windowStr isEqualToString:@"(null)"] || [windowStr length] == 0)) {
                    
                    // Get window bounds to check if it's the Mission Control overlay
                    CFDictionaryRef bounds = (CFDictionaryRef)CFDictionaryGetValue(windowInfo, kCGWindowBounds);
                    if (bounds) {
                        CGRect windowRect;
                        CGRectMakeWithDictionaryRepresentation(bounds, &windowRect);
                        
                        // Mission Control dock window typically has a Y origin of -1 or covers a large area
                        if (windowRect.origin.y <= 0 || windowRect.size.width > 1000) {
                            missionControlActive = true;
                            break;
                        }
                    }
                }
            }
            
            // Check for regular windows that might occlude our monitor
            if (ownerName && ownerPID != 0) {
                NSString* ownerStr = (__bridge NSString*)ownerName;
                
                // Skip our own process and system processes
                if (ownerPID == ourPID ||
                    [ownerStr isEqualToString:@"Window Server"] ||
                    [ownerStr isEqualToString:@"Dock"] ||
                    [ownerStr isEqualToString:@"Finder"] ||
                    [ownerStr isEqualToString:@"Wallpaper"]) {
                    continue;
                }
                
                // Get window bounds
                CFDictionaryRef bounds = (CFDictionaryRef)CFDictionaryGetValue(windowInfo, kCGWindowBounds);
                if (bounds) {
                    CGRect windowRect;
                    CGRectMakeWithDictionaryRepresentation(bounds, &windowRect);
                    
                    // Convert monitor info to CGRect
                    CGRect monitorRect = CGRectMake(monitor.x, monitor.y, monitor.width, monitor.height);
                    
                    // Check if window intersects with our monitor
                    CGRect intersection = CGRectIntersection(windowRect, monitorRect);
                    
                    if (!CGRectIsEmpty(intersection)) {
                        int intersectionPixels = (int)(intersection.size.width * intersection.size.height);
                        occludedPixels += intersectionPixels;
                        
                        // Check if this is a fullscreen window (covers most of the monitor)
                        double windowCoverage = intersectionPixels / (double)totalPixels;
                        if (windowCoverage > 0.9) { // 90% coverage indicates fullscreen
                            //std::cout << "window name: " << [ownerStr UTF8String] << std::endl;
                            hasFullscreenWindow = true;
                        }
                    }
                }
            }
        }
        
        CFRelease(windowList);
        
        // If Mission Control is active, never consider the monitor occluded
        if (missionControlActive) {
            //std::cout << "Mission Control active" << std::endl;
            return false;
        }
        
        // If there's a fullscreen window, consider it occluded regardless of threshold
        if (hasFullscreenWindow) {
            //std::cout << "Fullscreen window active" << std::endl;
            return true;
        }
        
        // Otherwise, use the threshold-based approach
        double occlusionFraction = (double)occludedPixels / (double)totalPixels;
        return occlusionFraction >= threshold;
    }
}

bool IsDesktopLocked() {
    // TODO: Implement desktop locking detection for macOS
    return false;
}

void UpdateMouseState() {
    // Save previous state
    for (int i = 0; i < 5; i++) {
        g_previousMouseState[i] = g_currentMouseState[i];
    }
    
    // Get current mouse button state
    NSUInteger pressedButtons = [NSEvent pressedMouseButtons];
    
    g_currentMouseState[0] = (pressedButtons & (1 << 0)) != 0; // Left button
    g_currentMouseState[1] = (pressedButtons & (1 << 1)) != 0; // Right button
    g_currentMouseState[2] = (pressedButtons & (1 << 2)) != 0; // Middle button
    g_currentMouseState[3] = (pressedButtons & (1 << 3)) != 0; // Button 4
    g_currentMouseState[4] = (pressedButtons & (1 << 4)) != 0; // Button 5
}

bool IsMouseButtonPressed(int button) {
    if (button < 0 || button >= 5) return false;
    return g_currentMouseState[button] && !g_previousMouseState[button];
}

bool IsMouseButtonDown(int button) {
    if (button < 0 || button >= 5) return false;
    return g_currentMouseState[button];
}

bool IsMouseButtonReleased(int button) {
    if (button < 0 || button >= 5) return false;
    return !g_currentMouseState[button] && g_previousMouseState[button];
}

bool IsMouseButtonUp(int button) {
    if (button < 0 || button >= 5) return false;
    return !g_currentMouseState[button];
}

int GetMouseX() {
    NSPoint mouseLocation = [NSEvent mouseLocation];
    
    // Convert to monitor-relative coordinates (no scaling needed)
    int relativeX = (int)(mouseLocation.x - g_selectedMonitor.x);
    
    if (relativeX >= 0 && relativeX < g_selectedMonitor.width) {
        return relativeX;
    }
    
    return 0;
}

int GetMouseY() {
    NSPoint mouseLocation = [NSEvent mouseLocation];
    
    // Convert to monitor-relative coordinates (flip Y coordinate)
    int relativeY = (int)(g_selectedMonitor.height - (mouseLocation.y - g_selectedMonitor.y));
    
    if (relativeY >= 0 && relativeY < g_selectedMonitor.height) {
        return relativeY;
    }
    
    return 0;
}

Vector2Platform GetMousePosition() { 
    NSPoint mouseLocation = [NSEvent mouseLocation];
    
    // Convert to monitor-relative coordinates
    float relativeX = mouseLocation.x - g_selectedMonitor.x;
    float relativeY = g_selectedMonitor.height - (mouseLocation.y - g_selectedMonitor.y);
    
    if (relativeX >= 0 && relativeX < g_selectedMonitor.width &&
        relativeY >= 0 && relativeY < g_selectedMonitor.height) {
        return {relativeX, relativeY};
    }
    
    return {0.0f, 0.0f};
}

bool SupportsDynamicWallpaper() {
    // macOS supports dynamic wallpapers, but our implementation is more limited
    return true;
}

bool SupportsMultiMonitor() {
    return true;
}

void ShowAlert(const std::string &title, const std::string &message) {
    @autoreleasepool {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:[NSString stringWithUTF8String:title.c_str()]];
        [alert setInformativeText:[NSString stringWithUTF8String:message.c_str()]];
        [alert addButtonWithTitle:@"OK"];
        [alert runModal];
    }
}

} // namespace DesktopIntegration

#endif // __APPLE__ 