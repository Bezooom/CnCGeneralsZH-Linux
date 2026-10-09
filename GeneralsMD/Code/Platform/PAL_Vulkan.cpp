#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>
#include <X11/Xlib.h>

#include "PAL_Vulkan.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static PlatformVulkanContext s_ctx = {
    nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, false
};

static VkQueue s_graphicsQueue = VK_NULL_HANDLE;
static VkQueue s_presentQueue = VK_NULL_HANDLE;
static uint32_t s_graphicsQueueFamily = 0;
static uint32_t s_presentQueueFamily = 0;

static VkFormat s_swapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
static std::vector<VkImage> s_swapchainImages;
static std::vector<VkImageView> s_swapchainImageViews;
static std::vector<VkFramebuffer> s_framebuffers;

static VkRenderPass s_renderPass = VK_NULL_HANDLE;
static VkCommandPool s_commandPool = VK_NULL_HANDLE;
static std::vector<VkCommandBuffer> s_commandBuffers;

static VkSemaphore s_imageAvailableSemaphore = VK_NULL_HANDLE;
static VkSemaphore s_renderFinishedSemaphore = VK_NULL_HANDLE;
static VkFence s_inFlightFence = VK_NULL_HANDLE;
static VkClearValue s_clearColor = { {{ 0.0f, 0.0f, 0.0f, 1.0f }} };
struct PlatformVulkanTexture {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    VkSampler sampler;
    uint32_t width;
    uint32_t height;
};

static VkPipeline s_graphicsPipeline = VK_NULL_HANDLE;
static VkPipelineLayout s_pipelineLayout = VK_NULL_HANDLE;
static VkBuffer s_dynamicVB = VK_NULL_HANDLE;
static VkDeviceMemory s_dynamicVBMem = VK_NULL_HANDLE;
static void *s_mappedVB = nullptr;
static VkBuffer s_dynamicIB = VK_NULL_HANDLE;
static VkDeviceMemory s_dynamicIBMem = VK_NULL_HANDLE;
static void *s_mappedIB = nullptr;
static PlatformVulkanTexture *s_boundTexture = nullptr;

static uint32_t find_memory_type(VkPhysicalDevice phys_dev, uint32_t type_filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(phys_dev, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return UINT32_MAX;
}

static const uint32_t k_vert_spv[] = {
    0x07230203, 0x00010000, 0x00000000, 0x0000001b, 0x00000000,
    0x00020011, 0x00000001,
    0x0003000e, 0x00000000, 0x00000001,
    0x0009000f, 0x00000000, 0x00000012, 0x6e69616d, 0x00000000, 0x00000009, 0x0000000c, 0x0000000e, 0x0000000f,
    0x00030047, 0x00000007, 0x00000002,
    0x00050048, 0x00000007, 0x00000000, 0x0000000b, 0x00000000,
    0x00040047, 0x0000000c, 0x0000001e, 0x00000000,
    0x00040047, 0x0000000e, 0x0000001e, 0x00000002,
    0x00040047, 0x0000000f, 0x0000001e, 0x00000000,
    0x00020013, 0x00000001,
    0x00030021, 0x00000002, 0x00000001,
    0x00030016, 0x00000003, 0x00000020,
    0x00040015, 0x00000004, 0x00000020, 0x00000000,
    0x00040017, 0x00000005, 0x00000003, 0x00000003,
    0x00040017, 0x00000006, 0x00000003, 0x00000004,
    0x0003001e, 0x00000007, 0x00000006,
    0x00040020, 0x00000008, 0x00000003, 0x00000007,
    0x0004003b, 0x00000008, 0x00000009, 0x00000003,
    0x00040020, 0x0000000a, 0x00000003, 0x00000006,
    0x00040020, 0x0000000b, 0x00000001, 0x00000005,
    0x0004003b, 0x0000000b, 0x0000000c, 0x00000001,
    0x00040020, 0x0000000d, 0x00000001, 0x00000006,
    0x0004003b, 0x0000000d, 0x0000000e, 0x00000001,
    0x0004003b, 0x0000000a, 0x0000000f, 0x00000003,
    0x0004002b, 0x00000003, 0x00000010, 0x3f800000,
    0x0004002b, 0x00000004, 0x00000011, 0x00000000,
    0x00050036, 0x00000001, 0x00000012, 0x00000000, 0x00000002,
    0x000200f8, 0x00000013,
    0x0004003d, 0x00000005, 0x00000014, 0x0000000c,
    0x00050051, 0x00000003, 0x00000015, 0x00000014, 0x00000000,
    0x00050051, 0x00000003, 0x00000016, 0x00000014, 0x00000001,
    0x00050051, 0x00000003, 0x00000017, 0x00000014, 0x00000002,
    0x00070050, 0x00000006, 0x00000018, 0x00000015, 0x00000016, 0x00000017, 0x00000010,
    0x00050041, 0x0000000a, 0x00000019, 0x00000009, 0x00000011,
    0x0003003e, 0x00000019, 0x00000018,
    0x0004003d, 0x00000006, 0x0000001a, 0x0000000e,
    0x0003003e, 0x0000000f, 0x0000001a,
    0x000100fd,
    0x00010038
};

static const uint32_t k_frag_spv[] = {
    0x07230203, 0x00010000, 0x00000000, 0x0000000c, 0x00000000,
    0x00020011, 0x00000001,
    0x0003000e, 0x00000000, 0x00000001,
    0x0007000f, 0x00000004, 0x00000009, 0x6e69616d, 0x00000000, 0x00000006, 0x00000008,
    0x00030010, 0x00000009, 0x00000007,
    0x00040047, 0x00000006, 0x0000001e, 0x00000000,
    0x00040047, 0x00000008, 0x0000001e, 0x00000000,
    0x00020013, 0x00000001,
    0x00030021, 0x00000002, 0x00000001,
    0x00030016, 0x00000003, 0x00000020,
    0x00040017, 0x00000004, 0x00000003, 0x00000004,
    0x00040020, 0x00000005, 0x00000003, 0x00000004,
    0x0004003b, 0x00000005, 0x00000006, 0x00000003,
    0x00040020, 0x00000007, 0x00000001, 0x00000004,
    0x0004003b, 0x00000007, 0x00000008, 0x00000001,
    0x00050036, 0x00000001, 0x00000009, 0x00000000, 0x00000002,
    0x000200f8, 0x0000000a,
    0x0004003d, 0x00000004, 0x0000000b, 0x00000008,
    0x0003003e, 0x00000006, 0x0000000b,
    0x000100fd,
    0x00010038
};

PlatformVulkanContext *Platform_GetVulkanContext(void)
{
    return &s_ctx;
}

bool Platform_InitVulkan(PlatformWindowInfo *windowInfo)
{
    if (s_ctx.isInitialized) {
        return true;
    }
    if (!windowInfo || !windowInfo->x11Display || !windowInfo->x11Window) {
        return false;
    }
    // Prefer NVIDIA ICD directly if present to prevent Mesa lavapipe LLVM conflicts
    if (getenv("VK_ICD_FILENAMES") == nullptr && getenv("VK_DRIVER_FILES") == nullptr) {
        if (access("/usr/share/vulkan/icd.d/nvidia_icd.json", R_OK) == 0) {
            setenv("VK_ICD_FILENAMES", "/usr/share/vulkan/icd.d/nvidia_icd.json", 0);
        }
    }


    Display *display = (Display *)windowInfo->x11Display;
    Window window = (Window)windowInfo->x11Window;
    uint32_t width = windowInfo->width > 0 ? (uint32_t)windowInfo->width : 800;
    uint32_t height = windowInfo->height > 0 ? (uint32_t)windowInfo->height : 600;

    // 1. Instance
    const char *extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME
    };
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Command & Conquer Generals";
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo instInfo = {};
    instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instInfo.pApplicationInfo = &appInfo;
    instInfo.enabledExtensionCount = 2;
    instInfo.ppEnabledExtensionNames = extensions;

    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&instInfo, nullptr, &instance) != VK_SUCCESS) {
        return false;
    }
    s_ctx.instance = instance;

    // 2. Xlib Surface
    VkXlibSurfaceCreateInfoKHR surfInfo = {};
    surfInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    surfInfo.dpy = display;
    surfInfo.window = window;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (vkCreateXlibSurfaceKHR(instance, &surfInfo, nullptr, &surface) != VK_SUCCESS) {
        vkDestroyInstance(instance, nullptr);
        return false;
    }
    s_ctx.surface = surface;

    // 3. Physical Device
    uint32_t gpuCount = 0;
    vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr);
    if (gpuCount == 0) return false;
    std::vector<VkPhysicalDevice> gpus(gpuCount);
    vkEnumeratePhysicalDevices(instance, &gpuCount, gpus.data());

    VkPhysicalDevice selectedGpu = VK_NULL_HANDLE;
    for (auto gpu : gpus) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(gpu, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            selectedGpu = gpu;
            break;
        }
    }
    if (!selectedGpu) selectedGpu = gpus[0];
    s_ctx.physicalDevice = selectedGpu;

    // 4. Queue Families
    uint32_t qfCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(selectedGpu, &qfCount, nullptr);
    std::vector<VkQueueFamilyProperties> qfProps(qfCount);
    vkGetPhysicalDeviceQueueFamilyProperties(selectedGpu, &qfCount, qfProps.data());

    bool foundGraphics = false, foundPresent = false;
    for (uint32_t i = 0; i < qfCount; ++i) {
        if (qfProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            s_graphicsQueueFamily = i;
            foundGraphics = true;
        }
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(selectedGpu, i, surface, &presentSupport);
        if (presentSupport) {
            s_presentQueueFamily = i;
            foundPresent = true;
        }
        if (foundGraphics && foundPresent) break;
    }
    if (!foundGraphics || !foundPresent) return false;

    // 5. Logical Device
    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    VkDeviceQueueCreateInfo gqInfo = {};
    gqInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    gqInfo.queueFamilyIndex = s_graphicsQueueFamily;
    gqInfo.queueCount = 1;
    gqInfo.pQueuePriorities = &queuePriority;
    queueInfos.push_back(gqInfo);

    if (s_graphicsQueueFamily != s_presentQueueFamily) {
        VkDeviceQueueCreateInfo pqInfo = {};
        pqInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        pqInfo.queueFamilyIndex = s_presentQueueFamily;
        pqInfo.queueCount = 1;
        pqInfo.pQueuePriorities = &queuePriority;
        queueInfos.push_back(pqInfo);
    }

    const char *devExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo devInfo = {};
    devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    devInfo.queueCreateInfoCount = (uint32_t)queueInfos.size();
    devInfo.pQueueCreateInfos = queueInfos.data();
    devInfo.enabledExtensionCount = 1;
    devInfo.ppEnabledExtensionNames = devExtensions;

    VkDevice device = VK_NULL_HANDLE;
    if (vkCreateDevice(selectedGpu, &devInfo, nullptr, &device) != VK_SUCCESS) {
        return false;
    }
    s_ctx.device = device;
    vkGetDeviceQueue(device, s_graphicsQueueFamily, 0, &s_graphicsQueue);
    vkGetDeviceQueue(device, s_presentQueueFamily, 0, &s_presentQueue);

    // 6. Surface Formats & Capabilities
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(selectedGpu, surface, &caps);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(selectedGpu, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(selectedGpu, surface, &formatCount, formats.data());

    s_swapchainFormat = formats[0].format;
    for (const auto &f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            s_swapchainFormat = f.format;
            break;
        }
    }

    VkExtent2D extent = { width, height };
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    }

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }

    // 7. Swapchain
    VkSwapchainCreateInfoKHR scInfo = {};
    scInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    scInfo.surface = surface;
    scInfo.minImageCount = imageCount;
    scInfo.imageFormat = s_swapchainFormat;
    scInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    scInfo.imageExtent = extent;
    scInfo.imageArrayLayers = 1;
    scInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    scInfo.preTransform = caps.currentTransform;
    scInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    scInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    scInfo.clipped = VK_TRUE;

    uint32_t qfIndices[] = { s_graphicsQueueFamily, s_presentQueueFamily };
    if (s_graphicsQueueFamily != s_presentQueueFamily) {
        scInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        scInfo.queueFamilyIndexCount = 2;
        scInfo.pQueueFamilyIndices = qfIndices;
    } else {
        scInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(device, &scInfo, nullptr, &swapchain) != VK_SUCCESS) {
        return false;
    }
    s_ctx.swapchain = swapchain;
    s_ctx.width = extent.width;
    s_ctx.height = extent.height;

    uint32_t scImageCount = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &scImageCount, nullptr);
    s_swapchainImages.resize(scImageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &scImageCount, s_swapchainImages.data());
    s_ctx.imageCount = scImageCount;

    // 8. Image Views
    s_swapchainImageViews.resize(scImageCount);
    for (uint32_t i = 0; i < scImageCount; ++i) {
        VkImageViewCreateInfo ivInfo = {};
        ivInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivInfo.image = s_swapchainImages[i];
        ivInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivInfo.format = s_swapchainFormat;
        ivInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                              VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        ivInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivInfo.subresourceRange.levelCount = 1;
        ivInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device, &ivInfo, nullptr, &s_swapchainImageViews[i]) != VK_SUCCESS) {
            return false;
        }
    }

    // 9. Render Pass
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = s_swapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo = {};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &colorAttachment;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &rpInfo, nullptr, &s_renderPass) != VK_SUCCESS) {
        return false;
    }

    // 10. Framebuffers
    s_framebuffers.resize(scImageCount);
    for (uint32_t i = 0; i < scImageCount; ++i) {
        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = s_renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &s_swapchainImageViews[i];
        fbInfo.width = extent.width;
        fbInfo.height = extent.height;
        fbInfo.layers = 1;
        if (vkCreateFramebuffer(device, &fbInfo, nullptr, &s_framebuffers[i]) != VK_SUCCESS) {
            return false;
        }
    }

    // 11. Command Pool & Command Buffers
    VkCommandPoolCreateInfo cpInfo = {};
    cpInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpInfo.queueFamilyIndex = s_graphicsQueueFamily;
    cpInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(device, &cpInfo, nullptr, &s_commandPool) != VK_SUCCESS) {
        return false;
    }

    s_commandBuffers.resize(scImageCount);
    VkCommandBufferAllocateInfo cbAlloc = {};
    cbAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbAlloc.commandPool = s_commandPool;
    cbAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbAlloc.commandBufferCount = scImageCount;
    if (vkAllocateCommandBuffers(device, &cbAlloc, s_commandBuffers.data()) != VK_SUCCESS) {
        return false;
    }

    // 12. Semaphores & Fence
    VkSemaphoreCreateInfo semInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT };

    if (vkCreateSemaphore(device, &semInfo, nullptr, &s_imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateSemaphore(device, &semInfo, nullptr, &s_renderFinishedSemaphore) != VK_SUCCESS ||
        vkCreateFence(device, &fenceInfo, nullptr, &s_inFlightFence) != VK_SUCCESS) {
        return false;
    }

    s_ctx.isInitialized = true;
    Platform_CreateVulkanPipeline();
    return true;
}

bool Platform_BeginVulkanFrame(uint32_t *imageIndex)
{
    if (!s_ctx.isInitialized) return false;

    VkDevice device = (VkDevice)s_ctx.device;
    vkWaitForFences(device, 1, &s_inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &s_inFlightFence);

    VkResult res = vkAcquireNextImageKHR(device, (VkSwapchainKHR)s_ctx.swapchain,
                                         UINT64_MAX, s_imageAvailableSemaphore,
                                         VK_NULL_HANDLE, imageIndex);
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        return false;
    }
    s_ctx.currentImageIndex = *imageIndex;

    VkCommandBuffer cb = s_commandBuffers[*imageIndex];
    vkResetCommandBuffer(cb, 0);

    VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(cb, &beginInfo);

    static VkClearValue s_clearColor = { {{ 0.0f, 0.0f, 0.0f, 1.0f }} };

    VkRenderPassBeginInfo rpBegin = {};
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass = s_renderPass;
    rpBegin.framebuffer = s_framebuffers[*imageIndex];
    rpBegin.renderArea.offset = { 0, 0 };
    rpBegin.renderArea.extent = { (uint32_t)s_ctx.width, (uint32_t)s_ctx.height };
    rpBegin.clearValueCount = 1;
    rpBegin.pClearValues = &s_clearColor;

    vkCmdBeginRenderPass(cb, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
    return true;
}

bool Platform_EndVulkanFrame(uint32_t imageIndex)
{
    if (!s_ctx.isInitialized) return false;

    VkCommandBuffer cb = s_commandBuffers[imageIndex];
    vkCmdEndRenderPass(cb);
    vkEndCommandBuffer(cb);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { s_imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cb;

    VkSemaphore signalSemaphores[] = { s_renderFinishedSemaphore };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VkDevice device = (VkDevice)s_ctx.device;
    if (vkQueueSubmit(s_graphicsQueue, 1, &submitInfo, s_inFlightFence) != VK_SUCCESS) {
        return false;
    }

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = { (VkSwapchainKHR)s_ctx.swapchain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    VkResult res = vkQueuePresentKHR(s_presentQueue, &presentInfo);
    return (res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR);
}
void Platform_SetVulkanClearColor(float r, float g, float b, float a)
{
    s_clearColor.color.float32[0] = r;
    s_clearColor.color.float32[1] = g;
    s_clearColor.color.float32[2] = b;
    s_clearColor.color.float32[3] = a;
}
bool Platform_CreateVulkanPipeline(void)
{
    if (!s_ctx.isInitialized || s_graphicsPipeline != VK_NULL_HANDLE) {
        return true;
    }
    VkDevice device = (VkDevice)s_ctx.device;
    VkPhysicalDevice phys_dev = (VkPhysicalDevice)s_ctx.physicalDevice;

    // 1. Shaders
    VkShaderModuleCreateInfo vert_ci = {};
    vert_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vert_ci.codeSize = sizeof(k_vert_spv);
    vert_ci.pCode = k_vert_spv;
    VkShaderModule vert_mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &vert_ci, nullptr, &vert_mod) != VK_SUCCESS) return false;

    VkShaderModuleCreateInfo frag_ci = {};
    frag_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    frag_ci.codeSize = sizeof(k_frag_spv);
    frag_ci.pCode = k_frag_spv;
    VkShaderModule frag_mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &frag_ci, nullptr, &frag_mod) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vert_mod, nullptr);
        return false;
    }

    // 2. Vertex Input State: pos (vec3) at offset 0, normal (vec3) at 12, diffuse (vec4) at 24
    VkVertexInputBindingDescription bind_desc = {};
    bind_desc.binding = 0;
    bind_desc.stride = 40; // Default stride for XYZNDUV1/XYZNDUV2
    bind_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attr_descs[3] = {};
    attr_descs[0].location = 0;
    attr_descs[0].binding = 0;
    attr_descs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attr_descs[0].offset = 0;

    attr_descs[1].location = 1;
    attr_descs[1].binding = 0;
    attr_descs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attr_descs[1].offset = 12;

    attr_descs[2].location = 2;
    attr_descs[2].binding = 0;
    attr_descs[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attr_descs[2].offset = 24;

    VkPipelineVertexInputStateCreateInfo vi_ci = {};
    vi_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi_ci.vertexBindingDescriptionCount = 1;
    vi_ci.pVertexBindingDescriptions = &bind_desc;
    vi_ci.vertexAttributeDescriptionCount = 3;
    vi_ci.pVertexAttributeDescriptions = attr_descs;

    VkPipelineInputAssemblyStateCreateInfo ia_ci = {};
    ia_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia_ci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport vp = { 0.0f, 0.0f, (float)s_ctx.width, (float)s_ctx.height, 0.0f, 1.0f };
    VkRect2D sc = { { 0, 0 }, { (uint32_t)s_ctx.width, (uint32_t)s_ctx.height } };
    VkPipelineViewportStateCreateInfo vp_ci = {};
    vp_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp_ci.viewportCount = 1;
    vp_ci.pViewports = &vp;
    vp_ci.scissorCount = 1;
    vp_ci.pScissors = &sc;

    VkPipelineRasterizationStateCreateInfo rast_ci = {};
    rast_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rast_ci.polygonMode = VK_POLYGON_MODE_FILL;
    rast_ci.cullMode = VK_CULL_MODE_NONE;
    rast_ci.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rast_ci.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms_ci = {};
    ms_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms_ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cb_att = {};
    cb_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cb_att.blendEnable = VK_TRUE;
    cb_att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cb_att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cb_att.colorBlendOp = VK_BLEND_OP_ADD;
    cb_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cb_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    cb_att.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo cb_ci = {};
    cb_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb_ci.attachmentCount = 1;
    cb_ci.pAttachments = &cb_att;

    VkDynamicState dyn_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn_ci = {};
    dyn_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn_ci.dynamicStateCount = 2;
    dyn_ci.pDynamicStates = dyn_states;

    VkPipelineLayoutCreateInfo layout_ci = {};
    layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    if (vkCreatePipelineLayout(device, &layout_ci, nullptr, &s_pipelineLayout) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vert_mod, nullptr);
        vkDestroyShaderModule(device, frag_mod, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert_mod;
    stages[0].pName = "main";

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag_mod;
    stages[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipe_ci = {};
    pipe_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipe_ci.stageCount = 2;
    pipe_ci.pStages = stages;
    pipe_ci.pVertexInputState = &vi_ci;
    pipe_ci.pInputAssemblyState = &ia_ci;
    pipe_ci.pViewportState = &vp_ci;
    pipe_ci.pRasterizationState = &rast_ci;
    pipe_ci.pMultisampleState = &ms_ci;
    pipe_ci.pColorBlendState = &cb_ci;
    pipe_ci.pDynamicState = &dyn_ci;
    pipe_ci.layout = s_pipelineLayout;
    pipe_ci.renderPass = s_renderPass;
    pipe_ci.subpass = 0;

    VkResult pRes = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipe_ci, nullptr, &s_graphicsPipeline);
    vkDestroyShaderModule(device, vert_mod, nullptr);
    vkDestroyShaderModule(device, frag_mod, nullptr);
    if (pRes != VK_SUCCESS) return false;

    // 3. Dynamic Buffers
    const VkDeviceSize vbSize = 4 * 1024 * 1024;
    VkBufferCreateInfo vb_ci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    vb_ci.size = vbSize;
    vb_ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vkCreateBuffer(device, &vb_ci, nullptr, &s_dynamicVB);
    VkMemoryRequirements vb_reqs;
    vkGetBufferMemoryRequirements(device, s_dynamicVB, &vb_reqs);
    VkMemoryAllocateInfo vb_ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    vb_ai.allocationSize = vb_reqs.size;
    vb_ai.memoryTypeIndex = find_memory_type(phys_dev, vb_reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(device, &vb_ai, nullptr, &s_dynamicVBMem);
    vkBindBufferMemory(device, s_dynamicVB, s_dynamicVBMem, 0);
    vkMapMemory(device, s_dynamicVBMem, 0, vbSize, 0, &s_mappedVB);

    const VkDeviceSize ibSize = 1024 * 1024;
    VkBufferCreateInfo ib_ci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    ib_ci.size = ibSize;
    ib_ci.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    vkCreateBuffer(device, &ib_ci, nullptr, &s_dynamicIB);
    VkMemoryRequirements ib_reqs;
    vkGetBufferMemoryRequirements(device, s_dynamicIB, &ib_reqs);
    VkMemoryAllocateInfo ib_ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    ib_ai.allocationSize = ib_reqs.size;
    ib_ai.memoryTypeIndex = find_memory_type(phys_dev, ib_reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(device, &ib_ai, nullptr, &s_dynamicIBMem);
    vkBindBufferMemory(device, s_dynamicIB, s_dynamicIBMem, 0);
    vkMapMemory(device, s_dynamicIBMem, 0, ibSize, 0, &s_mappedIB);

    return true;
}

bool Platform_DrawVulkanIndexed(const void *vertices, uint32_t vertexCount, uint32_t vertexStride,
                               const uint16_t *indices, uint32_t indexCount)
{
    if (!s_ctx.isInitialized || !s_graphicsPipeline || !vertices || !indices || indexCount == 0 || !s_mappedVB || !s_mappedIB) {
        return false;
    }
    if (vertexCount * vertexStride > 4 * 1024 * 1024 || indexCount * sizeof(uint16_t) > 1024 * 1024) {
        return false;
    }

    memcpy(s_mappedVB, vertices, vertexCount * vertexStride);
    memcpy(s_mappedIB, indices, indexCount * sizeof(uint16_t));

    VkCommandBuffer cb = s_commandBuffers[s_ctx.currentImageIndex];
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, s_graphicsPipeline);

    VkViewport vp = { 0.0f, 0.0f, (float)s_ctx.width, (float)s_ctx.height, 0.0f, 1.0f };
    VkRect2D scissor = { { 0, 0 }, { (uint32_t)s_ctx.width, (uint32_t)s_ctx.height } };
    vkCmdSetViewport(cb, 0, 1, &vp);
    vkCmdSetScissor(cb, 0, 1, &scissor);

    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cb, 0, 1, &s_dynamicVB, offsets);
    vkCmdBindIndexBuffer(cb, s_dynamicIB, 0, VK_INDEX_TYPE_UINT16);

    vkCmdDrawIndexed(cb, indexCount, 1, 0, 0, 0);
    return true;
}

bool Platform_DrawVulkanVertices(const void *vertices, uint32_t vertexCount, uint32_t vertexStride)
{
    if (!s_ctx.isInitialized || !s_graphicsPipeline || !vertices || vertexCount == 0 || !s_mappedVB) {
        return false;
    }
    if (vertexCount * vertexStride > 4 * 1024 * 1024) {
        return false;
    }

    memcpy(s_mappedVB, vertices, vertexCount * vertexStride);

    VkCommandBuffer cb = s_commandBuffers[s_ctx.currentImageIndex];
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, s_graphicsPipeline);

    VkViewport vp = { 0.0f, 0.0f, (float)s_ctx.width, (float)s_ctx.height, 0.0f, 1.0f };
    VkRect2D scissor = { { 0, 0 }, { (uint32_t)s_ctx.width, (uint32_t)s_ctx.height } };
    vkCmdSetViewport(cb, 0, 1, &vp);
    vkCmdSetScissor(cb, 0, 1, &scissor);

    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cb, 0, 1, &s_dynamicVB, offsets);

    vkCmdDraw(cb, vertexCount, 1, 0, 0);
    return true;
}

void *Platform_CreateVulkanTexture(uint32_t width, uint32_t height, const void *pixels, uint32_t format)
{
    if (!s_ctx.isInitialized || width == 0 || height == 0) return nullptr;
    VkDevice device = (VkDevice)s_ctx.device;
    VkPhysicalDevice phys_dev = (VkPhysicalDevice)s_ctx.physicalDevice;

    PlatformVulkanTexture *tex = new PlatformVulkanTexture();
    tex->width = width;
    tex->height = height;

    VkImageCreateInfo img_ci = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    img_ci.imageType = VK_IMAGE_TYPE_2D;
    img_ci.format = VK_FORMAT_B8G8R8A8_UNORM;
    img_ci.extent = { width, height, 1 };
    img_ci.mipLevels = 1;
    img_ci.arrayLayers = 1;
    img_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    img_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    img_ci.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    img_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &img_ci, nullptr, &tex->image) != VK_SUCCESS) {
        delete tex;
        return nullptr;
    }

    VkMemoryRequirements reqs;
    vkGetImageMemoryRequirements(device, tex->image, &reqs);
    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    ai.allocationSize = reqs.size;
    ai.memoryTypeIndex = find_memory_type(phys_dev, reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (ai.memoryTypeIndex == UINT32_MAX) {
        ai.memoryTypeIndex = find_memory_type(phys_dev, reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    }
    vkAllocateMemory(device, &ai, nullptr, &tex->memory);
    vkBindImageMemory(device, tex->image, tex->memory, 0);

    VkImageViewCreateInfo view_ci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    view_ci.image = tex->image;
    view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_ci.format = VK_FORMAT_B8G8R8A8_UNORM;
    view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_ci.subresourceRange.levelCount = 1;
    view_ci.subresourceRange.layerCount = 1;
    vkCreateImageView(device, &view_ci, nullptr, &tex->view);

    VkSamplerCreateInfo samp_ci = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samp_ci.magFilter = VK_FILTER_LINEAR;
    samp_ci.minFilter = VK_FILTER_LINEAR;
    samp_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samp_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samp_ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    vkCreateSampler(device, &samp_ci, nullptr, &tex->sampler);

    return tex;
}

void Platform_BindVulkanTexture(void *texture)
{
    s_boundTexture = (PlatformVulkanTexture*)texture;
}

void Platform_DestroyVulkanTexture(void *texture)
{
    if (!texture || !s_ctx.isInitialized) return;
    PlatformVulkanTexture *tex = (PlatformVulkanTexture*)texture;
    VkDevice device = (VkDevice)s_ctx.device;
    if (tex->sampler) vkDestroySampler(device, tex->sampler, nullptr);
    if (tex->view) vkDestroyImageView(device, tex->view, nullptr);
    if (tex->image) vkDestroyImage(device, tex->image, nullptr);
    if (tex->memory) vkFreeMemory(device, tex->memory, nullptr);
    delete tex;
}


void Platform_ShutdownVulkan(void)
{
    if (!s_ctx.isInitialized) return;

    VkDevice device = (VkDevice)s_ctx.device;
    VkInstance instance = (VkInstance)s_ctx.instance;

    vkDeviceWaitIdle(device);

    if (s_imageAvailableSemaphore) vkDestroySemaphore(device, s_imageAvailableSemaphore, nullptr);
    if (s_renderFinishedSemaphore) vkDestroySemaphore(device, s_renderFinishedSemaphore, nullptr);
    if (s_inFlightFence) vkDestroyFence(device, s_inFlightFence, nullptr);

    if (s_commandPool) vkDestroyCommandPool(device, s_commandPool, nullptr);
    if (s_graphicsPipeline) { vkDestroyPipeline(device, s_graphicsPipeline, nullptr); s_graphicsPipeline = VK_NULL_HANDLE; }
    if (s_pipelineLayout) { vkDestroyPipelineLayout(device, s_pipelineLayout, nullptr); s_pipelineLayout = VK_NULL_HANDLE; }
    if (s_dynamicVBMem) { vkUnmapMemory(device, s_dynamicVBMem); vkFreeMemory(device, s_dynamicVBMem, nullptr); s_dynamicVBMem = VK_NULL_HANDLE; }
    if (s_dynamicVB) { vkDestroyBuffer(device, s_dynamicVB, nullptr); s_dynamicVB = VK_NULL_HANDLE; }
    if (s_dynamicIBMem) { vkUnmapMemory(device, s_dynamicIBMem); vkFreeMemory(device, s_dynamicIBMem, nullptr); s_dynamicIBMem = VK_NULL_HANDLE; }
    if (s_dynamicIB) { vkDestroyBuffer(device, s_dynamicIB, nullptr); s_dynamicIB = VK_NULL_HANDLE; }

    for (auto fb : s_framebuffers) {
        if (fb) vkDestroyFramebuffer(device, fb, nullptr);
    }
    s_framebuffers.clear();

    if (s_renderPass) vkDestroyRenderPass(device, s_renderPass, nullptr);

    for (auto iv : s_swapchainImageViews) {
        if (iv) vkDestroyImageView(device, iv, nullptr);
    }
    s_swapchainImageViews.clear();
    s_swapchainImages.clear();

    if (s_ctx.swapchain) {
        vkDestroySwapchainKHR(device, (VkSwapchainKHR)s_ctx.swapchain, nullptr);
        s_ctx.swapchain = nullptr;
    }

    if (device) vkDestroyDevice(device, nullptr);
    if (s_ctx.surface) vkDestroySurfaceKHR(instance, (VkSurfaceKHR)s_ctx.surface, nullptr);
    if (instance) vkDestroyInstance(instance, nullptr);

    s_ctx.isInitialized = false;
}
