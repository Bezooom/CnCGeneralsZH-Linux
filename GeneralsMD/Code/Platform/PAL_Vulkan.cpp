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
