#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>
#include <X11/Xlib.h>

#include "PAL_Vulkan.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <unistd.h>

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

// One pair of semaphores and one fence per swapchain image. A single shared fence serialises the
// CPU behind the GPU: the frame cannot be recorded until the previous frame rendered, which caps
// throughput at one frame in flight no matter how many images the swapchain offers.
static std::vector<VkSemaphore> s_imageAvailableSemaphores;
static std::vector<VkSemaphore> s_renderFinishedSemaphores;
static std::vector<VkFence> s_inFlightFences;
static VkPresentModeKHR s_presentMode = VK_PRESENT_MODE_FIFO_KHR;
static VkClearValue s_clearColor = { {{ 0.0f, 0.0f, 0.0f, 1.0f }} };
struct PlatformVulkanTexture {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    uint32_t width;
    uint32_t height;
    uint32_t levels;
    VkDeviceSize uploadSize;
    // Compressed formats upload in 4x4 blocks; plain formats are "1 block = 1 texel".
    uint32_t blockDim;
    uint32_t blockBytes;
    uint32_t vkFormat;
    // One bit per mip level, set once that level's first copy has run: it tells the copy barriers
    // whether the level's layout is still UNDEFINED.
    uint32_t levelValidMask;
    // Set once every level's layout has been given a defined value. A level with no data yet is
    // still in UNDEFINED, and a sampler that reaches it reads undefined memory.
    bool levelsInitialised;
    // One sampler state this texture has been drawn with. Fog, terrain and water ask for different
    // address modes on the same texture, and a descriptor set a queued command buffer may still
    // read must never be rewritten, so every state gets its own sampler and its own four sets -
    // one per texture stage, because the pipeline layout has four single-sampler set slots.
    struct SamplerVariant {
        uint32_t key;
        VkSampler sampler;
        VkDescriptorSet sets[4];
    };
    std::vector<SamplerVariant> variants;
};

static VkPipeline s_graphicsPipelineOpaque = VK_NULL_HANDLE;
static VkPipeline s_graphicsPipelineBlend = VK_NULL_HANDLE;
static VkPipeline s_graphicsPipelineMultiply = VK_NULL_HANDLE;
static VkPipeline s_graphicsPipelineAdditive = VK_NULL_HANDLE;
static int s_blendMode = 0;
static VkPipelineLayout s_pipelineLayout = VK_NULL_HANDLE;
static VkDescriptorSetLayout s_descriptorSetLayout = VK_NULL_HANDLE;
static VkDescriptorPool s_descriptorPool = VK_NULL_HANDLE;
static PlatformVulkanTexture *s_defaultTexture = nullptr;
// The four texture stages the shim binds before a draw, and the descriptor sets the last draw used
// for them. A pipeline bind invalidates the sets, so the cache is dropped whenever one happens.
static PlatformVulkanTexture *s_boundTextures[4] = { nullptr, nullptr, nullptr, nullptr };
static uint32_t s_boundSamplerKeys[4] = { 0, 0, 0, 0 };
static VkDescriptorSet s_boundSets[4] = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE };
static PlatformVulkanPushConstants s_pushConstants = {};
static VkBuffer s_dynamicVB = VK_NULL_HANDLE;
static VkDeviceMemory s_dynamicVBMem = VK_NULL_HANDLE;
static void *s_mappedVB = nullptr;
static VkBuffer s_dynamicIB = VK_NULL_HANDLE;
static VkDeviceMemory s_dynamicIBMem = VK_NULL_HANDLE;
static void *s_mappedIB = nullptr;
static VkDeviceSize s_currentVBOffset = 0;
static VkDeviceSize s_currentIBOffset = 0;
static VkDeviceSize s_vbTotal = 0;
static VkDeviceSize s_ibTotal = 0;
static VkDeviceSize s_vbWindow = 0;
static VkDeviceSize s_ibWindow = 0;
static bool s_ringOverflow = false;
// A CPU frame counter plus the image each frame acquired. Re-acquiring a swapchain image already
// proves its previous use finished - an image is only acquirable again after its present
// completed, and a completed present means its rendering ran - which is the base correctness
// argument for the deferred-free drain below, before any fence even enters the picture.
static uint32_t s_frameCounter = 0;
static bool s_inFrame = false;
static uint32_t s_currentImage = 0;
static uint32_t s_currentSlot = 0;
static std::vector<int32_t> s_lastAcquiredFrame; // per image: the frame that last acquired it
// Frames in flight are tracked per slot, not per swapchain image: an acquire semaphore must not be
// signalled again until the submit that waits on it has run, and only a slot that waits on its own
// fence guarantees that.
static uint32_t s_frameSlot = 0;

// Texture copies ride one grow-only host-visible staging ring, windowed per swapchain image like
// the vertex/index ring, plus a pending list drained at frame start into the frame's command
// buffer before the render pass begins. The frame never blocks on a texture update.
struct PendingTexUpdate {
    PlatformVulkanTexture *tex;
    uint32_t level;
    int x, y, w, h;
    std::vector<uint8_t> bytes;
};
static std::vector<PendingTexUpdate> s_pendingTexUpdates;
static VkBuffer s_stagingBuffer = VK_NULL_HANDLE;
static VkDeviceMemory s_stagingMemory = VK_NULL_HANDLE;
static void *s_mappedStaging = nullptr;
static VkDeviceSize s_stagingTotal = 0;
static VkDeviceSize s_stagingWindow = 0;
static VkDeviceSize s_stagingCursor = 0;
static VkDeviceSize s_imageGranularity = 256;

struct PendingTexFree {
    PlatformVulkanTexture *tex;
    uint32_t frame;
    uint32_t image; // UINT32_MAX: not tied to one image; wait until every image rotated past it
};
static std::vector<PendingTexFree> s_pendingTexFrees;

// Draw-state cache, reset with the command buffer each frame: the engine re-sets the same
// viewport, scissor and depth state thousands of times per second; Vulkan needs it once.
static VkPipeline s_lastPipeline = VK_NULL_HANDLE;
static bool s_dynViewportScissor = false;
static int s_lastDepthKey = -1;
static VkDescriptorSet s_lastDescSet = VK_NULL_HANDLE;

static VkImage s_depthImage = VK_NULL_HANDLE;
static VkDeviceMemory s_depthMemory = VK_NULL_HANDLE;
static VkImageView s_depthView = VK_NULL_HANDLE;
static VkFormat s_depthFormat = VK_FORMAT_D32_SFLOAT;
static bool s_depthTest = true;
static bool s_depthWrite = true;
static VkCompareOp s_depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;
static bool s_dynamicDepth = false;
static PFN_vkCmdSetDepthTestEnable s_cmdSetDepthTest = nullptr;
static PFN_vkCmdSetDepthWriteEnable s_cmdSetDepthWrite = nullptr;
static PFN_vkCmdSetDepthCompareOp s_cmdSetDepthCompare = nullptr;
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

static VkDeviceSize align_up(VkDeviceSize value, VkDeviceSize alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

static void destroy_sync_objects(VkDevice device)
{
    for (auto sem : s_imageAvailableSemaphores) if (sem) vkDestroySemaphore(device, sem, nullptr);
    for (auto sem : s_renderFinishedSemaphores) if (sem) vkDestroySemaphore(device, sem, nullptr);
    for (auto fence : s_inFlightFences) if (fence) vkDestroyFence(device, fence, nullptr);
    s_imageAvailableSemaphores.clear();
    s_renderFinishedSemaphores.clear();
    s_inFlightFences.clear();
}

static bool create_sync_objects(VkDevice device, uint32_t count)
{
    VkSemaphoreCreateInfo semInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT };
    s_imageAvailableSemaphores.resize(count);
    s_renderFinishedSemaphores.resize(count);
    s_inFlightFences.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (vkCreateSemaphore(device, &semInfo, nullptr, &s_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semInfo, nullptr, &s_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &s_inFlightFences[i]) != VK_SUCCESS) {
            destroy_sync_objects(device);
            return false;
        }
    }
    s_lastAcquiredFrame.assign(count, -1);
    return true;
}

// The staging ring is a different memory object from every texture image, so copies only need
// texel-multiple offsets (16 bytes covers BC blocks and every 32-bit format), not the driver's
// bufferImageGranularity.
static const VkDeviceSize STAGING_ALIGN = 16;

static void free_staging(VkDevice device)
{
    if (s_mappedStaging) vkUnmapMemory(device, s_stagingMemory);
    if (s_stagingMemory) vkFreeMemory(device, s_stagingMemory, nullptr);
    if (s_stagingBuffer) vkDestroyBuffer(device, s_stagingBuffer, nullptr);
    s_mappedStaging = nullptr;
    s_stagingMemory = VK_NULL_HANDLE;
    s_stagingBuffer = VK_NULL_HANDLE;
    s_stagingTotal = 0;
}

// Only ever called at a frame boundary, before this frame's command buffer records anything, so
// one drain of the queue by WaitIdle per doubling is the whole cost.
static bool ensure_staging(VkDevice device, VkDeviceSize needed)
{
    if (s_stagingBuffer && needed <= s_stagingTotal) return true;
    vkDeviceWaitIdle(device);
    free_staging(device);
    VkPhysicalDevice phys = (VkPhysicalDevice)s_ctx.physicalDevice;
    VkDeviceSize size = needed > 2 * s_stagingTotal ? needed : 2 * s_stagingTotal;
    if (size < 32u * 1024u * 1024u) size = 32u * 1024u * 1024u;
    VkBufferCreateInfo ci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    ci.size = size;
    ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (vkCreateBuffer(device, &ci, nullptr, &s_stagingBuffer) != VK_SUCCESS) { s_stagingBuffer = VK_NULL_HANDLE; return false; }
    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(device, s_stagingBuffer, &reqs);
    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    ai.allocationSize = reqs.size;
    ai.memoryTypeIndex = find_memory_type(phys, reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (ai.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(device, &ai, nullptr, &s_stagingMemory) != VK_SUCCESS ||
        vkBindBufferMemory(device, s_stagingBuffer, s_stagingMemory, 0) != VK_SUCCESS ||
        vkMapMemory(device, s_stagingMemory, 0, size, 0, &s_mappedStaging) != VK_SUCCESS) {
        free_staging(device);
        return false;
    }
    s_stagingTotal = size;
    return true;
}

static bool ensure_dynamic(VkDevice device, VkDeviceSize vbTotal, VkDeviceSize ibTotal)
{
    VkPhysicalDevice phys = (VkPhysicalDevice)s_ctx.physicalDevice;
    vkDeviceWaitIdle(device);
    if (s_mappedVB) vkUnmapMemory(device, s_dynamicVBMem);
    if (s_dynamicVBMem) vkFreeMemory(device, s_dynamicVBMem, nullptr);
    if (s_dynamicVB) vkDestroyBuffer(device, s_dynamicVB, nullptr);
    s_mappedVB = nullptr; s_dynamicVB = VK_NULL_HANDLE; s_dynamicVBMem = VK_NULL_HANDLE;
    if (s_mappedIB) vkUnmapMemory(device, s_dynamicIBMem);
    if (s_dynamicIBMem) vkFreeMemory(device, s_dynamicIBMem, nullptr);
    if (s_dynamicIB) vkDestroyBuffer(device, s_dynamicIB, nullptr);
    s_mappedIB = nullptr; s_dynamicIB = VK_NULL_HANDLE; s_dynamicIBMem = VK_NULL_HANDLE;

    struct { VkBuffer *buf; VkDeviceMemory *mem; void **map; VkBufferUsageFlags usage; VkDeviceSize size; } items[2] = {
        { &s_dynamicVB, &s_dynamicVBMem, &s_mappedVB, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vbTotal },
        { &s_dynamicIB, &s_dynamicIBMem, &s_mappedIB, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, ibTotal },
    };
    for (auto &it : items) {
        VkBufferCreateInfo ci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        ci.size = it.size;
        ci.usage = it.usage;
        if (vkCreateBuffer(device, &ci, nullptr, it.buf) != VK_SUCCESS) return false;
        VkMemoryRequirements reqs;
        vkGetBufferMemoryRequirements(device, *it.buf, &reqs);
        VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        ai.allocationSize = reqs.size;
        ai.memoryTypeIndex = find_memory_type(phys, reqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (ai.memoryTypeIndex == UINT32_MAX ||
            vkAllocateMemory(device, &ai, nullptr, it.mem) != VK_SUCCESS ||
            vkBindBufferMemory(device, *it.buf, *it.mem, 0) != VK_SUCCESS ||
            vkMapMemory(device, *it.mem, 0, it.size, 0, it.map) != VK_SUCCESS) {
            return false;
        }
    }
    s_vbTotal = vbTotal;
    s_ibTotal = ibTotal;
    return true;
}

static void layout_ring_windows(void)
{
    uint32_t n = s_ctx.imageCount ? s_ctx.imageCount : 1;
    s_vbWindow = (s_vbTotal / n) & ~(VkDeviceSize(256) - 1);
    s_ibWindow = (s_ibTotal / n) & ~(VkDeviceSize(4) - 1);
    s_stagingWindow = (s_stagingTotal / n) & ~(STAGING_ALIGN - 1);
}


// Records one queued copy per pending update into cb (this frame's command buffer, recorded
// before the render pass begins), fed from the staging ring window reserved for this image.
static void flush_pending_texture_copies(VkCommandBuffer cb)
{
    if (s_pendingTexUpdates.empty()) return;
    if (!s_stagingBuffer || !s_mappedStaging) { s_pendingTexUpdates.clear(); return; }
    VkPhysicalDevice phys = (VkPhysicalDevice)s_ctx.physicalDevice;
    // Updates that do not fit this frame's window wait in `deferred`, in order. Erasing by count
    // instead (the old code) dropped whichever update happened to sit at the front rather than the
    // one that was skipped, so a large texture - the menu map's terrain, for one - could lose its
    // first upload and sample an UNDEFINED image for the rest of the run: flat grey ground.
    std::vector<PendingTexUpdate> deferred;
    for (auto &upd : s_pendingTexUpdates) {
        PlatformVulkanTexture *tex = upd.tex;
        const VkDeviceSize size = (VkDeviceSize)upd.bytes.size();
        if (size == 0) continue;
        const VkDeviceSize base = (VkDeviceSize)s_currentSlot * s_stagingWindow;
        const VkDeviceSize end = base + s_stagingWindow;
        VkDeviceSize off = align_up(s_stagingCursor, STAGING_ALIGN);
        if (off + size > end) {
            // The window filled after the pre-grow estimate (a frame can queue more updates than
            // the previous one drained); BeginVulkanFrame sizes the ring from this backlog, so the
            // next frame has room. Retry rather than drop.
            deferred.push_back(std::move(upd));
            continue;
        }
        memcpy((char *)s_mappedStaging + off, upd.bytes.data(), size);
        s_stagingCursor = off + size;

        const uint32_t levelBit = 1u << upd.level;
        const bool levelValid = (tex->levelValidMask & levelBit) != 0;
        VkImageMemoryBarrier bar = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        bar.image = (VkImage)tex->image;
        bar.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bar.subresourceRange.baseMipLevel = upd.level;
        bar.subresourceRange.levelCount = 1;
        bar.subresourceRange.baseArrayLayer = 0;
        bar.subresourceRange.layerCount = 1;
        bar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        const bool layoutKnown = levelValid || tex->levelsInitialised;
        bar.oldLayout = layoutKnown ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
        bar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        bar.srcAccessMask = layoutKnown ? VK_ACCESS_SHADER_READ_BIT : 0;
        bar.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cb, layoutKnown ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &bar);

        VkBufferImageCopy region = {};
        region.bufferOffset = off;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = upd.level;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { upd.x, upd.y, 0 };
        region.imageExtent = { (uint32_t)upd.w, (uint32_t)upd.h, 1 };
        vkCmdCopyBufferToImage(cb, s_stagingBuffer, (VkImage)tex->image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        bar.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        bar.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        bar.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        bar.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &bar);
        tex->levelValidMask |= levelBit;
        if (!tex->levelsInitialised && tex->levels > 1) {
            // Give the levels that carry no data yet a defined layout once, so nothing can sample
            // an image subresource that is still UNDEFINED (the validation layer reports exactly
            // that for the terrain atlas, whose mip chain arrives one level at a time).
            VkImageMemoryBarrier rest = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            rest.image = (VkImage)tex->image;
            rest.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            rest.subresourceRange.baseMipLevel = 1;
            rest.subresourceRange.levelCount = tex->levels - 1;
            rest.subresourceRange.baseArrayLayer = 0;
            rest.subresourceRange.layerCount = 1;
            rest.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            rest.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            rest.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            rest.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            rest.srcAccessMask = 0;
            rest.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &rest);
            tex->levelsInitialised = true;
        }
    }
    s_pendingTexUpdates.swap(deferred);
    (void)phys;
}

// Texture updates issued while no frame is in flight (loading, menus between Presents) ride a
// one-shot command buffer. Waiting here is allowed: the game is not in a frame.
static void flush_texture_updates_idle(void)
{
    if (s_pendingTexUpdates.empty() || !s_ctx.isInitialized) return;
    VkDevice device = (VkDevice)s_ctx.device;
    VkDeviceSize total = 0;
    for (auto &upd : s_pendingTexUpdates) {
        total += align_up((VkDeviceSize)upd.bytes.size(), STAGING_ALIGN);
    }
    if (!ensure_staging(device, total > s_stagingTotal ? total : s_stagingTotal)) {
        s_pendingTexUpdates.clear();
        return;
    }
    s_stagingCursor = 0;
    VkCommandBufferAllocateInfo ca = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    ca.commandPool = s_commandPool;
    ca.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ca.commandBufferCount = 1;
    VkCommandBuffer cb = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device, &ca, &cb) != VK_SUCCESS) { s_pendingTexUpdates.clear(); return; }
    VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);
    flush_pending_texture_copies(cb);
    vkEndCommandBuffer(cb);
    VkSubmitInfo submit = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cb;
    vkQueueSubmit(s_graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(s_graphicsQueue);
    vkFreeCommandBuffers(device, s_commandPool, 1, &cb);
}

static void free_texture_objects(VkDevice device, PlatformVulkanTexture *tex)
{
    for (auto &variant : tex->variants) {
        if (s_descriptorPool) {
            vkFreeDescriptorSets(device, s_descriptorPool, 4, variant.sets);
        }
        if (variant.sampler) vkDestroySampler(device, variant.sampler, nullptr);
    }
    tex->variants.clear();
    if (tex->view) vkDestroyImageView(device, tex->view, nullptr);
    if (tex->image) vkDestroyImage(device, tex->image, nullptr);
    if (tex->memory) vkFreeMemory(device, tex->memory, nullptr);
    delete tex;
}

// The sampler for one packed state key, created once per texture per state. Address modes and
// filters come straight from the D3D9 enumerations the shim recorded.
static PlatformVulkanTexture::SamplerVariant *texture_variant(PlatformVulkanTexture *tex, uint32_t key)
{
    for (auto &variant : tex->variants) {
        if (variant.key == key) return &variant;
    }
    if (tex->view == VK_NULL_HANDLE) {
        fprintf(stderr, "variant for a texture with no view: %ux%u\n", tex->width, tex->height);
        fflush(stderr);
    }
    VkDevice device = (VkDevice)s_ctx.device;
    PlatformVulkanTexture::SamplerVariant variant = {};
    variant.key = key;
    VkSamplerAddressMode modes[3] = {
        VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT
    };
    const uint32_t d3dModes[2] = { key & 0x7u, (key >> 3) & 0x7u };
    for (int i = 0; i < 2; ++i) {
        switch (d3dModes[i]) {
        case 2: modes[i] = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT; break;  // D3DTADDRESS_MIRROR
        case 3: modes[i] = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; break;    // D3DTADDRESS_CLAMP
        case 4: modes[i] = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER; break;  // D3DTADDRESS_BORDER
        case 5: modes[i] = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE; break; // MIRRORONCE
        default: break;                                                     // WRAP / unset
        }
    }
    const uint32_t minFilter = (key >> 6) & 0x7u;   // D3DTEXF_POINT = 1, D3DTEXF_LINEAR = 2
    const uint32_t magFilter = (key >> 9) & 0x7u;
    // How many mip levels actually carry data. A level that was never copied is still in layout
    // UNDEFINED, and a sampler allowed to reach it reads garbage: the terrain atlas has three
    // levels, only the first arrived, and sampling mips 1 and 2 turned the whole ground into one
    // flat colour. The sampler must never be allowed past the last level that has been uploaded.
    uint32_t validLevels = 0;
    for (uint32_t level = 0; level < tex->levels; ++level) {
        if ((tex->levelValidMask & (1u << level)) != 0) ++validLevels;
        else break;
    }
    const bool mipsReady = (key & PLATFORM_SAMPLER_MIPS_READY) != 0 && validLevels > 1;
    VkSamplerCreateInfo ci = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    ci.magFilter = (magFilter == 1u) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
    if (mipsReady) {
        ci.minFilter = (minFilter == 1u) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
        ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        ci.maxLod = (float)(validLevels - 1);
    } else {
        // Without a complete chain, filtering across levels samples whatever the unwritten levels
        // hold. Minification then aliases, which is what the shimmer was.
        ci.minFilter = VK_FILTER_LINEAR;
        ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        ci.maxLod = 0.0f;
    }
    ci.addressModeU = modes[0];
    ci.addressModeV = modes[1];
    ci.addressModeW = modes[2];
    if (vkCreateSampler(device, &ci, nullptr, &variant.sampler) != VK_SUCCESS) return nullptr;

    VkDescriptorSetAllocateInfo sa = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    sa.descriptorSetCount = 4;
    sa.descriptorPool = s_descriptorPool;
    VkDescriptorSetLayout layouts[4] = { s_descriptorSetLayout, s_descriptorSetLayout,
                                         s_descriptorSetLayout, s_descriptorSetLayout };
    sa.pSetLayouts = layouts;
    if (vkAllocateDescriptorSets(device, &sa, variant.sets) != VK_SUCCESS) {
        fprintf(stderr, "descriptor pool exhausted for texture %p\n", (void*)tex);
        vkDestroySampler(device, variant.sampler, nullptr);
        return nullptr;
    }
    VkDescriptorImageInfo imageInfo = { variant.sampler, tex->view,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkWriteDescriptorSet writes[4] = {};
    for (int i = 0; i < 4; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = variant.sets[i];
        writes[i].dstBinding = 0;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].pImageInfo = &imageInfo;
    }
    vkUpdateDescriptorSets(device, 4, writes, 0, nullptr);
    tex->variants.push_back(variant);
    return &tex->variants.back();
}

// A texture destroyed while frames are in flight cannot be freed yet: some queued command buffer
// may still sample it. An image that was acquired again after the frame that used the texture
// proves that use finished (a re-acquired image had its present complete), so the drain waits for
// exactly that, or for every image to rotate in the unbound case.
static void drain_pending_texture_frees(uint32_t acquiredImage, uint32_t currentFrame)
{
    if (s_pendingTexFrees.empty() || !s_ctx.isInitialized) return;
    VkDevice device = (VkDevice)s_ctx.device;
    size_t write = 0;
    for (size_t i = 0; i < s_pendingTexFrees.size(); ++i) {
        const PendingTexFree &e = s_pendingTexFrees[i];
        bool safe = false;
        if (e.frame >= currentFrame) safe = false;
        else if (e.image != UINT32_MAX) safe = (e.image == acquiredImage);
        else {
            safe = true;
            for (int32_t last : s_lastAcquiredFrame) {
                if (last <= (int32_t)e.frame) { safe = false; break; }
            }
        }
        if (safe) free_texture_objects(device, e.tex);
        else s_pendingTexFrees[write++] = e;
    }
    s_pendingTexFrees.resize(write);
}

static void destroy_depth(VkDevice device)
{
    if (s_depthView) vkDestroyImageView(device, s_depthView, nullptr);
    if (s_depthImage) vkDestroyImage(device, s_depthImage, nullptr);
    if (s_depthMemory) vkFreeMemory(device, s_depthMemory, nullptr);
    s_depthView = VK_NULL_HANDLE;
    s_depthImage = VK_NULL_HANDLE;
    s_depthMemory = VK_NULL_HANDLE;
}

static bool create_depth(VkDevice device, VkPhysicalDevice phys, uint32_t width, uint32_t height)
{
    destroy_depth(device);
    const VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM
    };
    VkFormat format = VK_FORMAT_UNDEFINED;
    for (VkFormat candidate : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(phys, candidate, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            format = candidate;
            break;
        }
    }
    if (format == VK_FORMAT_UNDEFINED || width == 0 || height == 0) return false;
    s_depthFormat = format;

    VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = { width, height, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(device, &imageInfo, nullptr, &s_depthImage) != VK_SUCCESS) return false;

    VkMemoryRequirements reqs;
    vkGetImageMemoryRequirements(device, s_depthImage, &reqs);
    VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    allocInfo.allocationSize = reqs.size;
    allocInfo.memoryTypeIndex = find_memory_type(phys, reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (allocInfo.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(device, &allocInfo, nullptr, &s_depthMemory) != VK_SUCCESS ||
        vkBindImageMemory(device, s_depthImage, s_depthMemory, 0) != VK_SUCCESS) {
        destroy_depth(device);
        return false;
    }

    VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.image = s_depthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D16_UNORM_S8_UINT)
        viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(device, &viewInfo, nullptr, &s_depthView) != VK_SUCCESS) {
        destroy_depth(device);
        return false;
    }
    return true;
}

static bool create_framebuffers(VkDevice device, uint32_t width, uint32_t height)
{
    s_framebuffers.resize(s_swapchainImageViews.size());
    for (uint32_t i = 0; i < s_swapchainImageViews.size(); ++i) {
        VkImageView attachments[2] = { s_swapchainImageViews[i], s_depthView };
        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = s_renderPass;
        fbInfo.attachmentCount = 2;
        fbInfo.pAttachments = attachments;
        fbInfo.width = width;
        fbInfo.height = height;
        fbInfo.layers = 1;
        if (vkCreateFramebuffer(device, &fbInfo, nullptr, &s_framebuffers[i]) != VK_SUCCESS)
            return false;
    }
    return true;
}

// The shaders are embedded SPIR-V generated from vk_canonical.vert/.frag by regen_vk_shaders.sh:
// the game links no shader compiler, and a shader that failed to load at run time would be a black
// screen instead of a build error. The vertex program is the one layout every FVF decodes into; the
// fragment program evaluates the four fixed-function texture stages and the alpha test.
#include "vk_canonical_shaders.h"

PlatformVulkanContext *Platform_GetVulkanContext(void)
{
    return &s_ctx;
}
static VkPresentModeKHR select_present_mode(VkPhysicalDevice phys, VkSurfaceKHR surface) {
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &count, nullptr);
    std::vector<VkPresentModeKHR> modes;
    if (count) {
        modes.resize(count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &count, modes.data());
    }
    const std::vector<VkPresentModeKHR> &available = modes;
    auto has = [&available](VkPresentModeKHR m) {
        for (auto candidate : available) if (candidate == m) return true;
        return false;
    };
    if (has(VK_PRESENT_MODE_MAILBOX_KHR)) return VK_PRESENT_MODE_MAILBOX_KHR;
    // Immediate when there is no mailbox. On this Xwayland surface the alternatives are FIFO and
    // FIFO_LATEST_READY, and with either of them the window never shows a frame - the driver spins
    // inside present instead (measured: 100% CPU, black window, no error). Immediate presents the
    // frame and costs a queue-full stall only when the compositor is behind, which is a stall the
    // loop survives.
    if (has(VK_PRESENT_MODE_IMMEDIATE_KHR)) return VK_PRESENT_MODE_IMMEDIATE_KHR;
    return VK_PRESENT_MODE_FIFO_KHR;
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
        fprintf(stderr, "INITFAIL instance\n");
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
        fprintf(stderr, "INITFAIL xlib surface win=0x%lx\n", (unsigned long)window);
        vkDestroyInstance(instance, nullptr);
        return false;
    }
    s_ctx.surface = surface;

    // 3. Physical Device
    uint32_t gpuCount = 0;
    vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr);
    if (gpuCount == 0) {
        fprintf(stderr, "INITFAIL no gpus\n");
        return false;
    }
    std::vector<VkPhysicalDevice> gpus(gpuCount);
    vkEnumeratePhysicalDevices(instance, &gpuCount, gpus.data());

    // 3. Physical Device. Prefer a discrete GPU that can present on the surface:
    // multi-GPU systems may list a headless discrete GPU first, and that one has
    // no present-capable queue family, which would fail the swapchain below.
    VkPhysicalDevice selectedGpu = VK_NULL_HANDLE;
    for (int pass = 0; pass < 2 && !selectedGpu; ++pass) {
        for (auto gpu : gpus) {
            if (pass == 0) {
                VkPhysicalDeviceProperties props;
                vkGetPhysicalDeviceProperties(gpu, &props);
                if (props.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) continue;
            }
            uint32_t qn = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &qn, nullptr);
            for (uint32_t i = 0; i < qn; ++i) {
                VkBool32 presentSupport = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface, &presentSupport);
                if (presentSupport) {
                    selectedGpu = gpu;
                    break;
                }
            }
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
    if (!foundGraphics || !foundPresent) {
        fprintf(stderr, "INITFAIL queues gfx=%d present=%d\n", (int)foundGraphics, (int)foundPresent);
        return false;
    }

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

    bool extendedDynamic = false;
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(selectedGpu, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> deviceExts(extCount);
    if (extCount)
        vkEnumerateDeviceExtensionProperties(selectedGpu, nullptr, &extCount, deviceExts.data());
    for (const auto &ext : deviceExts) {
        if (strcmp(ext.extensionName, "VK_EXT_extended_dynamic_state") == 0)
            extendedDynamic = true;
    }
    const char *devExtensions[2] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_EXT_extended_dynamic_state" };
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT dynFeatures = {};
    dynFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;
    dynFeatures.extendedDynamicState = VK_TRUE;
    VkPhysicalDeviceFeatures supportedFeatures = {};
    vkGetPhysicalDeviceFeatures(selectedGpu, &supportedFeatures);
    VkPhysicalDeviceFeatures enabledFeatures = {};
    if (supportedFeatures.textureCompressionBC) {
        enabledFeatures.textureCompressionBC = VK_TRUE;
    }
    VkDeviceCreateInfo devInfo = {};
    devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    devInfo.pNext = extendedDynamic ? &dynFeatures : nullptr;
    devInfo.queueCreateInfoCount = (uint32_t)queueInfos.size();
    devInfo.pQueueCreateInfos = queueInfos.data();
    devInfo.enabledExtensionCount = extendedDynamic ? 2u : 1u;
    devInfo.ppEnabledExtensionNames = devExtensions;
    devInfo.pEnabledFeatures = &enabledFeatures;
    VkDevice device = VK_NULL_HANDLE;
    if (vkCreateDevice(selectedGpu, &devInfo, nullptr, &device) != VK_SUCCESS) {
        fprintf(stderr, "INITFAIL device\n");
        return false;
    }
    s_ctx.device = device;
    vkGetDeviceQueue(device, s_graphicsQueueFamily, 0, &s_graphicsQueue);
    vkGetDeviceQueue(device, s_presentQueueFamily, 0, &s_presentQueue);
    if (extendedDynamic) {
        s_cmdSetDepthTest = (PFN_vkCmdSetDepthTestEnable)vkGetDeviceProcAddr(device, "vkCmdSetDepthTestEnable");
        s_cmdSetDepthWrite = (PFN_vkCmdSetDepthWriteEnable)vkGetDeviceProcAddr(device, "vkCmdSetDepthWriteEnable");
        s_cmdSetDepthCompare = (PFN_vkCmdSetDepthCompareOp)vkGetDeviceProcAddr(device, "vkCmdSetDepthCompareOp");
        if (!s_cmdSetDepthTest)
            s_cmdSetDepthTest = (PFN_vkCmdSetDepthTestEnable)vkGetDeviceProcAddr(device, "vkCmdSetDepthTestEnableEXT");
        if (!s_cmdSetDepthWrite)
            s_cmdSetDepthWrite = (PFN_vkCmdSetDepthWriteEnable)vkGetDeviceProcAddr(device, "vkCmdSetDepthWriteEnableEXT");
        if (!s_cmdSetDepthCompare)
            s_cmdSetDepthCompare = (PFN_vkCmdSetDepthCompareOp)vkGetDeviceProcAddr(device, "vkCmdSetDepthCompareOpEXT");
        s_dynamicDepth = s_cmdSetDepthTest && s_cmdSetDepthWrite && s_cmdSetDepthCompare;
    }

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
    if (extent.width == 0 || extent.height == 0) {
        fprintf(stderr, "INITFAIL zero extent\n");
        return false;
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
    if (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
        scInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    scInfo.preTransform = caps.currentTransform;
    scInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    scInfo.presentMode = select_present_mode(selectedGpu, surface);
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
    VkResult scRes = vkCreateSwapchainKHR(device, &scInfo, nullptr, &swapchain);
    if (scRes != VK_SUCCESS) {
        fprintf(stderr, "INITFAIL swapchain res=%d extent=%ux%u fmt=%d\n",
                (int)scRes, (int)extent.width, (int)extent.height, (int)s_swapchainFormat);
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

    if (!create_depth(device, selectedGpu, extent.width, extent.height)) {
        fprintf(stderr, "INITFAIL depth\n");
        return false;
    }

    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = s_swapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = s_depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference depthRef = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[2] = { colorAttachment, depthAttachment };
    VkRenderPassCreateInfo rpInfo = {};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 2;
    rpInfo.pAttachments = attachments;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &rpInfo, nullptr, &s_renderPass) != VK_SUCCESS) {
        return false;
    }

    s_framebuffers.resize(scImageCount);
    for (uint32_t i = 0; i < scImageCount; ++i) {
        VkImageView fbAttachments[2] = { s_swapchainImageViews[i], s_depthView };
        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = s_renderPass;
        fbInfo.attachmentCount = 2;
        fbInfo.pAttachments = fbAttachments;
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

    // 12. Semaphores & fences, one set per swapchain image.
    if (!create_sync_objects(device, scImageCount)) return false;
    s_presentMode = scInfo.presentMode;

    // One-line init report. If the name says llvmpipe/lavapipe, every performance number below
    // is a software-renderer number and pinning the real ICD matters more than any of it.
    {
        VkPhysicalDeviceProperties gpuProps;
        vkGetPhysicalDeviceProperties(selectedGpu, &gpuProps);
        fprintf(stderr, "VULKAN gpu=%s present=%s dynamic=%d images=%u\n",
                gpuProps.deviceName,
                s_presentMode == VK_PRESENT_MODE_MAILBOX_KHR ? "mailbox" :
                s_presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR ? "immediate" :
                s_presentMode == VK_PRESENT_MODE_FIFO_KHR ? "fifo" : "other",
                (int)s_dynamicDepth, scImageCount);
        fflush(stderr);
    }

    s_ctx.isInitialized = true;
    Platform_CreateVulkanPipeline();
    return true;
}
static bool recreate_swapchain(void)
{
    if (!s_ctx.isInitialized) return false;
    VkDevice device = (VkDevice)s_ctx.device;
    VkPhysicalDevice selectedGpu = (VkPhysicalDevice)s_ctx.physicalDevice;
    VkSurfaceKHR surface = (VkSurfaceKHR)s_ctx.surface;
    PlatformWindowInfo *winInfo = Platform_GetWindowInfo();
    if (!winInfo || winInfo->width <= 0 || winInfo->height <= 0) return false;

    vkDeviceWaitIdle(device);

    VkSurfaceCapabilitiesKHR caps;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(selectedGpu, surface, &caps) != VK_SUCCESS) {
        fprintf(stderr, "RECREATE fail: caps query failed\n");
        return false;
    }

    VkExtent2D extent = { (uint32_t)winInfo->width, (uint32_t)winInfo->height };
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    }
    if (extent.width == 0 || extent.height == 0) {
        fprintf(stderr, "RECREATE fail: extent %ux%u win=%ux%u\n",
                (int)extent.width, (int)extent.height, (int)winInfo->width, (int)winInfo->height);
        return false;
    }
    fprintf(stderr, "RECREATE swapchain %ux%u -> %ux%u\n",
            (int)s_ctx.width, (int)s_ctx.height, (int)extent.width, (int)extent.height);

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR scInfo = {};
    scInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    scInfo.surface = surface;
    scInfo.minImageCount = imageCount;
    scInfo.imageFormat = s_swapchainFormat;
    scInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    scInfo.imageExtent = extent;
    scInfo.imageArrayLayers = 1;
    scInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
        scInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    scInfo.preTransform = caps.currentTransform;
    scInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    scInfo.presentMode = select_present_mode((VkPhysicalDevice)s_ctx.physicalDevice, surface);
    scInfo.clipped = VK_TRUE;
    scInfo.oldSwapchain = (VkSwapchainKHR)s_ctx.swapchain;

    uint32_t qfIndices[] = { s_graphicsQueueFamily, s_presentQueueFamily };
    if (s_graphicsQueueFamily != s_presentQueueFamily) {
        scInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        scInfo.queueFamilyIndexCount = 2;
        scInfo.pQueueFamilyIndices = qfIndices;
    } else {
        scInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(device, &scInfo, nullptr, &newSwapchain) != VK_SUCCESS) {
        return false;
    }

    for (auto fb : s_framebuffers) if (fb) vkDestroyFramebuffer(device, fb, nullptr);
    s_framebuffers.clear();

    for (auto iv : s_swapchainImageViews) if (iv) vkDestroyImageView(device, iv, nullptr);
    s_swapchainImageViews.clear();

    vkDestroySwapchainKHR(device, (VkSwapchainKHR)s_ctx.swapchain, nullptr);
    s_ctx.swapchain = newSwapchain;
    s_ctx.width = extent.width;
    s_ctx.height = extent.height;

    uint32_t scImageCount = 0;
    vkGetSwapchainImagesKHR(device, newSwapchain, &scImageCount, nullptr);
    s_swapchainImages.resize(scImageCount);
    vkGetSwapchainImagesKHR(device, newSwapchain, &scImageCount, s_swapchainImages.data());
    s_ctx.imageCount = scImageCount;

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

    if (!create_depth(device, (VkPhysicalDevice)s_ctx.physicalDevice, extent.width, extent.height)) {
        fprintf(stderr, "RECREATE FAIL: depth %ux%u\n", (int)extent.width, (int)extent.height);
        return false;
    }

    s_framebuffers.resize(scImageCount);
    for (uint32_t i = 0; i < scImageCount; ++i) {
        VkImageView fbAttachments[2] = { s_swapchainImageViews[i], s_depthView };
        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = s_renderPass;
        fbInfo.attachmentCount = 2;
        fbInfo.pAttachments = fbAttachments;
        fbInfo.width = extent.width;
        fbInfo.height = extent.height;
        fbInfo.layers = 1;
        if (vkCreateFramebuffer(device, &fbInfo, nullptr, &s_framebuffers[i]) != VK_SUCCESS)
            return false;
    }
    // The fence/semaphore arrays are indexed by image, so a new imageCount means rebuilding them.
    // Safe here: recreate_swapchain starts with a device-wide WaitIdle.
    destroy_sync_objects(device);
    if (!create_sync_objects(device, scImageCount)) return false;
    s_presentMode = scInfo.presentMode;
    layout_ring_windows();
    return true;
}

bool Platform_BeginVulkanFrame(uint32_t *imageIndex)
{
    if (!s_ctx.isInitialized) return false;

    PlatformWindowInfo *winInfo = Platform_GetWindowInfo();
    if (winInfo && (winInfo->width != s_ctx.width || winInfo->height != s_ctx.height)) {
        if (!recreate_swapchain()) return false;
    }

    VkDevice device = (VkDevice)s_ctx.device;

    // Ring growth is decided from what the previous frame actually used, and happens only here,
    // at a frame boundary: a WaitIdle once per doubling beats an overflow that silently drops
    // every draw after it, which is exactly what the fixed 32 MB vertex ring did.
    VkDeviceSize pendingTotal = 0;
    for (auto &upd : s_pendingTexUpdates) pendingTotal += align_up((VkDeviceSize)upd.bytes.size(), STAGING_ALIGN);
    {
        VkDeviceSize needed = s_stagingTotal;
        uint32_t n = s_ctx.imageCount ? s_ctx.imageCount : 1;
        if (pendingTotal * n > needed) needed = pendingTotal * n;
        if (!ensure_staging(device, needed)) return false;
    }
    if (s_ringOverflow) {
        if (!ensure_dynamic(device, s_vbTotal * 2, s_ibTotal * 2)) return false;
        s_ringOverflow = false;
    }
    layout_ring_windows();

    // One frame slot owns one fence, one acquire semaphore and one ring window. Waiting on the
    // slot's fence before acquiring is what makes re-signalling the acquire semaphore legal: the
    // submit that waited on it has finished, so it carries no pending signal.
    const uint32_t slots = s_ctx.imageCount ? s_ctx.imageCount : 1u;
    const uint32_t slot = s_frameSlot % slots;
    vkWaitForFences(device, 1, &s_inFlightFences[slot], VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &s_inFlightFences[slot]);

    VkResult res = vkAcquireNextImageKHR(device, (VkSwapchainKHR)s_ctx.swapchain,
                                         UINT64_MAX, s_imageAvailableSemaphores[slot],
                                         VK_NULL_HANDLE, imageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        if (!recreate_swapchain()) return false;
        res = vkAcquireNextImageKHR(device, (VkSwapchainKHR)s_ctx.swapchain,
                                    UINT64_MAX, s_imageAvailableSemaphores[slot],
                                    VK_NULL_HANDLE, imageIndex);
    }
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        return false;
    }

    s_frameSlot = slot + 1;
    s_currentSlot = slot;
    s_ctx.currentImageIndex = *imageIndex;
    s_currentImage = *imageIndex;
    s_inFrame = true;
    ++s_frameCounter;
    s_lastAcquiredFrame[*imageIndex] = (int32_t)s_frameCounter;
    drain_pending_texture_frees(*imageIndex, s_frameCounter);

    s_currentVBOffset = (VkDeviceSize)slot * s_vbWindow;
    s_currentIBOffset = (VkDeviceSize)slot * s_ibWindow;
    s_stagingCursor = (VkDeviceSize)slot * s_stagingWindow;
    s_lastPipeline = VK_NULL_HANDLE;
    s_dynViewportScissor = false;
    s_lastDepthKey = -1;
    s_lastDescSet = VK_NULL_HANDLE;

    VkCommandBuffer cb = s_commandBuffers[*imageIndex];
    vkResetCommandBuffer(cb, 0);

    VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(cb, &beginInfo);

    // Texture copies queued during the previous frames run here, before the render pass opens, so
    // the frame samples the new texels and the CPU never waited for the queue to do it.
    flush_pending_texture_copies(cb);

    VkRenderPassBeginInfo rpBegin = {};
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass = s_renderPass;
    rpBegin.framebuffer = s_framebuffers[*imageIndex];
    rpBegin.renderArea.offset = { 0, 0 };
    rpBegin.renderArea.extent = { (uint32_t)s_ctx.width, (uint32_t)s_ctx.height };
    VkClearValue clears[2];
    clears[0] = s_clearColor;
    clears[1].depthStencil = { 1.0f, 0 };
    rpBegin.clearValueCount = 2;
    rpBegin.pClearValues = clears;

    vkCmdBeginRenderPass(cb, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
    return true;
}

bool Platform_EndVulkanFrame(uint32_t imageIndex)
{
    if (!s_ctx.isInitialized) return false;
    s_inFrame = false;

    VkCommandBuffer cb = s_commandBuffers[imageIndex];

    vkCmdEndRenderPass(cb);

    vkEndCommandBuffer(cb);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { s_imageAvailableSemaphores[s_currentSlot] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cb;

    VkSemaphore signalSemaphores[] = { s_renderFinishedSemaphores[imageIndex] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(s_graphicsQueue, 1, &submitInfo, s_inFlightFences[s_currentSlot]) != VK_SUCCESS) {
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
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        recreate_swapchain();
    }
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
    if (!s_ctx.isInitialized || s_graphicsPipelineOpaque != VK_NULL_HANDLE) {
        return true;
    }
    VkDevice device = (VkDevice)s_ctx.device;
    VkPhysicalDevice phys_dev = (VkPhysicalDevice)s_ctx.physicalDevice;

    // 1. Shaders
    VkShaderModuleCreateInfo vert_ci = {};
    vert_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vert_ci.codeSize = sizeof(s_canonicalVertexSPIRV);
    vert_ci.pCode = s_canonicalVertexSPIRV;
    VkShaderModule vert_mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &vert_ci, nullptr, &vert_mod) != VK_SUCCESS) return false;

    VkShaderModuleCreateInfo frag_ci = {};
    frag_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    frag_ci.codeSize = sizeof(s_canonicalFragmentSPIRV);
    frag_ci.pCode = s_canonicalFragmentSPIRV;
    VkShaderModule frag_mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &frag_ci, nullptr, &frag_mod) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vert_mod, nullptr);
        return false;
    }

    // 2. Vertex Input State: the one layout the pipeline accepts. Position at 0, one texture
    // coordinate set at 12, colour at 20. Every FVF the engine uses is decoded into it before the
    // draw, so the pipeline never has to be rebuilt per material.
    VkVertexInputBindingDescription bind_desc = {};
    bind_desc.binding = 0;
    bind_desc.stride = PLATFORM_CANONICAL_VERTEX_STRIDE;
    bind_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    // Position, texture coordinate set 0, colour, texture coordinate set 1, and the two projected
    // coordinates the host computed for camera-space stages. Offsets match VKCanonicalVertex.
    VkVertexInputAttributeDescription attr_descs[6] = {};
    attr_descs[0].location = 0;
    attr_descs[0].binding = 0;
    attr_descs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attr_descs[0].offset = 0;

    attr_descs[1].location = 1;
    attr_descs[1].binding = 0;
    attr_descs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attr_descs[1].offset = 12;

    attr_descs[2].location = 2;
    attr_descs[2].binding = 0;
    attr_descs[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attr_descs[2].offset = 20;

    attr_descs[3].location = 3;
    attr_descs[3].binding = 0;
    attr_descs[3].format = VK_FORMAT_R32G32_SFLOAT;
    attr_descs[3].offset = 36;

    attr_descs[4].location = 4;
    attr_descs[4].binding = 0;
    attr_descs[4].format = VK_FORMAT_R32G32_SFLOAT;
    attr_descs[4].offset = 44;

    attr_descs[5].location = 5;
    attr_descs[5].binding = 0;
    attr_descs[5].format = VK_FORMAT_R32G32_SFLOAT;
    attr_descs[5].offset = 52;

    VkPipelineVertexInputStateCreateInfo vi_ci = {};
    vi_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi_ci.vertexBindingDescriptionCount = 1;
    vi_ci.pVertexBindingDescriptions = &bind_desc;
    vi_ci.vertexAttributeDescriptionCount = 6;
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

    VkPipelineDepthStencilStateCreateInfo ds_ci = {};
    ds_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds_ci.depthTestEnable = VK_TRUE;
    ds_ci.depthWriteEnable = VK_TRUE;
    ds_ci.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    // 3. Descriptor set: one combined image sampler. Each texture gets its own set at creation.
    // Writing a set that a submitted command buffer is still reading is undefined, so the draw
    // binds a set that was written once, instead of rewriting one shared set.
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo dsl_ci = {};
    dsl_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsl_ci.bindingCount = 1;
    dsl_ci.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(device, &dsl_ci, nullptr, &s_descriptorSetLayout) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vert_mod, nullptr);
        vkDestroyShaderModule(device, frag_mod, nullptr);
        return false;
    }

    // Every texture holds four sets per sampler state it is drawn with, so the pool is sized for
    // the whole texture set of a large map plus its water and fog variants.
    // Four sets per (texture, sampler state). A large map plus its shell scene and the control bar
    // runs past the old 32k-set ceiling, and a texture that cannot get a set samples the white
    // dummy - which is what turned the terrain flat white while its image was correct in VRAM.
    VkDescriptorPoolSize pool_size = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 524288 };
    VkDescriptorPoolCreateInfo dp_ci = {};
    dp_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dp_ci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    dp_ci.maxSets = 131072;
    dp_ci.poolSizeCount = 1;
    dp_ci.pPoolSizes = &pool_size;
    if (vkCreateDescriptorPool(device, &dp_ci, nullptr, &s_descriptorPool) != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(device, s_descriptorSetLayout, nullptr);
        vkDestroyShaderModule(device, vert_mod, nullptr);
        vkDestroyShaderModule(device, frag_mod, nullptr);
        return false;
    }

    VkPushConstantRange pc_range = {};
    pc_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pc_range.offset = 0;
    pc_range.size = sizeof(PlatformVulkanPushConstants);

    VkPipelineLayoutCreateInfo layout_ci = {};
    layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    // One single-sampler set per texture stage: set 0 is stage 0, set 3 is stage 3. A draw binds
    // four sets in one call, each one written once when that texture met that sampler state.
    VkDescriptorSetLayout setLayouts[4] = { s_descriptorSetLayout, s_descriptorSetLayout,
                                            s_descriptorSetLayout, s_descriptorSetLayout };
    layout_ci.setLayoutCount = 4;
    layout_ci.pSetLayouts = setLayouts;
    layout_ci.pushConstantRangeCount = 1;
    layout_ci.pPushConstantRanges = &pc_range;
    if (vkCreatePipelineLayout(device, &layout_ci, nullptr, &s_pipelineLayout) != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(device, s_descriptorSetLayout, nullptr);
        vkDestroyDescriptorPool(device, s_descriptorPool, nullptr);
        vkDestroyShaderModule(device, vert_mod, nullptr);
        vkDestroyShaderModule(device, frag_mod, nullptr);
        return false;
    }

    // A white 1x1 texture, so a draw with nothing bound gives what the fixed-function path gives
    // when the texture stage is off: vertex colour times material colour.
    static const uint8_t kWhitePixel[4] = { 255, 255, 255, 255 };
    s_defaultTexture = (PlatformVulkanTexture *)Platform_CreateVulkanTexture(1, 1, kWhitePixel, 0);

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert_mod;
    stages[0].pName = "main";

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag_mod;
    stages[1].pName = "main";
    VkPipelineMultisampleStateCreateInfo ms_ci = {};
    ms_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms_ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cb_att = {};
    cb_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cb_att.blendEnable = VK_FALSE;
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

    VkDynamicState dyn_states[5] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
        VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
        VK_DYNAMIC_STATE_DEPTH_COMPARE_OP
    };
    VkPipelineDynamicStateCreateInfo dyn_ci = {};
    dyn_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn_ci.dynamicStateCount = s_dynamicDepth ? 5u : 2u;
    dyn_ci.pDynamicStates = dyn_states;

    VkGraphicsPipelineCreateInfo pipe_ci = {};
    pipe_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipe_ci.stageCount = 2;
    pipe_ci.pStages = stages;
    pipe_ci.pVertexInputState = &vi_ci;
    pipe_ci.pInputAssemblyState = &ia_ci;
    pipe_ci.pViewportState = &vp_ci;   // required even with the viewport and scissor dynamic
    pipe_ci.pRasterizationState = &rast_ci;
    pipe_ci.pDepthStencilState = &ds_ci;
    pipe_ci.pMultisampleState = &ms_ci;
    pipe_ci.pColorBlendState = &cb_ci;
    pipe_ci.pDynamicState = &dyn_ci;
    pipe_ci.layout = s_pipelineLayout;
    pipe_ci.renderPass = s_renderPass;
    pipe_ci.subpass = 0;

    VkResult pRes = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipe_ci, nullptr, &s_graphicsPipelineOpaque);
    if (pRes != VK_SUCCESS) {
        vkDestroyShaderModule(device, vert_mod, nullptr);
        vkDestroyShaderModule(device, frag_mod, nullptr);
        return false;
    }
    // 2. Alpha blend (SRC_ALPHA, ONE_MINUS_SRC_ALPHA)
    cb_att.blendEnable = VK_TRUE;
    cb_att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cb_att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cb_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cb_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    pRes = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipe_ci, nullptr, &s_graphicsPipelineBlend);
    if (pRes != VK_SUCCESS) {
        vkDestroyShaderModule(device, vert_mod, nullptr);
        vkDestroyShaderModule(device, frag_mod, nullptr);
        return false;
    }

    // 3. Multiplicative blend (DST_COLOR, ZERO) - for projected shadows
    cb_att.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
    cb_att.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    cb_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    cb_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    pRes = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipe_ci, nullptr, &s_graphicsPipelineMultiply);
    if (pRes != VK_SUCCESS) {
        vkDestroyShaderModule(device, vert_mod, nullptr);
        vkDestroyShaderModule(device, frag_mod, nullptr);
        return false;
    }

    // 4. Additive blend (ONE, ONE) - for fire, glow
    cb_att.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    cb_att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    cb_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cb_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    pRes = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipe_ci, nullptr, &s_graphicsPipelineAdditive);
    vkDestroyShaderModule(device, vert_mod, nullptr);
    vkDestroyShaderModule(device, frag_mod, nullptr);
    if (pRes != VK_SUCCESS) return false;

    // 3. Dynamic Buffers
    const VkDeviceSize vbSize = 32 * 1024 * 1024;
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
    const VkDeviceSize ibSize = 8 * 1024 * 1024;

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
    s_vbTotal = vbSize;
    s_ibTotal = ibSize;
    layout_ring_windows();
    return true;
}

void Platform_SetVulkanPushConstants(const PlatformVulkanPushConstants *pushConstants)
{
    if (pushConstants) s_pushConstants = *pushConstants;
}
void Platform_SetVulkanBlendState(int mode)
{
    s_blendMode = mode;
}

// The engine hands over decoded canonical vertices, so this is the only draw path.

bool Platform_DrawVulkanCanonical(const float *canonicalVertices, uint32_t vertexCount,
                                  const uint16_t *indices, uint32_t indexCount)
{
    if (!s_ctx.isInitialized || !s_graphicsPipelineOpaque || !canonicalVertices || vertexCount == 0) return false;
    if (!s_mappedVB) return false;

    const VkDeviceSize vbAlign = 256;
    VkDeviceSize alignedVBOffset = (s_currentVBOffset + (vbAlign - 1)) & ~(vbAlign - 1);
    VkDeviceSize vbBytes = (VkDeviceSize)vertexCount * PLATFORM_CANONICAL_VERTEX_STRIDE;
    const VkDeviceSize vbEnd = (VkDeviceSize)s_currentSlot * s_vbWindow + s_vbWindow;
    if (alignedVBOffset + vbBytes > vbEnd) { s_ringOverflow = true; return false; }

    memcpy((char *)s_mappedVB + alignedVBOffset, canonicalVertices, vbBytes);
    s_currentVBOffset = alignedVBOffset + vbBytes;

    VkCommandBuffer cb = s_commandBuffers[s_currentImage];
    VkPipeline pipe = s_graphicsPipelineOpaque;
    const int blendMode = s_blendMode;
    if (blendMode == 1) pipe = s_graphicsPipelineBlend;
    else if (blendMode == 2) pipe = s_graphicsPipelineMultiply;
    else if (blendMode == 3) pipe = s_graphicsPipelineAdditive;
    if (!pipe) pipe = s_graphicsPipelineOpaque;
    if (pipe != s_lastPipeline) {
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
        s_lastPipeline = pipe;
        // A pipeline bind invalidates the descriptor sets, so the next draw has to re-bind one.
        s_lastDescSet = VK_NULL_HANDLE;
    }

    // Viewport and scissor never move within a frame, and survive a pipeline bind because every
    // variant declares them dynamic; depth only moves when SetRenderState says so. One emission
    // per command buffer per state instead of one per draw call.
    if (!s_dynViewportScissor) {
        VkViewport vp = { 0.0f, 0.0f, (float)s_ctx.width, (float)s_ctx.height, 0.0f, 1.0f };
        VkRect2D scissor = { { 0, 0 }, { (uint32_t)s_ctx.width, (uint32_t)s_ctx.height } };
        vkCmdSetViewport(cb, 0, 1, &vp);
        vkCmdSetScissor(cb, 0, 1, &scissor);
        s_dynViewportScissor = true;
    }
    if (s_dynamicDepth) {
        const int key = (s_depthTest ? 1 : 0) | (s_depthWrite ? 2 : 0) | ((int)s_depthCompare << 2);
        if (key != s_lastDepthKey) {
            s_cmdSetDepthTest(cb, s_depthTest ? VK_TRUE : VK_FALSE);
            s_cmdSetDepthWrite(cb, s_depthWrite ? VK_TRUE : VK_FALSE);
            s_cmdSetDepthCompare(cb, s_depthCompare);
            s_lastDepthKey = key;
        }
    }

    vkCmdPushConstants(cb, s_pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PlatformVulkanPushConstants), &s_pushConstants);

    // Four stage sets in one bind. Stages with no texture bound fall back to the white dummy, which
    // is what the fixed-function pipeline sampled when a stage had no texture.
    VkDescriptorSet sets[4];
    bool setsChanged = false;
    for (int stage = 0; stage < 4; ++stage) {
        PlatformVulkanTexture *texture = s_boundTextures[stage] ? s_boundTextures[stage] : s_defaultTexture;
        // A texture whose first copy has not run yet is still in layout UNDEFINED: sampling it reads
        // whatever the allocator left there, which showed up as garbage stripes on the loading
        // screen and on any surface drawn in the frame its texture arrived. Draw the white dummy
        // for that one frame instead, which is what the old synchronous upload gave us.
        if (texture && texture->levelValidMask == 0) texture = s_defaultTexture;
        PlatformVulkanTexture::SamplerVariant *variant =
            texture ? texture_variant(texture, s_boundSamplerKeys[stage]) : nullptr;
        if (!variant && s_defaultTexture && texture != s_defaultTexture) {
            variant = texture_variant(s_defaultTexture, s_boundSamplerKeys[stage]);
        }
        sets[stage] = variant ? variant->sets[stage] : VK_NULL_HANDLE;
        if (sets[stage] != s_boundSets[stage]) setsChanged = true;
    }
    if (sets[0] && sets[1] && sets[2] && sets[3] && (setsChanged || s_lastDescSet == VK_NULL_HANDLE)) {
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, s_pipelineLayout, 0, 4, sets, 0, nullptr);
        for (int stage = 0; stage < 4; ++stage) s_boundSets[stage] = sets[stage];
        s_lastDescSet = sets[0];
    }

    VkDeviceSize offsets[] = { alignedVBOffset };
    vkCmdBindVertexBuffers(cb, 0, 1, &s_dynamicVB, offsets);

    if (indexCount > 0 && indices && s_mappedIB) {
        const VkDeviceSize ibAlign = 4;
        VkDeviceSize alignedIBOffset = (s_currentIBOffset + (ibAlign - 1)) & ~(ibAlign - 1);
        VkDeviceSize ibBytes = (VkDeviceSize)indexCount * sizeof(uint16_t);
        const VkDeviceSize ibEnd = (VkDeviceSize)s_currentSlot * s_ibWindow + s_ibWindow;
        if (alignedIBOffset + ibBytes > ibEnd) { s_ringOverflow = true; return false; }

        memcpy((char *)s_mappedIB + alignedIBOffset, indices, ibBytes);
        s_currentIBOffset = alignedIBOffset + ibBytes;

        vkCmdBindIndexBuffer(cb, s_dynamicIB, alignedIBOffset, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(cb, indexCount, 1, 0, 0, 0);
    } else {
        vkCmdDraw(cb, vertexCount, 1, 0, 0);
    }
    return true;
}

const uint32_t *Platform_GetVertexShaderSPIRV(uint32_t *wordCount)
{
    if (wordCount) *wordCount = (uint32_t)(sizeof(s_canonicalVertexSPIRV) / sizeof(uint32_t));
    return s_canonicalVertexSPIRV;
}

const uint32_t *Platform_GetFragmentShaderSPIRV(uint32_t *wordCount)
{
    if (wordCount) *wordCount = (uint32_t)(sizeof(s_canonicalFragmentSPIRV) / sizeof(uint32_t));
    return s_canonicalFragmentSPIRV;
}
// The engine fills a texture through LockRect and hands the bytes over at UnlockRect; the shim
// calls this with the dirty rectangle's pixels already converted to the image format. The copy
// rides the staging ring and runs at the next frame start, before the render pass opens - the
// caller never waits on the queue, which is what made every fog update cost a full queue drain.
bool Platform_UpdateVulkanTexture(void *texture, int x, int y, int w, int h, const void *pixels)
{
    return Platform_UpdateVulkanTextureLevel(texture, 0u, x, y, w, h, pixels);
}

bool Platform_UpdateVulkanTextureLevel(void *texture, uint32_t level, int x, int y, int w, int h,
                                       const void *pixels)
{
    if (!texture || !pixels || !s_ctx.isInitialized) return false;
    PlatformVulkanTexture *tex = (PlatformVulkanTexture *)texture;
    if (level >= tex->levels) return false;
    const int levelWidth = (int)(tex->width >> level ? tex->width >> level : 1);
    const int levelHeight = (int)(tex->height >> level ? tex->height >> level : 1);
    if (w <= 0 || h <= 0) return true;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= levelWidth || y >= levelHeight) return true;
    if (x + w > levelWidth) w = levelWidth - x;
    if (y + h > levelHeight) h = levelHeight - y;
    if (w <= 0 || h <= 0) return true;
    if (tex->blockDim > 1) {
        x = (x / (int)tex->blockDim) * (int)tex->blockDim;
        y = (y / (int)tex->blockDim) * (int)tex->blockDim;
        w = ((w + (int)tex->blockDim - 1) / (int)tex->blockDim) * (int)tex->blockDim;
        h = ((h + (int)tex->blockDim - 1) / (int)tex->blockDim) * (int)tex->blockDim;
    }
    PendingTexUpdate upd;
    upd.tex = tex; upd.level = level; upd.x = x; upd.y = y; upd.w = w; upd.h = h;
    const size_t blocksWide = (size_t)(w + tex->blockDim - 1) / tex->blockDim;
    const size_t blocksHigh = (size_t)(h + tex->blockDim - 1) / tex->blockDim;
    upd.bytes.resize(blocksWide * blocksHigh * tex->blockBytes);
    memcpy(upd.bytes.data(), pixels, upd.bytes.size());
    s_pendingTexUpdates.push_back(std::move(upd));
    // Outside a frame the game is loading or idling: drain right here so the next Present sees
    // fresh texels, matching the old synchronous create behaviour on those paths.
    if (!s_inFrame) flush_texture_updates_idle();
    return true;
}

void *Platform_CreateVulkanTextureEx(uint32_t width, uint32_t height, uint32_t levels,
                                     const void *pixels, uint32_t format)
{
    if (!s_ctx.isInitialized || width == 0 || height == 0) return nullptr;
    if (levels == 0) levels = 1;
    uint32_t maxLevels = 1;
    for (uint32_t extent = width > height ? width : height; extent > 1; extent >>= 1) ++maxLevels;
    if (levels > maxLevels) levels = maxLevels;

    VkDevice device = (VkDevice)s_ctx.device;
    VkPhysicalDevice phys_dev = (VkPhysicalDevice)s_ctx.physicalDevice;

    PlatformVulkanTexture *tex = new PlatformVulkanTexture();
    tex->width = width;
    tex->height = height;
    tex->levels = levels;
    tex->levelValidMask = 0;
    tex->levelsInitialised = false;

    VkFormat vkFmt = VK_FORMAT_B8G8R8A8_UNORM;
    VkDeviceSize uploadSz = (VkDeviceSize)width * height * 4;
    tex->blockDim = 1;
    tex->blockBytes = 4;
    if (format == 0x31545844) { // D3DFMT_DXT1
        vkFmt = VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        uploadSz = (VkDeviceSize)((width + 3) / 4) * ((height + 3) / 4) * 8;
        tex->blockDim = 4;
        tex->blockBytes = 8;
    } else if (format == 0x32545844 || format == 0x33545844) { // D3DFMT_DXT2 / D3DFMT_DXT3
        vkFmt = VK_FORMAT_BC2_UNORM_BLOCK;
        uploadSz = (VkDeviceSize)((width + 3) / 4) * ((height + 3) / 4) * 16;
        tex->blockDim = 4;
        tex->blockBytes = 16;
    } else if (format == 0x34545844 || format == 0x35545844) { // D3DFMT_DXT4 / D3DFMT_DXT5
        vkFmt = VK_FORMAT_BC3_UNORM_BLOCK;
        uploadSz = (VkDeviceSize)((width + 3) / 4) * ((height + 3) / 4) * 16;
        tex->blockDim = 4;
        tex->blockBytes = 16;
    } else if (format == 23) {                   // D3DFMT_R5G6B5
        // Packed 16-bit formats go to the GPU untouched: Vulkan's bit layout is Direct3D's, and the
        // per-pixel conversion was both slow and lossy. A1R5G5B5 stays on the conversion path on
        // purpose - its single alpha bit is what the terrain blends with, and sampling it as a
        // native packed format made the ground render as white.
        vkFmt = VK_FORMAT_B5G6R5_UNORM_PACK16;
        uploadSz = (VkDeviceSize)width * height * 2;
        tex->blockBytes = 2;
    }
    tex->uploadSize = uploadSz;
    tex->vkFormat = (uint32_t)vkFmt;


    VkImageCreateInfo img_ci = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    img_ci.imageType = VK_IMAGE_TYPE_2D;
    img_ci.format = vkFmt;
    img_ci.extent = { width, height, 1 };
    img_ci.mipLevels = levels;
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
    // An image without memory bound is not a half-broken object the driver tolerates: the next
    // barrier that names it faults inside libnvidia-glcore with a null dereference - which is how
    // the 2048x1024 terrain map used to die. The GPU is also often not empty (a local model holds
    // gigabytes of VRAM), and an OPTIMAL image is never offered host-visible memory by a real
    // driver, so the retry has to remake the image as LINEAR before system RAM can back it.
    // Sampling is slower there, but the picture survives instead of every failed texture turning
    // into the missing one.
    VkResult res = vkAllocateMemory(device, &ai, nullptr, &tex->memory);
    if (res == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
        vkDestroyImage(device, tex->image, nullptr);
        tex->image = VK_NULL_HANDLE;
        img_ci.tiling = VK_IMAGE_TILING_LINEAR;
        if (vkCreateImage(device, &img_ci, nullptr, &tex->image) != VK_SUCCESS) {
            delete tex;
            return nullptr;
        }
        vkGetImageMemoryRequirements(device, tex->image, &reqs);
        ai.allocationSize = reqs.size;
        ai.memoryTypeIndex = find_memory_type(phys_dev, reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        if (ai.memoryTypeIndex == UINT32_MAX) {
            vkDestroyImage(device, tex->image, nullptr);
            tex->image = VK_NULL_HANDLE;
            delete tex;
            return nullptr;
        }
        res = vkAllocateMemory(device, &ai, nullptr, &tex->memory);
    }
    if (res != VK_SUCCESS) {
        fprintf(stderr, "vkAllocateMemory %ux%u size=%llu: %d\n", width, height,
                (unsigned long long)reqs.size, (int)res);
        fflush(stderr);
        vkDestroyImage(device, tex->image, nullptr);
        tex->image = VK_NULL_HANDLE;
        delete tex;
        return nullptr;
    }
    res = vkBindImageMemory(device, tex->image, tex->memory, 0);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "vkBindImageMemory %ux%u: %d\n", width, height, (int)res);
        fflush(stderr);
        vkFreeMemory(device, tex->memory, nullptr);
        tex->memory = VK_NULL_HANDLE;
        vkDestroyImage(device, tex->image, nullptr);
        tex->image = VK_NULL_HANDLE;
        delete tex;
        return nullptr;
    }

    VkImageViewCreateInfo view_ci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    view_ci.image = tex->image;
    view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_ci.format = vkFmt;
    view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_ci.subresourceRange.levelCount = levels;
    view_ci.subresourceRange.layerCount = 1;
    VkResult viewRes = vkCreateImageView(device, &view_ci, nullptr, &tex->view);
    if (viewRes != VK_SUCCESS || tex->view == VK_NULL_HANDLE) {
        fprintf(stderr, "vkCreateImageView failed %ux%u fmt=%d levels=%u res=%d\n",
                width, height, (int)vkFmt, levels, (int)viewRes);
        fflush(stderr);
    }

    if (pixels) {
        // Initial pixels join the same deferred queue: no staging buffer, no submit and no wait
        // at creation time. The next BeginVulkanFrame (or an idle drain) records the copy.
        PendingTexUpdate upd;
        upd.tex = tex; upd.level = 0u; upd.x = 0; upd.y = 0; upd.w = (int)width; upd.h = (int)height;
        upd.bytes.assign((const uint8_t *)pixels, (const uint8_t *)pixels + uploadSz);
        s_pendingTexUpdates.push_back(std::move(upd));
    }

    // The default sampler state, created now so a texture that is drawn before any state is set
    // still has sets to bind. Other states are created on demand by texture_variant().
    if (s_descriptorPool != VK_NULL_HANDLE) texture_variant(tex, 0u);

    return tex;
}

void *Platform_CreateVulkanTexture(uint32_t width, uint32_t height, const void *pixels, uint32_t format)
{
    return Platform_CreateVulkanTextureEx(width, height, 1u, pixels, format);
}

void Platform_BindVulkanTexture(void *texture)
{
    s_boundTextures[0] = (PlatformVulkanTexture*)texture;
    s_boundTextures[1] = nullptr;
    s_boundTextures[2] = nullptr;
    s_boundTextures[3] = nullptr;
    s_boundSamplerKeys[0] = s_boundSamplerKeys[1] = s_boundSamplerKeys[2] = s_boundSamplerKeys[3] = 0u;
}

void Platform_BindVulkanTextures(void *const textures[4], const uint32_t *samplerKeys)
{
    for (int stage = 0; stage < 4; ++stage) {
        s_boundTextures[stage] = (PlatformVulkanTexture*)(textures ? textures[stage] : nullptr);
        s_boundSamplerKeys[stage] = samplerKeys ? samplerKeys[stage] : 0u;
    }
}

uint32_t Platform_MakeSamplerKey(uint32_t addressU, uint32_t addressV, uint32_t minFilter,
                                 uint32_t magFilter, uint32_t mipFilter, int mipsReady)
{
    uint32_t key = (addressU & 0x7u) | ((addressV & 0x7u) << 3) |
                   ((minFilter & 0x7u) << 6) | ((magFilter & 0x7u) << 9) |
                   ((mipFilter & 0x7u) << 12);
    if (mipsReady) key |= PLATFORM_SAMPLER_MIPS_READY;
    return key;
}

void Platform_SetVulkanDepthState(int enable, int write, int d3dCompare)
{
    s_depthTest = enable != 0;
    s_depthWrite = write != 0;
    switch (d3dCompare) {
    case 1: s_depthCompare = VK_COMPARE_OP_NEVER; break;
    case 2: s_depthCompare = VK_COMPARE_OP_LESS; break;
    case 3: s_depthCompare = VK_COMPARE_OP_EQUAL; break;
    case 5: s_depthCompare = VK_COMPARE_OP_GREATER; break;
    case 6: s_depthCompare = VK_COMPARE_OP_NOT_EQUAL; break;
    case 7: s_depthCompare = VK_COMPARE_OP_GREATER_OR_EQUAL; break;
    case 8: s_depthCompare = VK_COMPARE_OP_ALWAYS; break;
    default: s_depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL; break;
    }
}

void Platform_DestroyVulkanTexture(void *texture)
{
    if (!texture || !s_ctx.isInitialized) return;
    PlatformVulkanTexture *tex = (PlatformVulkanTexture*)texture;
    for (int stage = 0; stage < 4; ++stage) {
        if (s_boundTextures[stage] == tex) s_boundTextures[stage] = nullptr;
    }
    s_lastDescSet = VK_NULL_HANDLE;
    // No queue wait: in-flight frames may still sample this image. The drain at the start of
    // BeginVulkanFrame frees it once every acquired image has rotated past the frame that
    // destroyed it, which is when no queued command buffer can reference it any more.
    PendingTexFree e;
    e.tex = tex;
    e.frame = s_frameCounter;
    e.image = UINT32_MAX;
    s_pendingTexFrees.push_back(e);
}


void Platform_ShutdownVulkan(void)
{
    if (!s_ctx.isInitialized) return;

    VkDevice device = (VkDevice)s_ctx.device;
    VkInstance instance = (VkInstance)s_ctx.instance;

    vkDeviceWaitIdle(device);
    // With the queue drained, everything still waiting on a fence is safe to free now.
    while (!s_pendingTexFrees.empty()) {
        free_texture_objects(device, s_pendingTexFrees.back().tex);
        s_pendingTexFrees.pop_back();
    }
    s_pendingTexUpdates.clear();

    destroy_sync_objects(device);
    free_staging(device);
    if (s_commandPool) vkDestroyCommandPool(device, s_commandPool, nullptr);
    destroy_depth(device);
    if (s_graphicsPipelineOpaque) { vkDestroyPipeline(device, s_graphicsPipelineOpaque, nullptr); s_graphicsPipelineOpaque = VK_NULL_HANDLE; }
    if (s_graphicsPipelineBlend) { vkDestroyPipeline(device, s_graphicsPipelineBlend, nullptr); s_graphicsPipelineBlend = VK_NULL_HANDLE; }
    if (s_graphicsPipelineMultiply) { vkDestroyPipeline(device, s_graphicsPipelineMultiply, nullptr); s_graphicsPipelineMultiply = VK_NULL_HANDLE; }
    if (s_graphicsPipelineAdditive) { vkDestroyPipeline(device, s_graphicsPipelineAdditive, nullptr); s_graphicsPipelineAdditive = VK_NULL_HANDLE; }
    if (s_pipelineLayout) { vkDestroyPipelineLayout(device, s_pipelineLayout, nullptr); s_pipelineLayout = VK_NULL_HANDLE; }
    if (s_defaultTexture) {
        Platform_DestroyVulkanTexture(s_defaultTexture);
        s_defaultTexture = nullptr;
    }
    for (int stage = 0; stage < 4; ++stage) s_boundTextures[stage] = nullptr;
    if (s_descriptorPool) { vkDestroyDescriptorPool(device, s_descriptorPool, nullptr); s_descriptorPool = VK_NULL_HANDLE; }
    if (s_descriptorSetLayout) { vkDestroyDescriptorSetLayout(device, s_descriptorSetLayout, nullptr); s_descriptorSetLayout = VK_NULL_HANDLE; }
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
