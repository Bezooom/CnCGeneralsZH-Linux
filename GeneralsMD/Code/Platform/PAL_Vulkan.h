#ifndef PAL_VULKAN_H
#define PAL_VULKAN_H

#include <stdbool.h>
#include <stdint.h>
#include "PAL_Window.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PlatformVulkanContext {
    void *instance;             // VkInstance
    void *physicalDevice;       // VkPhysicalDevice
    void *device;               // VkDevice
    void *surface;              // VkSurfaceKHR
    void *swapchain;            // VkSwapchainKHR
    uint32_t imageCount;
    uint32_t currentImageIndex;
    int width;
    int height;
    bool isInitialized;
} PlatformVulkanContext;

bool Platform_InitVulkan(PlatformWindowInfo *windowInfo);
bool Platform_BeginVulkanFrame(uint32_t *imageIndex);
bool Platform_EndVulkanFrame(uint32_t imageIndex);
void Platform_ShutdownVulkan(void);
PlatformVulkanContext *Platform_GetVulkanContext(void);

#ifdef __cplusplus
}
#endif

#endif // PAL_VULKAN_H
