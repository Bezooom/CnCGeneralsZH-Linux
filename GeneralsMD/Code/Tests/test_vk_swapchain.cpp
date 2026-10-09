#include "test_harness.h"
#include "PAL_Window.h"
#include "PAL_Vulkan.h"
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
TEST(vulkan_swapchain_and_presentation_lifecycle)
{
    // If running in a truly headless environment without X11 DISPLAY, test skips gracefully
    const char *display = getenv("DISPLAY");
    if (!display || display[0] == '\0') {
        return;
    }

    // 1. Create native platform window
    bool windowCreated = Platform_CreateWindow("Vulkan Presentation Test Window", 320, 240, true);
    CHECK(windowCreated);
    CHECK(Platform_IsWindowCreated());

    PlatformWindowInfo *winInfo = Platform_GetWindowInfo();
    CHECK(winInfo != nullptr);
    CHECK(winInfo->x11Display != nullptr);
    CHECK(winInfo->x11Window != 0);

    // 2. Initialize Vulkan presentation subsystem
    bool vkInitted = Platform_InitVulkan(winInfo);
    CHECK(vkInitted);

    PlatformVulkanContext *vkCtx = Platform_GetVulkanContext();
    CHECK(vkCtx != nullptr);
    CHECK(vkCtx->isInitialized);
    CHECK(vkCtx->instance != nullptr);
    CHECK(vkCtx->device != nullptr);
    CHECK(vkCtx->swapchain != nullptr);
    CHECK(vkCtx->imageCount >= 2);

    // 3. Render and present 3 test frames
    for (int frame = 0; frame < 3; ++frame) {
        uint32_t imageIndex = 0;
        bool frameBegun = Platform_BeginVulkanFrame(&imageIndex);
        CHECK(frameBegun);

        bool frameEnded = Platform_EndVulkanFrame(imageIndex);
        CHECK(frameEnded);

        // Process any window events
        Platform_PumpEvents();
    }

    // 4. Shutdown Vulkan
    Platform_ShutdownVulkan();
    CHECK(!vkCtx->isInitialized);

    // 5. Destroy Window
    Platform_DestroyWindow();
    CHECK(!Platform_IsWindowCreated());
}
