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
void Platform_SetVulkanClearColor(float r, float g, float b, float a);
bool Platform_CreateVulkanPipeline(void);
bool Platform_DrawVulkanIndexed(const void *vertices, uint32_t vertexCount, uint32_t vertexStride,
                               const uint16_t *indices, uint32_t indexCount);
bool Platform_DrawVulkanVertices(const void *vertices, uint32_t vertexCount, uint32_t vertexStride);
void *Platform_CreateVulkanTexture(uint32_t width, uint32_t height, const void *pixels, uint32_t format);
void Platform_BindVulkanTexture(void *texture);
void Platform_DestroyVulkanTexture(void *texture);
PlatformVulkanContext *Platform_GetVulkanContext(void);

#ifdef __cplusplus
}
#endif

#endif // PAL_VULKAN_H
