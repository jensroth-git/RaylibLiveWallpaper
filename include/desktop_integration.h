#pragma once
#include <string>
#include <vector>

// Cross-platform desktop integration interface

// Structure to hold information about a monitor
struct MonitorInfo
{
	int x; // X coordinate of the monitor's top-left corner
	int y; // Y coordinate of the monitor's top-left corner
	int width; // Monitor width in pixels
	int height; // Monitor height in pixels
};

// Vector2 structure to avoid raylib dependency in this header
struct Vector2Platform
{
	float x;
	float y;
};

namespace DesktopIntegration
{
	// Initialize desktop integration
	bool Initialize();

	// Cleanup desktop integration
	void Cleanup();

	// Monitor management
	std::vector<MonitorInfo> EnumerateMonitors();
	MonitorInfo GetWallpaperTarget(int monitorIndex = -1); // -1 for entire desktop

	// Configure the application window for wallpaper mode
	void ConfigureWallpaperWindow(void *windowHandle, const MonitorInfo &monitor);

	// Check if monitor is occluded
	bool IsMonitorOccluded(const MonitorInfo &monitor, double threshold = 0.95);

	// Check if desktop is occluded by Lock/Secure screen (windows only for now)
	bool IsDesktopLocked();

	// Mouse handling (since raylib's mouse doesn't work with wallpaper windows)
	void UpdateMouseState();
	bool IsMouseButtonPressed(int button); // 0=left, 1=right, 2=middle
	bool IsMouseButtonDown(int button);
	bool IsMouseButtonReleased(int button);
	bool IsMouseButtonUp(int button);
	int GetMouseX();
	int GetMouseY();
	Vector2Platform GetMousePosition();

	// Platform-specific features
	bool SupportsDynamicWallpaper();
	bool SupportsMultiMonitor();

	void ShowAlert(const std::string &title, const std::string &message);
} // namespace DesktopIntegration
