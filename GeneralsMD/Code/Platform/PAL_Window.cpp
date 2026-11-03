#include "PAL_Window.h"
#include "PAL_Icon.h"
extern void Platform_AddKeyEvent(unsigned char scanCode, bool isDown);
extern void Platform_ResetKeys(void);
#define Drawable X11_Drawable
#define Status X11_Status
#define None X11_None
#define Bool X11_Bool
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#undef Drawable
#undef Status
#undef None
#undef Bool
#include "win.h"
#include "GameClient/KeyDefs.h"

#include "Win32Device/GameClient/Win32Mouse.h"
extern Win32Mouse *TheWin32Mouse;
#include <cstdlib>
#include <cstdio>

static PlatformWindowInfo s_info = {
    nullptr, 0, 800, 600, 800, 600, true, true, false
};

static Atom s_wmDeleteMessage = 0;

static unsigned char X11KeyToScanCode(KeySym keysym, unsigned int keycode)
{
    switch (keysym) {
        case XK_Escape: return KEY_ESC;
        case XK_Return: return KEY_ENTER;
        case XK_BackSpace: return KEY_BACKSPACE;
        case XK_Tab: return KEY_TAB;
        case XK_space: return KEY_SPACE;
        case XK_Left: return KEY_LEFT;
        case XK_Right: return KEY_RIGHT;
        case XK_Up: return KEY_UP;
        case XK_Down: return KEY_DOWN;
        case XK_Home: return KEY_HOME;
        case XK_End: return KEY_END;
        case XK_Page_Up: return KEY_PGUP;
        case XK_Page_Down: return KEY_PGDN;
        case XK_Insert: return KEY_INS;
        case XK_Delete: return KEY_DEL;
        case XK_F1: return KEY_F1;
        case XK_F2: return KEY_F2;
        case XK_F3: return KEY_F3;
        case XK_F4: return KEY_F4;
        case XK_F5: return KEY_F5;
        case XK_F6: return KEY_F6;
        case XK_F7: return KEY_F7;
        case XK_F8: return KEY_F8;
        case XK_F9: return KEY_F9;
        case XK_F10: return KEY_F10;
        case XK_F11: return KEY_F11;
        case XK_F12: return KEY_F12;
        case XK_Shift_L: return KEY_LSHIFT;
        case XK_Shift_R: return KEY_RSHIFT;
        case XK_Control_L: return KEY_LCTRL;
        case XK_Control_R: return KEY_RCTRL;
        case XK_Alt_L: return KEY_LALT;
        case XK_Alt_R: return KEY_RALT;
        case XK_Caps_Lock: return KEY_CAPS;
        default:
            if (keycode >= 8 && keycode <= 255) {
                return (unsigned char)(keycode - 8);
            }
            return KEY_NONE;
    }
}

bool Platform_CreateWindow(const char *title, int width, int height, bool windowed)
{
    if (s_info.x11Display != nullptr) {
        return true;
    }

    const char *displayName = getenv("DISPLAY");
    if (!displayName || displayName[0] == '\0') {
        // Headless environment
        s_info.width = width;
        s_info.height = height;
        s_info.isWindowed = windowed;
        s_info.isActive = true;
        s_info.shouldQuit = false;
        return true;
    }

    Display *display = XOpenDisplay(displayName);
    if (!display) {
        return false;
    }

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);

    unsigned long black = BlackPixel(display, screen);
    unsigned long white = WhitePixel(display, screen);

    Window window = XCreateSimpleWindow(
        display, root,
        0, 0, width, height, 0,
        black, black
    );

    if (!window) {
        XCloseDisplay(display);
        return false;
    }

    XStoreName(display, window, title ? title : "Command and Conquer Generals");
    XClassHint classHint;
    char resName[] = "generals";
    char className[] = "generals";
    classHint.res_name = resName;
    classHint.res_class = className;
    XSetClassHint(display, window, &classHint);
    Atom netWmIcon = XInternAtom(display, "_NET_WM_ICON", False);
    XChangeProperty(display, window, netWmIcon, XA_CARDINAL, 32, PropModeReplace,
                    (const unsigned char *)kWindowIconData, kWindowIconDataCount);


    s_wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &s_wmDeleteMessage, 1);

    long eventMask = ExposureMask | KeyPressMask | KeyReleaseMask |
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                     FocusChangeMask | StructureNotifyMask;
    XSelectInput(display, window, eventMask);

    if (!windowed) {
        Atom wmState = XInternAtom(display, "_NET_WM_STATE", False);
        Atom wmFullscreen = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
        XChangeProperty(display, window, wmState, XA_ATOM, 32, PropModeReplace,
                        (unsigned char *)&wmFullscreen, 1);
    }

    XMapWindow(display, window);
    // Raise it above whatever the launcher left on screen. A window the compositor considers
    // occluded is not composited at all, and a present that waits for the compositor (every
    // blocking present mode does) then stalls until the driver times out.
    XRaiseWindow(display, window);
    XFlush(display);

    s_info.x11Display = display;
    s_info.x11Window = window;
    s_info.width = width;
    s_info.height = height;
    s_info.gameWidth = width;
    s_info.gameHeight = height;
    s_info.isWindowed = windowed;
    s_info.isActive = true;
    s_info.shouldQuit = false;


    if (!windowed) {
        Platform_SetFullscreen(true);
    }
    ApplicationHWnd = (HWND)(uintptr_t)window;

    return true;
}

void Platform_PumpEvents(void)
{
    if (!s_info.x11Display) {
        return;
    }

    Display *display = (Display *)s_info.x11Display;
    while (XPending(display)) {
        XEvent ev;
        XNextEvent(display, &ev);

        switch (ev.type) {
            case ClientMessage:
                if ((Atom)ev.xclient.data.l[0] == s_wmDeleteMessage) {
                    s_info.shouldQuit = true;
                    PostQuitMessage(0);
                }
                break;

            case ConfigureNotify:
                s_info.width = ev.xconfigure.width;
                s_info.height = ev.xconfigure.height;
                break;

            case FocusIn:
                s_info.isActive = true;
                if (TheWin32Mouse) {
                    TheWin32Mouse->lostFocus(FALSE);
                }
                break;

            case FocusOut:
                s_info.isActive = false;
                if (TheWin32Mouse) {
                    TheWin32Mouse->lostFocus(TRUE);
                }
                Platform_ResetKeys();
                break;

            case MotionNotify: {
                int x = ev.xmotion.x;
                int y = ev.xmotion.y;
                if (s_info.gameWidth > 0 && s_info.width > 0 && s_info.width != s_info.gameWidth) {
                    x = (int)((long long)x * s_info.gameWidth / s_info.width);
                }
                if (s_info.gameHeight > 0 && s_info.height > 0 && s_info.height != s_info.gameHeight) {
                    y = (int)((long long)y * s_info.gameHeight / s_info.height);
                }
                LPARAM lParam = (LPARAM)(((y & 0xFFFF) << 16) | (x & 0xFFFF));
                DWORD time = (DWORD)ev.xmotion.time;
                if (TheWin32Mouse) {
                    TheWin32Mouse->addWin32Event(WM_MOUSEMOVE, 0, lParam, time);
                }
                break;
            }

            case ButtonPress:
            case ButtonRelease: {
                bool isDown = (ev.type == ButtonPress);
                int x = ev.xbutton.x;
                int y = ev.xbutton.y;
                if (s_info.gameWidth > 0 && s_info.width > 0 && s_info.width != s_info.gameWidth) {
                    x = (int)((long long)x * s_info.gameWidth / s_info.width);
                }
                if (s_info.gameHeight > 0 && s_info.height > 0 && s_info.height != s_info.gameHeight) {
                    y = (int)((long long)y * s_info.gameHeight / s_info.height);
                }
                LPARAM lParam = (LPARAM)(((y & 0xFFFF) << 16) | (x & 0xFFFF));
                DWORD time = (DWORD)ev.xbutton.time;

                UINT msg = 0;
                WPARAM wParam = 0;

                switch (ev.xbutton.button) {
                    case 1: // Left
                        msg = isDown ? WM_LBUTTONDOWN : WM_LBUTTONUP;
                        break;
                    case 2: // Middle
                        msg = isDown ? WM_MBUTTONDOWN : WM_MBUTTONUP;
                        break;
                    case 3: // Right
                        msg = isDown ? WM_RBUTTONDOWN : WM_RBUTTONUP;
                        break;
                    case 4: // Wheel Up
                        if (isDown) {
                            msg = WM_MOUSEWHEEL;
                            wParam = (WPARAM)(120 << 16);
                        }
                        break;
                    case 5: // Wheel Down
                        if (isDown) {
                            msg = WM_MOUSEWHEEL;
                            wParam = (WPARAM)((-120) << 16);
                        }
                        break;
                    default:
                        break;
                }

                if (msg != 0 && TheWin32Mouse) {
                    TheWin32Mouse->addWin32Event(msg, wParam, lParam, time);
                }
                break;
            }

            case KeyPress:
            case KeyRelease: {
                bool isDown = (ev.type == KeyPress);
                KeySym keysym = XLookupKeysym(&ev.xkey, 0);
                unsigned char scanCode = X11KeyToScanCode(keysym, ev.xkey.keycode);
                if (scanCode != KEY_NONE) {
                    Platform_AddKeyEvent(scanCode, isDown);
                }
                break;
            }

            default:
                break;
        }
    }
}

void Platform_DestroyWindow(void)
{
    if (s_info.x11Display) {
        Display *display = (Display *)s_info.x11Display;
        if (s_info.x11Window) {
            XDestroyWindow(display, (Window)s_info.x11Window);
            s_info.x11Window = 0;
        }
        XCloseDisplay(display);
        s_info.x11Display = nullptr;
    }
    s_info.isActive = false;
    s_info.shouldQuit = false;
    ApplicationHWnd = nullptr;
}

PlatformWindowInfo *Platform_GetWindowInfo(void)
{
    return &s_info;
}

bool Platform_IsWindowCreated(void)
{
    return s_info.x11Display != nullptr || s_info.isActive;
}

void Platform_SetFullscreen(bool fullscreen)
{
    if (!s_info.x11Display || !s_info.x11Window) return;
    Display *display = (Display *)s_info.x11Display;
    Window window = (Window)s_info.x11Window;
    Window root = RootWindow(display, DefaultScreen(display));

    Atom wmState = XInternAtom(display, "_NET_WM_STATE", False);
    Atom wmFullscreen = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);

    XEvent xev = {};
    xev.type = ClientMessage;
    xev.xclient.window = window;
    xev.xclient.message_type = wmState;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = fullscreen ? 1 : 0; // 1 = _NET_WM_STATE_ADD, 0 = _NET_WM_STATE_REMOVE
    xev.xclient.data.l[1] = wmFullscreen;
    xev.xclient.data.l[2] = 0;
    xev.xclient.data.l[3] = 1;

    XSendEvent(display, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    XFlush(display);
    s_info.isWindowed = !fullscreen;
}

void Platform_ResizeWindow(int width, int height)
{
    if (!s_info.x11Display || !s_info.x11Window || width <= 0 || height <= 0) return;
    Display *display = (Display *)s_info.x11Display;
    Window window = (Window)s_info.x11Window;
    XResizeWindow(display, window, (unsigned int)width, (unsigned int)height);
    XFlush(display);
    s_info.width = width;
    s_info.height = height;
    s_info.gameWidth = width;
    s_info.gameHeight = height;
}

void Platform_SetGameResolution(int width, int height)
{
    if (width > 0) s_info.gameWidth = width;
    if (height > 0) s_info.gameHeight = height;
}
