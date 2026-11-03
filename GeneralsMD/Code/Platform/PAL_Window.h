#ifndef PAL_WINDOW_H
#define PAL_WINDOW_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PlatformWindowInfo {
    void *x11Display;          // Display*
    unsigned long x11Window;   // Window
    int width;
    int height;
    int gameWidth;
    int gameHeight;
    bool isWindowed;
    bool isActive;
    bool shouldQuit;
} PlatformWindowInfo;

bool Platform_CreateWindow(const char *title, int width, int height, bool windowed);
void Platform_PumpEvents(void);
void Platform_DestroyWindow(void);
PlatformWindowInfo *Platform_GetWindowInfo(void);
bool Platform_IsWindowCreated(void);
void Platform_SetFullscreen(bool fullscreen);
void Platform_ResizeWindow(int width, int height);
void Platform_SetGameResolution(int width, int height);

#ifdef __cplusplus
}
#endif

#endif // PAL_WINDOW_H
