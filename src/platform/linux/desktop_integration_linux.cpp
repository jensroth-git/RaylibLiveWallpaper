#ifdef __linux__

#include "../desktop_integration.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include <algorithm>
#include <climits>
#include <cstdio>
#include <dlfcn.h>
#include <string>
#include <unistd.h>
#include <vector>

#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

// Simple point struct for cursor position
struct POINT
{
	int x;
	int y;
};

static Display *g_display = nullptr;
static Window g_rootWindow = 0;
static Window g_raylibWindow = 0;
static int g_desktopX = 0;
static int g_desktopY = 0;
static MonitorInfo g_selectedMonitor = {0, 0, 0, 0};
static bool g_currentMouseState[5] = {false};
static bool g_previousMouseState[5] = {false};

namespace DesktopIntegration
{
	bool Initialize()
	{
		g_display = XOpenDisplay(nullptr);
		if (!g_display) {
			fprintf(stderr, "[DesktopIntegration] Failed to open X display\n");
			return false;
		}
		g_rootWindow = DefaultRootWindow(g_display);

		// Enable synchronous X11 error reporting for debugging
		XSynchronize(g_display, True);

		return true;
	}

	void Cleanup()
	{
		if (g_raylibWindow) {
			XDestroyWindow(g_display, g_raylibWindow);
			g_raylibWindow = 0;
		}
		if (g_display) {
			XCloseDisplay(g_display);
			g_display = nullptr;
		}
	}

	std::vector<MonitorInfo> EnumerateMonitors()
	{
		std::vector<MonitorInfo> monitors;
		XRRScreenResources *res = XRRGetScreenResourcesCurrent(g_display, g_rootWindow);
		if (!res)
			return monitors;

		for (int i = 0; i < res->ncrtc; ++i) {
			XRRCrtcInfo *ci = XRRGetCrtcInfo(g_display, res, res->crtcs[i]);
			if (!ci)
				continue;
			if (ci->mode == None) {
				XRRFreeCrtcInfo(ci);
				continue;
			}

			MonitorInfo m;
			m.x = ci->x;
			m.y = ci->y;
			m.width = ci->width;
			m.height = ci->height;
			monitors.push_back(m);
			XRRFreeCrtcInfo(ci);
		}
		XRRFreeScreenResources(res);

		// Normalize so top-left of virtual desktop is (0,0)
		g_desktopX = INT_MAX;
		g_desktopY = INT_MAX;
		for (auto &m : monitors) {
			g_desktopX = std::min(g_desktopX, m.x);
			g_desktopY = std::min(g_desktopY, m.y);
		}
		for (auto &m : monitors) {
			m.x -= g_desktopX;
			m.y -= g_desktopY;
		}

		return monitors;
	}

	MonitorInfo GetWallpaperTarget(int monitorIndex)
	{
		auto monitors = EnumerateMonitors();
		if (monitorIndex < 0 || monitorIndex >= static_cast<int>(monitors.size())) {
			MonitorInfo info;
			info.x = 0;
			info.y = 0;
			info.width = DisplayWidth(g_display, DefaultScreen(g_display));
			info.height = DisplayHeight(g_display, DefaultScreen(g_display));
			return info;
		}
		return monitors[monitorIndex];
	}

	void ConfigureWallpaperWindow(void *windowHandle, const MonitorInfo &monitor)
	{
		// Get the X11 Window from the handle with validation
		g_display = glfwGetX11Display();
		GLFWwindow *glfwWin = (GLFWwindow *)windowHandle; // the *real* GLFW pointer
		g_raylibWindow = glfwGetX11Window(glfwWin);

		if (!g_raylibWindow) {
			fprintf(stderr, "[DesktopIntegration] Could not obtain valid X11 window from handle\n");
			return;
		}

		// Wait for the window to be fully mapped (up to 1 second)
		bool windowReady = false;
		for (int attempts = 0; attempts < 100; attempts++) {
			XWindowAttributes attrs;
			if (XGetWindowAttributes(g_display, g_raylibWindow, &attrs) != 0) {
				if (attrs.map_state == IsViewable) {
					windowReady = true;
					break;
				}
			}
			usleep(10000); // Wait 10ms
		}

		if (!windowReady) {
			fprintf(stderr, "[DesktopIntegration] Warning: Window may not be fully ready, proceeding anyway\n");
		}

		g_selectedMonitor = monitor;

		// Force synchronization to ensure we catch all errors
		XSync(g_display, False);

		// Tell the window manager to ignore this window
		XSetWindowAttributes attrs;
		attrs.override_redirect = True;

		// Add error handling for XChangeWindowAttributes
		XErrorHandler oldHandler = XSetErrorHandler(
			[](Display *display, XErrorEvent *event) -> int
			{
				char errorText[256];
				XGetErrorText(display, event->error_code, errorText, sizeof(errorText));
				fprintf(
					stderr,
					"[DesktopIntegration] X Error in XChangeWindowAttributes: %s (Window: 0x%lx)\n",
					errorText,
					event->resourceid
				);
				return 0;
			}
		);

		int result = XChangeWindowAttributes(g_display, g_raylibWindow, CWOverrideRedirect, &attrs);
		XSetErrorHandler(oldHandler);

		if (result == 0) {
			fprintf(stderr, "[DesktopIntegration] Failed to set override_redirect attribute\n");
			return;
		}

		// Set _NET_WM_WINDOW_TYPE_DESKTOP hint
		Atom type = XInternAtom(g_display, "_NET_WM_WINDOW_TYPE", False);
		Atom desktopAtom = XInternAtom(g_display, "_NET_WM_WINDOW_TYPE_DESKTOP", False);

		if (type != None && desktopAtom != None) {
			XChangeProperty(
				g_display,
				g_raylibWindow,
				type,
				XA_ATOM,
				32,
				PropModeReplace,
				reinterpret_cast<unsigned char *>(&desktopAtom),
				1
			);
		}
		else {
			fprintf(stderr, "[DesktopIntegration] Warning: Could not set window type atoms\n");
		}

		// Reparent and position with error checking
		fprintf(
			stderr,
			"[DesktopIntegration] Reparenting window to root, position: %d,%d size: %dx%d\n",
			monitor.x,
			monitor.y,
			monitor.width,
			monitor.height
		);

		XReparentWindow(g_display, g_raylibWindow, g_rootWindow, monitor.x, monitor.y);
		XMoveResizeWindow(g_display, g_raylibWindow, monitor.x, monitor.y, monitor.width, monitor.height);
		XMapWindow(g_display, g_raylibWindow);

		// Force synchronization to catch any errors
		XSync(g_display, False);

		fprintf(stderr, "[DesktopIntegration] Window configuration completed successfully\n");
	}

	bool IsMonitorOccluded(const MonitorInfo &monitor, double threshold)
	{
		Window root_ret, parent_ret;
		Window *children = nullptr;
		unsigned int nchildren = 0;

		if (!XQueryTree(g_display, g_rootWindow, &root_ret, &parent_ret, &children, &nchildren)) {
			return false;
		}

		long occludedPixels = 0;
		long totalPixels = static_cast<long>(monitor.width) * monitor.height;
		bool hasFullscreen = false;

		// Iterate windows top-down (last in children[] is top)
		for (int i = nchildren - 1; i >= 0; --i) {
			Window w = children[i];
			if (w == g_raylibWindow)
				continue;

			XWindowAttributes attr;
			if (!XGetWindowAttributes(g_display, w, &attr))
				continue;
			if (attr.map_state != IsViewable)
				continue;

			// Get window position in root coords
			int wx, wy;
			Window child;
			XTranslateCoordinates(g_display, w, g_rootWindow, 0, 0, &wx, &wy, &child);

			int left = wx - g_desktopX;
			int top = wy - g_desktopY;
			int right = left + attr.width;
			int bottom = top + attr.height;

			int mx1 = monitor.x;
			int my1 = monitor.y;
			int mx2 = monitor.x + monitor.width;
			int my2 = monitor.y + monitor.height;

			int ix1 = std::max(left, mx1);
			int iy1 = std::max(top, my1);
			int ix2 = std::min(right, mx2);
			int iy2 = std::min(bottom, my2);
			if (ix2 > ix1 && iy2 > iy1) {
				long area = static_cast<long>(ix2 - ix1) * (iy2 - iy1);
				occludedPixels += area;
				double coverage = area / static_cast<double>(totalPixels);
				if (coverage > 0.9) {
					hasFullscreen = true;
					break;
				}
			}
		}
		if (children)
			XFree(children);

		if (hasFullscreen)
			return true;
		double fraction = occludedPixels / static_cast<double>(totalPixels);
		return fraction >= threshold;
	}

	bool IsDesktopLocked() {
		// TODO: Implement desktop locking detection for Linux
		return false;
	}

	void UpdateMouseState()
	{
		for (int i = 0; i < 5; ++i) {
			g_previousMouseState[i] = g_currentMouseState[i];
		}
		Window ret_win;
		int rx, ry;
		unsigned int mask;
		XQueryPointer(g_display, g_rootWindow, &ret_win, &ret_win, &rx, &ry, &rx, &ry, &mask);

		g_currentMouseState[0] = mask & Button1Mask;
		g_currentMouseState[1] = mask & Button3Mask;
		g_currentMouseState[2] = mask & Button2Mask;
		g_currentMouseState[3] = mask & Button4Mask;
		g_currentMouseState[4] = mask & Button5Mask;
	}

	bool IsMouseButtonPressed(int button)
	{
		if (button < 0 || button >= 5)
			return false;
		return g_currentMouseState[button] && !g_previousMouseState[button];
	}

	bool IsMouseButtonDown(int button)
	{
		if (button < 0 || button >= 5)
			return false;
		return g_currentMouseState[button];
	}

	bool IsMouseButtonReleased(int button)
	{
		if (button < 0 || button >= 5)
			return false;
		return !g_currentMouseState[button] && g_previousMouseState[button];
	}

	bool IsMouseButtonUp(int button)
	{
		if (button < 0 || button >= 5)
			return false;
		return !g_currentMouseState[button];
	}

	bool GetRelativeCursorPos(POINT *p)
	{
		Window ret_win;
		int rx, ry;
		unsigned int mask;
		if (!XQueryPointer(g_display, g_rootWindow, &ret_win, &ret_win, &rx, &ry, &rx, &ry, &mask)) {
			return false;
		}
		p->x = rx - g_desktopX - g_selectedMonitor.x;
		p->y = ry - g_desktopY - g_selectedMonitor.y;
		return true;
	}

	int GetMouseX()
	{
		POINT pt;
		return GetRelativeCursorPos(&pt) ? pt.x : 0;
	}

	int GetMouseY()
	{
		POINT pt;
		return GetRelativeCursorPos(&pt) ? pt.y : 0;
	}

	Vector2Platform GetMousePosition()
	{
		POINT pt;
		if (GetRelativeCursorPos(&pt)) {
			return {static_cast<float>(pt.x), static_cast<float>(pt.y)};
		}
		return {0.0f, 0.0f};
	}

	bool SupportsDynamicWallpaper()
	{
		return true;
	}

	bool SupportsMultiMonitor()
	{
		return true;
	}

	void ShowAlert(const std::string &title, const std::string &message)
	{
		std::fprintf(stderr, "%s: %s\n", title.c_str(), message.c_str());
	}

} // namespace DesktopIntegration

#endif // __linux__
