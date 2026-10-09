#include "test_harness.h"
#include "PAL_Window.h"
#include "win.h"

HWND ApplicationHWnd = nullptr;
class Win32Mouse {
public:
    void lostFocus(BOOL);
    void addWin32Event(UINT, WPARAM, LPARAM, DWORD);
};
void Win32Mouse::lostFocus(BOOL) {}
void Win32Mouse::addWin32Event(UINT, WPARAM, LPARAM, DWORD) {}
Win32Mouse *TheWin32Mouse = nullptr;
void Platform_AddKeyEvent(unsigned char, bool) {}
void Platform_ResetKeys(void) {}

TEST(pal_window_lifecycle_and_info)
{
    // Query info before window creation
    PlatformWindowInfo *info = Platform_GetWindowInfo();
    CHECK(info != nullptr);

    // Create test window
    bool created = Platform_CreateWindow("Generals Port Test Window", 640, 480, true);
    CHECK(created);
    CHECK(Platform_IsWindowCreated());

    // Verify info updated
    info = Platform_GetWindowInfo();
    CHECK(info != nullptr);
    CHECK_EQ(info->width, 640);
    CHECK_EQ(info->height, 480);
    CHECK(info->isWindowed);

    // Pump events (should be clean with no events pending)
    Platform_PumpEvents();

    // Destroy window
    Platform_DestroyWindow();
    CHECK(!Platform_IsWindowCreated());
}
