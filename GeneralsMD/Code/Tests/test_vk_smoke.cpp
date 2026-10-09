/*
** Command & Conquer Generals ZH Reborn
** Vulkan Renderer Milestone M2.1 — Smoke Test
**
** Proves native Vulkan initialization, offscreen framebuffer setup,
** graphics pipeline creation with embedded SPIR-V, draw call execution,
** and pixel readback verification without requiring a window or display.
*/

#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <vector>

#define VK_CHECK(call, msg) \
    do { \
        VkResult _res = (call); \
        if (_res != VK_SUCCESS) { \
            fprintf(stderr, "FAIL: %s (VkResult: %d, line %d)\n", msg, (int)_res, __LINE__); \
            return 1; \
        } \
    } while (0)

static const uint32_t FB_WIDTH = 256;
static const uint32_t FB_HEIGHT = 256;

// Clear color matching test_dx9_smoke: RGB(64, 0, 128)
static const uint8_t CLEAR_R = 64;
static const uint8_t CLEAR_G = 0;
static const uint8_t CLEAR_B = 128;
static const uint8_t CLEAR_A = 255;

// Rendered triangle color: RGB(0, 255, 0) - Solid Green
static const uint8_t TRI_R = 0;
static const uint8_t TRI_G = 255;
static const uint8_t TRI_B = 0;
static const uint8_t TRI_A = 255;

struct Vertex {
    float x, y;
};

// SPIR-V bytecode: Vertex Shader (vec2 inPos -> gl_Position = vec4(inPos, 0.0, 1.0))
static const uint32_t k_vert_spv[122] = {
    0x07230203, 0x00010000, 0x00000000, 0x00000017, 0x00000000, 0x00020011, 0x00000001, 0x0003000e,
    0x00000000, 0x00000001, 0x0007000f, 0x00000000, 0x00000010, 0x6e69616d, 0x00000000, 0x00000009,
    0x0000000c, 0x00030047, 0x00000007, 0x00000002, 0x00050048, 0x00000007, 0x00000000, 0x0000000b,
    0x00000000, 0x00040047, 0x0000000c, 0x0000001e, 0x00000000, 0x00020013, 0x00000001, 0x00030021,
    0x00000002, 0x00000001, 0x00030016, 0x00000003, 0x00000020, 0x00040015, 0x00000004, 0x00000020,
    0x00000001, 0x00040017, 0x00000005, 0x00000003, 0x00000002, 0x00040017, 0x00000006, 0x00000003,
    0x00000004, 0x0003001e, 0x00000007, 0x00000006, 0x00040020, 0x00000008, 0x00000003, 0x00000007,
    0x0004003b, 0x00000008, 0x00000009, 0x00000003, 0x00040020, 0x0000000a, 0x00000003, 0x00000006,
    0x00040020, 0x0000000b, 0x00000001, 0x00000005, 0x0004003b, 0x0000000b, 0x0000000c, 0x00000001,
    0x0004002b, 0x00000003, 0x0000000d, 0x00000000, 0x0004002b, 0x00000003, 0x0000000e, 0x3f800000,
    0x0004002b, 0x00000004, 0x0000000f, 0x00000000, 0x00050036, 0x00000001, 0x00000010, 0x00000000,
    0x00000002, 0x000200f8, 0x00000011, 0x0004003d, 0x00000005, 0x00000012, 0x0000000c, 0x00050051,
    0x00000003, 0x00000013, 0x00000012, 0x00000000, 0x00050051, 0x00000003, 0x00000014, 0x00000012,
    0x00000001, 0x00070050, 0x00000006, 0x00000015, 0x00000013, 0x00000014, 0x0000000d, 0x0000000e,
    0x00050041, 0x0000000a, 0x00000016, 0x00000009, 0x0000000f, 0x0003003e, 0x00000016, 0x00000015,
    0x000100fd, 0x00010038,
};

// SPIR-V bytecode: Fragment Shader (outColor = vec4(0.0, 1.0, 0.0, 1.0))
static const uint32_t k_frag_spv[70] = {
    0x07230203, 0x00010000, 0x00000000, 0x0000000c, 0x00000000, 0x00020011, 0x00000001, 0x0003000e,
    0x00000000, 0x00000001, 0x0006000f, 0x00000004, 0x0000000a, 0x6e69616d, 0x00000000, 0x00000006,
    0x00030010, 0x0000000a, 0x00000007, 0x00040047, 0x00000006, 0x0000001e, 0x00000000, 0x00020013,
    0x00000001, 0x00030021, 0x00000002, 0x00000001, 0x00030016, 0x00000003, 0x00000020, 0x00040017,
    0x00000004, 0x00000003, 0x00000004, 0x00040020, 0x00000005, 0x00000003, 0x00000004, 0x0004003b,
    0x00000005, 0x00000006, 0x00000003, 0x0004002b, 0x00000003, 0x00000007, 0x00000000, 0x0004002b,
    0x00000003, 0x00000008, 0x3f800000, 0x0007002c, 0x00000004, 0x00000009, 0x00000007, 0x00000008,
    0x00000007, 0x00000008, 0x00050036, 0x00000001, 0x0000000a, 0x00000000, 0x00000002, 0x000200f8,
    0x0000000b, 0x0003003e, 0x00000006, 0x00000009, 0x000100fd, 0x00010038,
};

static uint32_t find_memory_type(VkPhysicalDevice phys_dev, uint32_t type_filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(phys_dev, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    // Fall back to first matching filter bit if desired properties not fully met
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if (type_filter & (1 << i)) {
            return i;
        }
    }
    fprintf(stderr, "Failed to find suitable memory type\n");
    std::abort();
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    // 1. Create Vulkan Instance
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "VKSmoke";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "ZHReborn";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo inst_ci = {};
    inst_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_ci.pApplicationInfo = &app_info;

    VkInstance instance = VK_NULL_HANDLE;
    VK_CHECK(vkCreateInstance(&inst_ci, nullptr, &instance), "vkCreateInstance");

    // 2. Select Physical Device
    uint32_t dev_count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &dev_count, nullptr), "vkEnumeratePhysicalDevices(count)");
    if (dev_count == 0) {
        fprintf(stderr, "FAIL: No Vulkan physical devices found\n");
        vkDestroyInstance(instance, nullptr);
        return 1;
    }

    std::vector<VkPhysicalDevice> phys_devs(dev_count);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &dev_count, phys_devs.data()), "vkEnumeratePhysicalDevices(list)");

    VkPhysicalDevice phys_dev = phys_devs[0];
    VkPhysicalDeviceProperties chosen_props;
    vkGetPhysicalDeviceProperties(phys_dev, &chosen_props);

    for (const auto &dev : phys_devs) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            phys_dev = dev;
            chosen_props = props;
            break;
        }
    }
    printf("Vulkan physical device: %s (type %d, driver %u)\n",
           chosen_props.deviceName, (int)chosen_props.deviceType, chosen_props.driverVersion);

    // 3. Find Graphics Queue Family
    uint32_t q_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys_dev, &q_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> q_props(q_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(phys_dev, &q_family_count, q_props.data());

    uint32_t graphics_q_idx = UINT32_MAX;
    for (uint32_t i = 0; i < q_family_count; ++i) {
        if (q_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_q_idx = i;
            break;
        }
    }
    if (graphics_q_idx == UINT32_MAX) {
        fprintf(stderr, "FAIL: No graphics queue family found\n");
        vkDestroyInstance(instance, nullptr);
        return 1;
    }

    // 4. Create Logical Device & Queue
    float q_prio = 1.0f;
    VkDeviceQueueCreateInfo q_ci = {};
    q_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    q_ci.queueFamilyIndex = graphics_q_idx;
    q_ci.queueCount = 1;
    q_ci.pQueuePriorities = &q_prio;

    VkDeviceCreateInfo dev_ci = {};
    dev_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dev_ci.queueCreateInfoCount = 1;
    dev_ci.pQueueCreateInfos = &q_ci;

    VkDevice device = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDevice(phys_dev, &dev_ci, nullptr, &device), "vkCreateDevice");

    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, graphics_q_idx, 0, &queue);

    // 5. Create Offscreen Color Image (256x256, R8G8B8A8_UNORM)
    VkImageCreateInfo img_ci = {};
    img_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    img_ci.imageType = VK_IMAGE_TYPE_2D;
    img_ci.format = VK_FORMAT_R8G8B8A8_UNORM;
    img_ci.extent = { FB_WIDTH, FB_HEIGHT, 1 };
    img_ci.mipLevels = 1;
    img_ci.arrayLayers = 1;
    img_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    img_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    img_ci.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    img_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    img_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage color_img = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImage(device, &img_ci, nullptr, &color_img), "vkCreateImage");

    VkMemoryRequirements img_mem_req;
    vkGetImageMemoryRequirements(device, color_img, &img_mem_req);

    VkMemoryAllocateInfo img_mem_ai = {};
    img_mem_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    img_mem_ai.allocationSize = img_mem_req.size;
    img_mem_ai.memoryTypeIndex = find_memory_type(phys_dev, img_mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkDeviceMemory color_mem = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(device, &img_mem_ai, nullptr, &color_mem), "vkAllocateMemory(color_img)");
    VK_CHECK(vkBindImageMemory(device, color_img, color_mem, 0), "vkBindImageMemory");

    // 6. Create Image View
    VkImageViewCreateInfo view_ci = {};
    view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_ci.image = color_img;
    view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_ci.format = VK_FORMAT_R8G8B8A8_UNORM;
    view_ci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
    view_ci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    VkImageView color_view = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(device, &view_ci, nullptr, &color_view), "vkCreateImageView");

    // 7. Create Render Pass
    VkAttachmentDescription color_att = {};
    color_att.format = VK_FORMAT_R8G8B8A8_UNORM;
    color_att.samples = VK_SAMPLE_COUNT_1_BIT;
    color_att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_att.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_att_ref = {};
    color_att_ref.attachment = 0;
    color_att_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_att_ref;

    VkRenderPassCreateInfo rp_ci = {};
    rp_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_ci.attachmentCount = 1;
    rp_ci.pAttachments = &color_att;
    rp_ci.subpassCount = 1;
    rp_ci.pSubpasses = &subpass;

    VkRenderPass render_pass = VK_NULL_HANDLE;
    VK_CHECK(vkCreateRenderPass(device, &rp_ci, nullptr, &render_pass), "vkCreateRenderPass");

    // 8. Create Framebuffer
    VkFramebufferCreateInfo fb_ci = {};
    fb_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_ci.renderPass = render_pass;
    fb_ci.attachmentCount = 1;
    fb_ci.pAttachments = &color_view;
    fb_ci.width = FB_WIDTH;
    fb_ci.height = FB_HEIGHT;
    fb_ci.layers = 1;

    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFramebuffer(device, &fb_ci, nullptr, &framebuffer), "vkCreateFramebuffer");

    // 9. Create Vertex Buffer
    const Vertex k_vertices[3] = {
        {  0.0f, -0.6f }, // Top
        {  0.6f,  0.6f }, // Bottom right
        { -0.6f,  0.6f }  // Bottom left
    };

    VkBufferCreateInfo vb_ci = {};
    vb_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vb_ci.size = sizeof(k_vertices);
    vb_ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vb_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer vertex_buf = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(device, &vb_ci, nullptr, &vertex_buf), "vkCreateBuffer(vertex)");

    VkMemoryRequirements vb_mem_req;
    vkGetBufferMemoryRequirements(device, vertex_buf, &vb_mem_req);

    VkMemoryAllocateInfo vb_mem_ai = {};
    vb_mem_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vb_mem_ai.allocationSize = vb_mem_req.size;
    vb_mem_ai.memoryTypeIndex = find_memory_type(phys_dev, vb_mem_req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDeviceMemory vertex_mem = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(device, &vb_mem_ai, nullptr, &vertex_mem), "vkAllocateMemory(vertex)");
    VK_CHECK(vkBindBufferMemory(device, vertex_buf, vertex_mem, 0), "vkBindBufferMemory(vertex)");

    void *p_vdata = nullptr;
    VK_CHECK(vkMapMemory(device, vertex_mem, 0, sizeof(k_vertices), 0, &p_vdata), "vkMapMemory(vertex)");
    memcpy(p_vdata, k_vertices, sizeof(k_vertices));
    vkUnmapMemory(device, vertex_mem);

    // 10. Create Shader Modules
    VkShaderModuleCreateInfo vs_ci = {};
    vs_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vs_ci.codeSize = sizeof(k_vert_spv);
    vs_ci.pCode = k_vert_spv;

    VkShaderModule vert_module = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device, &vs_ci, nullptr, &vert_module), "vkCreateShaderModule(vert)");

    VkShaderModuleCreateInfo fs_ci = {};
    fs_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fs_ci.codeSize = sizeof(k_frag_spv);
    fs_ci.pCode = k_frag_spv;

    VkShaderModule frag_module = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device, &fs_ci, nullptr, &frag_module), "vkCreateShaderModule(frag)");

    // 11. Pipeline Layout & Graphics Pipeline
    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert_module;
    stages[0].pName = "main";

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag_module;
    stages[1].pName = "main";

    VkVertexInputBindingDescription vi_binding = {};
    vi_binding.binding = 0;
    vi_binding.stride = sizeof(Vertex);
    vi_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription vi_attr = {};
    vi_attr.location = 0;
    vi_attr.binding = 0;
    vi_attr.format = VK_FORMAT_R32G32_SFLOAT;
    vi_attr.offset = 0;

    VkPipelineVertexInputStateCreateInfo vi_state = {};
    vi_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi_state.vertexBindingDescriptionCount = 1;
    vi_state.pVertexBindingDescriptions = &vi_binding;
    vi_state.vertexAttributeDescriptionCount = 1;
    vi_state.pVertexAttributeDescriptions = &vi_attr;

    VkPipelineInputAssemblyStateCreateInfo ia_state = {};
    ia_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport = { 0.0f, 0.0f, (float)FB_WIDTH, (float)FB_HEIGHT, 0.0f, 1.0f };
    VkRect2D scissor = { { 0, 0 }, { FB_WIDTH, FB_HEIGHT } };
    VkPipelineViewportStateCreateInfo vp_state = {};
    vp_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp_state.viewportCount = 1;
    vp_state.pViewports = &viewport;
    vp_state.scissorCount = 1;
    vp_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rast_state = {};
    rast_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rast_state.polygonMode = VK_POLYGON_MODE_FILL;
    rast_state.cullMode = VK_CULL_MODE_NONE;
    rast_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rast_state.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms_state = {};
    ms_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cb_att = {};
    cb_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cb_att.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo cb_state = {};
    cb_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb_state.attachmentCount = 1;
    cb_state.pAttachments = &cb_att;

    VkPipelineLayoutCreateInfo pl_ci = {};
    pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(device, &pl_ci, nullptr, &pipeline_layout), "vkCreatePipelineLayout");

    VkGraphicsPipelineCreateInfo pipe_ci = {};
    pipe_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipe_ci.stageCount = 2;
    pipe_ci.pStages = stages;
    pipe_ci.pVertexInputState = &vi_state;
    pipe_ci.pInputAssemblyState = &ia_state;
    pipe_ci.pViewportState = &vp_state;
    pipe_ci.pRasterizationState = &rast_state;
    pipe_ci.pMultisampleState = &ms_state;
    pipe_ci.pColorBlendState = &cb_state;
    pipe_ci.layout = pipeline_layout;
    pipe_ci.renderPass = render_pass;
    pipe_ci.subpass = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipe_ci, nullptr, &pipeline), "vkCreateGraphicsPipelines");

    // 12. Readback Staging Buffer (256 * 256 * 4 bytes)
    const VkDeviceSize readback_size = FB_WIDTH * FB_HEIGHT * 4;
    VkBufferCreateInfo rb_ci = {};
    rb_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    rb_ci.size = readback_size;
    rb_ci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    rb_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer readback_buf = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(device, &rb_ci, nullptr, &readback_buf), "vkCreateBuffer(readback)");

    VkMemoryRequirements rb_mem_req;
    vkGetBufferMemoryRequirements(device, readback_buf, &rb_mem_req);

    VkMemoryAllocateInfo rb_mem_ai = {};
    rb_mem_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    rb_mem_ai.allocationSize = rb_mem_req.size;
    rb_mem_ai.memoryTypeIndex = find_memory_type(phys_dev, rb_mem_req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDeviceMemory readback_mem = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(device, &rb_mem_ai, nullptr, &readback_mem), "vkAllocateMemory(readback)");
    VK_CHECK(vkBindBufferMemory(device, readback_buf, readback_mem, 0), "vkBindBufferMemory(readback)");

    // 13. Command Pool & Command Buffer
    VkCommandPoolCreateInfo cp_ci = {};
    cp_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cp_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cp_ci.queueFamilyIndex = graphics_q_idx;

    VkCommandPool cmd_pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateCommandPool(device, &cp_ci, nullptr, &cmd_pool), "vkCreateCommandPool");

    VkCommandBufferAllocateInfo cmd_ai = {};
    cmd_ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_ai.commandPool = cmd_pool;
    cmd_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_ai.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(device, &cmd_ai, &cmd), "vkAllocateCommandBuffers");

    // 14. Record & Execute Render Commands
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info), "vkBeginCommandBuffer");

    VkClearValue clear_color = {};
    clear_color.color.float32[0] = (float)CLEAR_R / 255.0f;
    clear_color.color.float32[1] = (float)CLEAR_G / 255.0f;
    clear_color.color.float32[2] = (float)CLEAR_B / 255.0f;
    clear_color.color.float32[3] = (float)CLEAR_A / 255.0f;

    VkRenderPassBeginInfo rp_bi = {};
    rp_bi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_bi.renderPass = render_pass;
    rp_bi.framebuffer = framebuffer;
    rp_bi.renderArea = { { 0, 0 }, { FB_WIDTH, FB_HEIGHT } };
    rp_bi.clearValueCount = 1;
    rp_bi.pClearValues = &clear_color;

    vkCmdBeginRenderPass(cmd, &rp_bi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buf, &offset);
    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRenderPass(cmd);

    // Transition image to transfer src layout
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.image = color_img;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy to readback buffer
    VkBufferImageCopy copy_region = {};
    copy_region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copy_region.imageExtent = { FB_WIDTH, FB_HEIGHT, 1 };

    vkCmdCopyImageToBuffer(cmd, color_img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback_buf, 1, &copy_region);

    VK_CHECK(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE), "vkQueueSubmit");
    VK_CHECK(vkQueueWaitIdle(queue), "vkQueueWaitIdle");

    // 15. Verify Pixel Data
    void *p_readback = nullptr;
    VK_CHECK(vkMapMemory(device, readback_mem, 0, readback_size, 0, &p_readback), "vkMapMemory(readback)");
    const uint8_t *pixels = static_cast<const uint8_t *>(p_readback);

    // Sample 1: Background pixel at (10, 10) - must be clear color RGB(64, 0, 128)
    const uint32_t bg_offset = (10 * FB_WIDTH + 10) * 4;
    const uint8_t bg_r = pixels[bg_offset + 0];
    const uint8_t bg_g = pixels[bg_offset + 1];
    const uint8_t bg_b = pixels[bg_offset + 2];
    const uint8_t bg_a = pixels[bg_offset + 3];

    // Sample 2: Center pixel at (128, 128) - inside triangle, must be TRI_G=255
    const uint32_t tri_offset = (128 * FB_WIDTH + 128) * 4;
    const uint8_t tri_r = pixels[tri_offset + 0];
    const uint8_t tri_g = pixels[tri_offset + 1];
    const uint8_t tri_b = pixels[tri_offset + 2];
    const uint8_t tri_a = pixels[tri_offset + 3];

    vkUnmapMemory(device, readback_mem);

    printf("Pixel sample (10, 10) [Background]: R=%u G=%u B=%u A=%u (expected %u, %u, %u, %u)\n",
           bg_r, bg_g, bg_b, bg_a, CLEAR_R, CLEAR_G, CLEAR_B, CLEAR_A);
    printf("Pixel sample (128, 128) [Triangle]:  R=%u G=%u B=%u A=%u (expected %u, %u, %u, %u)\n",
           tri_r, tri_g, tri_b, tri_a, TRI_R, TRI_G, TRI_B, TRI_A);

    bool bg_ok = (std::abs((int)bg_r - (int)CLEAR_R) <= 2) &&
                 (std::abs((int)bg_g - (int)CLEAR_G) <= 2) &&
                 (std::abs((int)bg_b - (int)CLEAR_B) <= 2);

    bool tri_ok = (std::abs((int)tri_r - (int)TRI_R) <= 2) &&
                  (std::abs((int)tri_g - (int)TRI_G) <= 2) &&
                  (std::abs((int)tri_b - (int)TRI_B) <= 2);

    // 16. Cleanup
    vkDestroyBuffer(device, readback_buf, nullptr);
    vkFreeMemory(device, readback_mem, nullptr);

    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    vkDestroyShaderModule(device, frag_module, nullptr);
    vkDestroyShaderModule(device, vert_module, nullptr);

    vkDestroyBuffer(device, vertex_buf, nullptr);
    vkFreeMemory(device, vertex_mem, nullptr);

    vkDestroyFramebuffer(device, framebuffer, nullptr);
    vkDestroyRenderPass(device, render_pass, nullptr);
    vkDestroyImageView(device, color_view, nullptr);
    vkDestroyImage(device, color_img, nullptr);
    vkFreeMemory(device, color_mem, nullptr);

    vkDestroyCommandPool(device, cmd_pool, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

    if (!bg_ok || !tri_ok) {
        fprintf(stderr, "FAIL: Pixel verification mismatch (bg_ok=%d, tri_ok=%d)\n", bg_ok, tri_ok);
        return 1;
    }

    printf("Vulkan smoke OK: offscreen triangle rendered and verified.\n");
    return 0;
}
