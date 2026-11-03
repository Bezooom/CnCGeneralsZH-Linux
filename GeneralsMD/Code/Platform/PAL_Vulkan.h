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
// before the draw: position, two texture coordinate sets, one colour, and two projected texture
// coordinates the host computed for camera-space stages. 60 bytes, matching VKCanonicalVertex.
//
// The colour is four floats, not the packed D3DCOLOR the engine writes: a packed colour read as a
// float is a number near zero, and the picture it makes is a dark field with the occasional
// bright triangle.
const uint32_t PLATFORM_CANONICAL_VERTEX_STRIDE = 60;

// Push constants are the only per-draw state the shaders see. The matrix is stored column-major,
// which is the transpose of the row-major D3D9 matrix the engine hands over, because the vertex
// shader multiplies the matrix by the column vector. The y row is negated on the way in: Vulkan
// puts NDC y=+1 at the top of the image where Direct3D 9 puts it at the bottom, so without the
// flip the whole frame is mirrored vertically.
//
// The stage fields are the fixed-function texture stages, packed so the whole block stays inside
// the 128 bytes every Vulkan implementation guarantees: 124 bytes.
typedef struct PlatformVulkanPushConstants {
    float mvp[16];              // 0
    float materialColor[4];     // 64
    // Per stage: colourOp | colourArg1 << 8 | colourArg2 << 16 | uvSource << 24.
    float stageConfig[4];       // 80
    // Per stage: alphaOp | alphaArg1 << 8 | alphaArg2 << 16 | colourArg0 << 24.
    float stageAlphaConfig[4];  // 96
    float alphaReference;       // 112: D3DRS_ALPHAREF / 255
    float alphaConfig;          // 116: bit 0 enable, bits 8..15 the D3DCMP_* function
    float textureFactor;        // 120: D3DRS_TEXTUREFACTOR, packed D3DCOLOR
} PlatformVulkanPushConstants;

// uvSource values packed into stageConfig bits 24..25, matching the vertex shader's selector.
#define PLATFORM_UV_SOURCE_UV0 0u
#define PLATFORM_UV_SOURCE_UV1 1u
#define PLATFORM_UV_SOURCE_PROJECTED0 2u
#define PLATFORM_UV_SOURCE_PROJECTED1 3u

// Sampler state key: address modes and filters packed the way the shim's D3D9 enums arrive, plus
// a bit saying the texture's mip chain is complete enough to filter across.
#define PLATFORM_SAMPLER_MIPS_READY 0x8000u
uint32_t Platform_MakeSamplerKey(uint32_t addressU, uint32_t addressV, uint32_t minFilter,
                                 uint32_t magFilter, uint32_t mipFilter, int mipsReady);

bool Platform_InitVulkan(PlatformWindowInfo *windowInfo);
bool Platform_BeginVulkanFrame(uint32_t *imageIndex);
bool Platform_EndVulkanFrame(uint32_t imageIndex);
void Platform_ShutdownVulkan(void);
void Platform_SetVulkanClearColor(float r, float g, float b, float a);
void Platform_SetVulkanDepthState(int enable, int write, int d3dCompare);
void Platform_SetVulkanBlendState(int enable);
bool Platform_CreateVulkanPipeline(void);
void Platform_SetVulkanPushConstants(const PlatformVulkanPushConstants *pushConstants);
bool Platform_DrawVulkanCanonical(const float *canonicalVertices, uint32_t vertexCount,
                                  const uint16_t *indices, uint32_t indexCount);

// Textures. The image, view and sampler are created once per texture handle and updated in place:
// the fog path rewrites a few hundred texels per frame and used to pay a full destroy/recreate.
void *Platform_CreateVulkanTexture(uint32_t width, uint32_t height, const void *pixels, uint32_t format);
// Same, with an explicit mip count; levels are uploaded with Platform_UpdateVulkanTextureLevel.
void *Platform_CreateVulkanTextureEx(uint32_t width, uint32_t height, uint32_t levels,
                                     const void *pixels, uint32_t format);
// Updates a rectangle of level 0 in place. The pixels must already be in the image format (the
// shim converts odd D3D formats itself). Never waits on the GPU.
bool Platform_UpdateVulkanTexture(void *texture, int x, int y, int w, int h, const void *pixels);
bool Platform_UpdateVulkanTextureLevel(void *texture, uint32_t level, int x, int y, int w, int h,
                                       const void *pixels);
// Binds the four texture stages in one call. A NULL texture, or a stage with no texture, samples
// the white dummy, which is what the fixed-function pipeline did with the stage off. samplerKeys
// is PLATFORM_SAMPLER_MIPS_READY plus the packed state for each stage.
void Platform_BindVulkanTextures(void *const textures[4], const uint32_t *samplerKeys);
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
