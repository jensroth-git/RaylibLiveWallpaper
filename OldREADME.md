
# Raylib Live Wallpaper
![Logo](images/video.gif)

This library allows users to create dynamic wallpapers on Windows using Raylib by using an undocumented Windows API.

### Note

This is just a preview, more work is needed to
- provide input method replacements for the keyboard

## Features

- Supports Windows 11 24H2 and prior!
- Use familiar Raylib drawing methods to create a Live Wallpaper on the Windows desktop
- Provides mouse input replacements for interactive desktops
- Supports Multi Monitor Setups and is DPI aware.
- Doesn't Render if Wallpaper or Monitor is occluded

## Getting Started

### Prerequisites

Ensure you have Raylib installed. You can download it from [Raylib's official site](https://www.raylib.com/), or use the [Nuget package](https://www.nuget.org/packages/raylib/).

### Installation

Include `RaylibDesktop.h` in your project and look at the provided example code.

### Example Usage

```cpp
#include "RaylibDesktop.h"
#include "raylib.h"

int main()
{
    // Initializes desktop replacement magic
    InitRaylibDesktop();

    // Sets up the desktop (-1 is the entire desktop spanning all monitors)
    MonitorInfo monitorInfo = GetWallpaperTarget(-1);

    // Initialize the raylib window.
    InitWindow(monitorInfo.monitorWidth, monitorInfo.monitorHeight, "Raylib Desktop Demo");

    // Retrieve the handle for the raylib-created window.
    void* raylibWindowHandle = GetWindowHandle();

    // Reparent the raylib window to the window behind the desktop icons.
    RaylibDesktopReparentWindow(raylibWindowHandle);

    // Configure the desktop positioning.
    ConfigureDesktopPositioning(monitorInfo);

    // Now, enter the raylib render loop.
    SetTargetFPS(60);

    // Main render loop.
    while (!WindowShouldClose())
    {
        // Skip rendering if the monitor is occluded more than 95%
        if (IsMonitorOccluded(monitorInfo, 0.95))
        {
            WaitTime(0.1);
            continue;
        }

        // Update Custom Mouse Input Replacements
        RaylibDesktopUpdateMouseState();

        BeginDrawing();
        // Normal Drawing Code...
        EndDrawing();
    }

    // Close the window and unload resources.
    CloseWindow();

    // Clean up the desktop window.
    CleanupRaylibDesktop();

    return 0;
}
```

## Notes

- To hide the console window when deploying set the SubSystem to `/SUBSYSTEM\:WINDOWS`, and to avoid having to include `windows.h` also set the entry point back to `mainCRTStartup`
- The wallpaper window becomes a child of a desktop window created using an undocumented windows feature.

### Mouse Input Functions

Since the reparented Raylib window does not receive input normally, the following replacement functions are provided:

```cpp
// Call this function once per frame to update mouse states.
// It updates both the previous and current state arrays.
void RaylibDesktopUpdateMouseState(void);

// Mouse button state functions.
// The valid button indices are:
//   0: Left button (VK_LBUTTON)
//   1: Right button (VK_RBUTTON)
//   2: Middle button (VK_MBUTTON)
//   3: XButton1   (VK_XBUTTON1)
//   4: XButton2   (VK_XBUTTON2)
bool RaylibDesktopIsMouseButtonPressed(int button);  // Returns true only on the frame the button was pressed.
bool RaylibDesktopIsMouseButtonDown(int button);     // Returns true if the button is currently down.
bool RaylibDesktopIsMouseButtonReleased(int button); // Returns true only on the frame the button was released.
bool RaylibDesktopIsMouseButtonUp(int button);       // Returns true if the button is currently up.

// Mouse position functions.
// These return the global cursor position (in physical pixels).
int RaylibDesktopGetMouseX(void);
int RaylibDesktopGetMouseY(void);
Vector2 RaylibDesktopGetMousePosition(void);
```

Currently, there are no replacements for keyboard input, which may be added in the future.

## License

This project is licensed under the MIT License.

