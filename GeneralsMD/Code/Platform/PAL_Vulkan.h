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

// The pipeline reads one vertex format, so every D3D9 flexible vertex format is decoded into it
// before the draw. Position, one texture coordinate set and one colour, in that order.
//
// The colour is four floats, not the packed D3DCOLOR the engine writes: a packed colour read as a
// float is a number near zero, and the picture it makes is a dark field with the occasional
// bright triangle.
const uint32_t PLATFORM_CANONICAL_VERTEX_STRIDE = 36;

// Push constants are the only per-draw state the shaders see. The matrix is stored column-major,
// which is the transpose of the row-major D3D9 matrix the engine hands over, because the vertex
// shader multiplies the matrix by the column vector. The y row is negated on the way in: Vulkan
// puts NDC y=+1 at the top of the image where Direct3D 9 puts it at the bottom, so without the
// flip the whole frame is mirrored vertically.
typedef struct PlatformVulkanPushConstants {
    float mvp[16];
    float materialColor[4];
} PlatformVulkanPushConstants;

bool Platform_InitVulkan(PlatformWindowInfo *windowInfo);
bool Platform_BeginVulkanFrame(uint32_t *imageIndex);
bool Platform_EndVulkanFrame(uint32_t imageIndex);
void Platform_ShutdownVulkan(void);
void Platform_SetVulkanClearColor(float r, float g, float b, float a);
bool Platform_CreateVulkanPipeline(void);
void Platform_SetVulkanPushConstants(const PlatformVulkanPushConstants *pushConstants);
bool Platform_DrawVulkanCanonical(const float *canonicalVertices, uint32_t vertexCount,
                                  const uint16_t *indices, uint32_t indexCount);
void *Platform_CreateVulkanTexture(uint32_t width, uint32_t height, const void *pixels, uint32_t format);
void Platform_BindVulkanTexture(void *texture);
void Platform_DestroyVulkanTexture(void *texture);
PlatformVulkanContext *Platform_GetVulkanContext(void);

// The embedded SPIR-V, exposed so a test can build its own offscreen pipeline around it and check
// what the shaders actually do with a matrix, a colour and a texture. The names drop the hyphen
// because an identifier cannot contain one.
const uint32_t *Platform_GetVertexShaderSPIRV(uint32_t *wordCount);
const uint32_t *Platform_GetFragmentShaderSPIRV(uint32_t *wordCount);

#ifdef __cplusplus
}
#endif

#endif // PAL_VULKAN_H
