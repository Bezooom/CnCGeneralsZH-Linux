/*
** Command & Conquer Generals ZH Reborn
** Vulkan Renderer Milestone M2.2 — Full Pipeline Test
**
** Proves Vulkan device initialization, offscreen color and depth framebuffer,
** FVF-aligned vertex input layout (DX8_FVF_XYZNDUV2), indexed drawing,
** depth-stencil test and write validation (occlusion & pass-through),
** and pixel readback with precision color verification.
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

static const uint32_t TARGET_WIDTH = 64;
static const uint32_t TARGET_HEIGHT = 64;

// Clear color: RGB(64, 0, 128, 255)
static const uint8_t CLEAR_R = 64;
static const uint8_t CLEAR_G = 0;
static const uint8_t CLEAR_B = 128;
static const uint8_t CLEAR_A = 255;

// Quad 1 Color matching test_dx11pipeline.cpp VERTEX_COLOUR = 0xff112233
static const uint8_t Q1_R = 0x11;
static const uint8_t Q1_G = 0x22;
static const uint8_t Q1_B = 0x33;
static const uint8_t Q1_A = 0xff;

// Quad 3 Color (Green overlay in front)
static const uint8_t Q3_R = 0;
static const uint8_t Q3_G = 255;
static const uint8_t Q3_B = 0;
static const uint8_t Q3_A = 255;

// One vertex of DX8_FVF_XYZNDUV2 format
struct PipelineVertex {
    float Position[3];
    float Normal[3];
    float Diffuse[4];
    float TexCoord0[2];
    float TexCoord1[2];
};

// Embedded SPIR-V for Vertex Shader
// layout(location = 0) in vec3 inPos;
// layout(location = 1) in vec3 inNormal;
// layout(location = 2) in vec4 inDiffuse;
// layout(location = 3) in vec2 inTex0;
// layout(location = 4) in vec2 inTex1;
// layout(location = 0) out vec4 outColor;
// void main() {
//     gl_Position = vec4(inPos, 1.0);
//     outColor = inDiffuse;
// }
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

// Embedded SPIR-V for Fragment Shader
// layout(location = 0) in vec4 inColor;
// layout(location = 0) out vec4 outColor;
// void main() {
//     outColor = inColor;
// }
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

int main(int argc, char** argv)
{
    (void)argc; (void)argv;
    printf("=== Vulkan Milestone M2.2: Full Graphics Pipeline Test ===\n");

    // 1. Create Instance
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "GeneralsZH_M2_2_Pipeline";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "GeneralsMD";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo instance_ci = {};
    instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_ci.pApplicationInfo = &app_info;

    VkInstance instance = VK_NULL_HANDLE;
    VK_CHECK(vkCreateInstance(&instance_ci, nullptr, &instance), "vkCreateInstance");

    // 2. Select Discrete Physical Device
    uint32_t dev_count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &dev_count, nullptr), "vkEnumeratePhysicalDevices count");
    if (dev_count == 0) {
        fprintf(stderr, "FAIL: No Vulkan physical devices found\n");
        return 1;
    }

    std::vector<VkPhysicalDevice> devices(dev_count);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &dev_count, devices.data()), "vkEnumeratePhysicalDevices");

    VkPhysicalDevice phys_dev = devices[0];
    for (const auto& d : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(d, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            phys_dev = d;
            break;
        }
    }

    VkPhysicalDeviceProperties chosen_props;
    vkGetPhysicalDeviceProperties(phys_dev, &chosen_props);
    printf("Chosen GPU: %s (Type: %d)\n", chosen_props.deviceName, (int)chosen_props.deviceType);

    // 3. Find Graphics Queue Family
    uint32_t qf_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys_dev, &qf_count, nullptr);
    std::vector<VkQueueFamilyProperties> qf_props(qf_count);
    vkGetPhysicalDeviceQueueFamilyProperties(phys_dev, &qf_count, qf_props.data());

    uint32_t graphics_qf = UINT32_MAX;
    for (uint32_t i = 0; i < qf_count; ++i) {
        if (qf_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_qf = i;
            break;
        }
    }
    if (graphics_qf == UINT32_MAX) {
        fprintf(stderr, "FAIL: No graphics queue family found\n");
        return 1;
    }

    // 4. Create Logical Device
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_ci = {};
    queue_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_ci.queueFamilyIndex = graphics_qf;
    queue_ci.queueCount = 1;
    queue_ci.pQueuePriorities = &queue_priority;

    VkPhysicalDeviceFeatures device_features = {};
    device_features.depthClamp = VK_FALSE;

    VkDeviceCreateInfo device_ci = {};
    device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_ci.queueCreateInfoCount = 1;
    device_ci.pQueueCreateInfos = &queue_ci;
    device_ci.pEnabledFeatures = &device_features;

    VkDevice device = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDevice(phys_dev, &device_ci, nullptr, &device), "vkCreateDevice");

    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, graphics_qf, 0, &queue);

    // 5. Create Command Pool
    VkCommandPoolCreateInfo pool_ci = {};
    pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_ci.queueFamilyIndex = graphics_qf;

    VkCommandPool cmd_pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateCommandPool(device, &pool_ci, nullptr, &cmd_pool), "vkCreateCommandPool");

    // 6. Create Color Attachment Image (R8G8B8A8_UNORM)
    VkImageCreateInfo color_img_ci = {};
    color_img_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    color_img_ci.imageType = VK_IMAGE_TYPE_2D;
    color_img_ci.format = VK_FORMAT_R8G8B8A8_UNORM;
    color_img_ci.extent = { TARGET_WIDTH, TARGET_HEIGHT, 1 };
    color_img_ci.mipLevels = 1;
    color_img_ci.arrayLayers = 1;
    color_img_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    color_img_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    color_img_ci.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    color_img_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage color_image = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImage(device, &color_img_ci, nullptr, &color_image), "vkCreateImage color");

    VkMemoryRequirements color_mem_reqs;
    vkGetImageMemoryRequirements(device, color_image, &color_mem_reqs);
    uint32_t color_mem_type = find_memory_type(phys_dev, color_mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkMemoryAllocateInfo color_mem_ai = {};
    color_mem_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    color_mem_ai.allocationSize = color_mem_reqs.size;
    color_mem_ai.memoryTypeIndex = color_mem_type;

    VkDeviceMemory color_memory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(device, &color_mem_ai, nullptr, &color_memory), "vkAllocateMemory color");
    VK_CHECK(vkBindImageMemory(device, color_image, color_memory, 0), "vkBindImageMemory color");

    VkImageViewCreateInfo color_view_ci = {};
    color_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    color_view_ci.image = color_image;
    color_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    color_view_ci.format = VK_FORMAT_R8G8B8A8_UNORM;
    color_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    color_view_ci.subresourceRange.levelCount = 1;
    color_view_ci.subresourceRange.layerCount = 1;

    VkImageView color_view = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(device, &color_view_ci, nullptr, &color_view), "vkCreateImageView color");

    // 7. Create Depth Attachment Image (D32_SFLOAT)
    VkImageCreateInfo depth_img_ci = {};
    depth_img_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depth_img_ci.imageType = VK_IMAGE_TYPE_2D;
    depth_img_ci.format = VK_FORMAT_D32_SFLOAT;
    depth_img_ci.extent = { TARGET_WIDTH, TARGET_HEIGHT, 1 };
    depth_img_ci.mipLevels = 1;
    depth_img_ci.arrayLayers = 1;
    depth_img_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_img_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    depth_img_ci.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth_img_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage depth_image = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImage(device, &depth_img_ci, nullptr, &depth_image), "vkCreateImage depth");

    VkMemoryRequirements depth_mem_reqs;
    vkGetImageMemoryRequirements(device, depth_image, &depth_mem_reqs);
    uint32_t depth_mem_type = find_memory_type(phys_dev, depth_mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkMemoryAllocateInfo depth_mem_ai = {};
    depth_mem_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    depth_mem_ai.allocationSize = depth_mem_reqs.size;
    depth_mem_ai.memoryTypeIndex = depth_mem_type;

    VkDeviceMemory depth_memory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(device, &depth_mem_ai, nullptr, &depth_memory), "vkAllocateMemory depth");
    VK_CHECK(vkBindImageMemory(device, depth_image, depth_memory, 0), "vkBindImageMemory depth");

    VkImageViewCreateInfo depth_view_ci = {};
    depth_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depth_view_ci.image = depth_image;
    depth_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depth_view_ci.format = VK_FORMAT_D32_SFLOAT;
    depth_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depth_view_ci.subresourceRange.levelCount = 1;
    depth_view_ci.subresourceRange.layerCount = 1;

    VkImageView depth_view = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(device, &depth_view_ci, nullptr, &depth_view), "vkCreateImageView depth");

    // 8. Create Render Pass (Color + Depth Attachments)
    VkAttachmentDescription attachments[2] = {};
    // Color
    attachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    // Depth
    attachments[1].format = VK_FORMAT_D32_SFLOAT;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference depth_ref = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;
    subpass.pDepthStencilAttachment = &depth_ref;

    VkRenderPassCreateInfo rp_ci = {};
    rp_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_ci.attachmentCount = 2;
    rp_ci.pAttachments = attachments;
    rp_ci.subpassCount = 1;
    rp_ci.pSubpasses = &subpass;

    VkRenderPass render_pass = VK_NULL_HANDLE;
    VK_CHECK(vkCreateRenderPass(device, &rp_ci, nullptr, &render_pass), "vkCreateRenderPass");

    // 9. Create Framebuffer
    VkImageView fb_views[2] = { color_view, depth_view };
    VkFramebufferCreateInfo fb_ci = {};
    fb_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_ci.renderPass = render_pass;
    fb_ci.attachmentCount = 2;
    fb_ci.pAttachments = fb_views;
    fb_ci.width = TARGET_WIDTH;
    fb_ci.height = TARGET_HEIGHT;
    fb_ci.layers = 1;

    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFramebuffer(device, &fb_ci, nullptr, &framebuffer), "vkCreateFramebuffer");

    // 10. Create Shader Modules
    VkShaderModuleCreateInfo vert_sm_ci = {};
    vert_sm_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vert_sm_ci.codeSize = sizeof(k_vert_spv);
    vert_sm_ci.pCode = k_vert_spv;
    VkShaderModule vert_module = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device, &vert_sm_ci, nullptr, &vert_module), "vkCreateShaderModule vert");

    VkShaderModuleCreateInfo frag_sm_ci = {};
    frag_sm_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    frag_sm_ci.codeSize = sizeof(k_frag_spv);
    frag_sm_ci.pCode = k_frag_spv;
    VkShaderModule frag_module = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device, &frag_sm_ci, nullptr, &frag_module), "vkCreateShaderModule frag");

    // 11. Create Graphics Pipeline with DX8_FVF_XYZNDUV2 Vertex Input Layout
    VkVertexInputBindingDescription binding_desc = {};
    binding_desc.binding = 0;
    binding_desc.stride = sizeof(PipelineVertex);
    binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attr_descs[5] = {};
    // Location 0: Position (float3)
    attr_descs[0].location = 0;
    attr_descs[0].binding = 0;
    attr_descs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attr_descs[0].offset = offsetof(PipelineVertex, Position);

    // Location 1: Normal (float3)
    attr_descs[1].location = 1;
    attr_descs[1].binding = 0;
    attr_descs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attr_descs[1].offset = offsetof(PipelineVertex, Normal);

    // Location 2: Diffuse (float4)
    attr_descs[1].location = 2;
    attr_descs[1].binding = 0;
    attr_descs[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attr_descs[1].offset = offsetof(PipelineVertex, Diffuse);

    VkPipelineVertexInputStateCreateInfo vi_ci = {};
    vi_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi_ci.vertexBindingDescriptionCount = 1;
    vi_ci.pVertexBindingDescriptions = &binding_desc;
    vi_ci.vertexAttributeDescriptionCount = 2;
    vi_ci.pVertexAttributeDescriptions = attr_descs;

    VkPipelineInputAssemblyStateCreateInfo ia_ci = {};
    ia_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia_ci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport = { 0.0f, 0.0f, (float)TARGET_WIDTH, (float)TARGET_HEIGHT, 0.0f, 1.0f };
    VkRect2D scissor = { { 0, 0 }, { TARGET_WIDTH, TARGET_HEIGHT } };
    VkPipelineViewportStateCreateInfo vp_ci = {};
    vp_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp_ci.viewportCount = 1;
    vp_ci.pViewports = &viewport;
    vp_ci.scissorCount = 1;
    vp_ci.pScissors = &scissor;
    VkPipelineRasterizationStateCreateInfo rast_ci = {};
    rast_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rast_ci.polygonMode = VK_POLYGON_MODE_FILL;
    rast_ci.cullMode = VK_CULL_MODE_NONE;
    rast_ci.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rast_ci.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms_ci = {};
    ms_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms_ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth & Stencil State: LESS_OR_EQUAL
    VkPipelineDepthStencilStateCreateInfo ds_ci = {};
    ds_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds_ci.depthTestEnable = VK_TRUE;
    ds_ci.depthWriteEnable = VK_TRUE;
    ds_ci.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    ds_ci.depthBoundsTestEnable = VK_FALSE;
    ds_ci.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState cb_att = {};
    cb_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cb_att.blendEnable = VK_FALSE;

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

    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(device, &layout_ci, nullptr, &pipeline_layout), "vkCreatePipelineLayout");

    VkPipelineShaderStageCreateInfo shader_stages[2] = {};
    shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stages[0].module = vert_module;
    shader_stages[0].pName = "main";

    shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stages[1].module = frag_module;
    shader_stages[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipe_ci = {};
    pipe_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipe_ci.stageCount = 2;
    pipe_ci.pStages = shader_stages;
    pipe_ci.pVertexInputState = &vi_ci;
    pipe_ci.pInputAssemblyState = &ia_ci;
    pipe_ci.pViewportState = &vp_ci;
    pipe_ci.pRasterizationState = &rast_ci;
    pipe_ci.pMultisampleState = &ms_ci;
    pipe_ci.pDepthStencilState = &ds_ci;
    pipe_ci.pColorBlendState = &cb_ci;
    pipe_ci.pDynamicState = &dyn_ci;
    pipe_ci.layout = pipeline_layout;
    pipe_ci.renderPass = render_pass;
    pipe_ci.subpass = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipe_ci, nullptr, &pipeline), "vkCreateGraphicsPipelines");

    // 12. Create Vertex & Index Buffers
    // Quad 1: Centered at Z = 0.5, Color = 0xff112233 (Q1_R, Q1_G, Q1_B, Q1_A)
    // Quad 2: Behind Quad 1 at Z = 0.8, Color = White (must be rejected by depth test)
    // Quad 3: In front at Z = 0.2, Top-left quadrant, Color = Green (must pass depth test)
    float q1_r = Q1_R / 255.0f, q1_g = Q1_G / 255.0f, q1_b = Q1_B / 255.0f, q1_a = 1.0f;
    float q2_r = 1.0f, q2_g = 1.0f, q2_b = 1.0f, q2_a = 1.0f;
    float q3_r = Q3_R / 255.0f, q3_g = Q3_G / 255.0f, q3_b = Q3_B / 255.0f, q3_a = 1.0f;

    std::vector<PipelineVertex> vertices = {
        // Quad 1 (Z = 0.5) - Indices 0..3
        { { -0.5f, -0.5f, 0.5f }, { 0, 0, 1 }, { q1_r, q1_g, q1_b, q1_a }, { 0, 0 }, { 0, 0 } },
        { {  0.5f, -0.5f, 0.5f }, { 0, 0, 1 }, { q1_r, q1_g, q1_b, q1_a }, { 1, 0 }, { 1, 0 } },
        { {  0.5f,  0.5f, 0.5f }, { 0, 0, 1 }, { q1_r, q1_g, q1_b, q1_a }, { 1, 1 }, { 1, 1 } },
        { { -0.5f,  0.5f, 0.5f }, { 0, 0, 1 }, { q1_r, q1_g, q1_b, q1_a }, { 0, 1 }, { 0, 1 } },

        // Quad 2 (Z = 0.8, occluded behind Quad 1) - Indices 4..7
        { { -0.7f, -0.7f, 0.8f }, { 0, 0, 1 }, { q2_r, q2_g, q2_b, q2_a }, { 0, 0 }, { 0, 0 } },
        { {  0.7f, -0.7f, 0.8f }, { 0, 0, 1 }, { q2_r, q2_g, q2_b, q2_a }, { 1, 0 }, { 1, 0 } },
        { {  0.7f,  0.7f, 0.8f }, { 0, 0, 1 }, { q2_r, q2_g, q2_b, q2_a }, { 1, 1 }, { 1, 1 } },
        { { -0.7f,  0.7f, 0.8f }, { 0, 0, 1 }, { q2_r, q2_g, q2_b, q2_a }, { 0, 1 }, { 0, 1 } },

        // Quad 3 (Z = 0.2, top-left in front of Quad 1) - Indices 8..11
        { { -0.8f, -0.8f, 0.2f }, { 0, 0, 1 }, { q3_r, q3_g, q3_b, q3_a }, { 0, 0 }, { 0, 0 } },
        { { -0.1f, -0.8f, 0.2f }, { 0, 0, 1 }, { q3_r, q3_g, q3_b, q3_a }, { 1, 0 }, { 1, 0 } },
        { { -0.1f, -0.1f, 0.2f }, { 0, 0, 1 }, { q3_r, q3_g, q3_b, q3_a }, { 1, 1 }, { 1, 1 } },
        { { -0.8f, -0.1f, 0.2f }, { 0, 0, 1 }, { q3_r, q3_g, q3_b, q3_a }, { 0, 1 }, { 0, 1 } },
    };

    std::vector<uint16_t> indices = {
        // Quad 1
        0, 1, 2,  2, 3, 0,
        // Quad 2
        4, 5, 6,  6, 7, 4,
        // Quad 3
        8, 9, 10, 10, 11, 8
    };

    VkDeviceSize vb_size = vertices.size() * sizeof(PipelineVertex);
    VkDeviceSize ib_size = indices.size() * sizeof(uint16_t);

    // Vertex Buffer
    VkBufferCreateInfo vb_ci = {};
    vb_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vb_ci.size = vb_size;
    vb_ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vb_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(device, &vb_ci, nullptr, &vertex_buffer), "vkCreateBuffer VB");

    VkMemoryRequirements vb_mem_reqs;
    vkGetBufferMemoryRequirements(device, vertex_buffer, &vb_mem_reqs);
    uint32_t vb_mem_type = find_memory_type(phys_dev, vb_mem_reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkMemoryAllocateInfo vb_mem_ai = {};
    vb_mem_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vb_mem_ai.allocationSize = vb_mem_reqs.size;
    vb_mem_ai.memoryTypeIndex = vb_mem_type;

    VkDeviceMemory vb_memory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(device, &vb_mem_ai, nullptr, &vb_memory), "vkAllocateMemory VB");
    VK_CHECK(vkBindBufferMemory(device, vertex_buffer, vb_memory, 0), "vkBindBufferMemory VB");

    void* vb_map = nullptr;
    VK_CHECK(vkMapMemory(device, vb_memory, 0, vb_size, 0, &vb_map), "vkMapMemory VB");
    memcpy(vb_map, vertices.data(), vb_size);
    vkUnmapMemory(device, vb_memory);

    // Index Buffer
    VkBufferCreateInfo ib_ci = {};
    ib_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ib_ci.size = ib_size;
    ib_ci.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    ib_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer index_buffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(device, &ib_ci, nullptr, &index_buffer), "vkCreateBuffer IB");

    VkMemoryRequirements ib_mem_reqs;
    vkGetBufferMemoryRequirements(device, index_buffer, &ib_mem_reqs);
    uint32_t ib_mem_type = find_memory_type(phys_dev, ib_mem_reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkMemoryAllocateInfo ib_mem_ai = {};
    ib_mem_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ib_mem_ai.allocationSize = ib_mem_reqs.size;
    ib_mem_ai.memoryTypeIndex = ib_mem_type;

    VkDeviceMemory ib_memory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(device, &ib_mem_ai, nullptr, &ib_memory), "vkAllocateMemory IB");
    VK_CHECK(vkBindBufferMemory(device, index_buffer, ib_memory, 0), "vkBindBufferMemory IB");

    void* ib_map = nullptr;
    VK_CHECK(vkMapMemory(device, ib_memory, 0, ib_size, 0, &ib_map), "vkMapMemory IB");
    memcpy(ib_map, indices.data(), ib_size);
    vkUnmapMemory(device, ib_memory);

    // 13. Create Staging Buffer for Readback (64 * 64 * 4 bytes)
    VkDeviceSize stage_size = TARGET_WIDTH * TARGET_HEIGHT * 4;
    VkBufferCreateInfo stage_ci = {};
    stage_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stage_ci.size = stage_size;
    stage_ci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    stage_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer stage_buffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(device, &stage_ci, nullptr, &stage_buffer), "vkCreateBuffer staging");

    VkMemoryRequirements stage_mem_reqs;
    vkGetBufferMemoryRequirements(device, stage_buffer, &stage_mem_reqs);
    uint32_t stage_mem_type = find_memory_type(phys_dev, stage_mem_reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkMemoryAllocateInfo stage_mem_ai = {};
    stage_mem_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stage_mem_ai.allocationSize = stage_mem_reqs.size;
    stage_mem_ai.memoryTypeIndex = stage_mem_type;

    VkDeviceMemory stage_memory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(device, &stage_mem_ai, nullptr, &stage_memory), "vkAllocateMemory staging");
    VK_CHECK(vkBindBufferMemory(device, stage_buffer, stage_memory, 0), "vkBindBufferMemory staging");

    // 14. Record and Execute Render Commands
    VkCommandBufferAllocateInfo cb_ai = {};
    cb_ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cb_ai.commandPool = cmd_pool;
    cb_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cb_ai.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(device, &cb_ai, &cmd), "vkAllocateCommandBuffers");

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info), "vkBeginCommandBuffer");

    VkClearValue clear_values[2] = {};
    clear_values[0].color.float32[0] = CLEAR_R / 255.0f;
    clear_values[0].color.float32[1] = CLEAR_G / 255.0f;
    clear_values[0].color.float32[2] = CLEAR_B / 255.0f;
    clear_values[0].color.float32[3] = CLEAR_A / 255.0f;
    clear_values[1].depthStencil.depth = 1.0f;
    clear_values[1].depthStencil.stencil = 0;

    VkRenderPassBeginInfo rp_bi = {};
    rp_bi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_bi.renderPass = render_pass;
    rp_bi.framebuffer = framebuffer;
    rp_bi.renderArea.offset = { 0, 0 };
    rp_bi.renderArea.extent = { TARGET_WIDTH, TARGET_HEIGHT };
    rp_bi.clearValueCount = 2;
    rp_bi.pClearValues = clear_values;

    vkCmdBeginRenderPass(cmd, &rp_bi, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkDeviceSize offsets[1] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer, offsets);
    vkCmdBindIndexBuffer(cmd, index_buffer, 0, VK_INDEX_TYPE_UINT16);

    // Draw Quad 1 (centered at Z=0.5)
    vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);

    // Draw Quad 2 (centered behind at Z=0.8, should be occluded/rejected by depth test)
    vkCmdDrawIndexed(cmd, 6, 1, 6, 0, 0);

    // Draw Quad 3 (top-left at Z=0.2, should pass depth test and overwrite)
    vkCmdDrawIndexed(cmd, 6, 1, 12, 0, 0);

    vkCmdEndRenderPass(cmd);

    // Copy color attachment image to staging buffer for CPU readback
    VkBufferImageCopy copy_region = {};
    copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_region.imageSubresource.layerCount = 1;
    copy_region.imageExtent = { TARGET_WIDTH, TARGET_HEIGHT, 1 };

    vkCmdCopyImageToBuffer(cmd, color_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           stage_buffer, 1, &copy_region);

    VK_CHECK(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE), "vkQueueSubmit");
    VK_CHECK(vkQueueWaitIdle(queue), "vkQueueWaitIdle");

    // 15. Verify Readback Pixels
    uint8_t* pixels = nullptr;
    VK_CHECK(vkMapMemory(device, stage_memory, 0, stage_size, 0, (void**)&pixels), "vkMapMemory staging read");

    auto get_pixel = [pixels](uint32_t x, uint32_t y, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a) {
        uint32_t idx = (y * TARGET_WIDTH + x) * 4;
        r = pixels[idx + 0];
        g = pixels[idx + 1];
        b = pixels[idx + 2];
        a = pixels[idx + 3];
    };

    // Check 1: Center pixel (32, 32) -> Must be Quad 1 color (R=0x11, G=0x22, B=0x33, A=0xFF)
    // Proves Quad 1 was rendered and Quad 2 (White, Z=0.8) was properly rejected by depth test!
    uint8_t cr, cg, cb, ca;
    get_pixel(32, 32, cr, cg, cb, ca);
    printf("Pixel (32, 32) Center: RGBA(%u, %u, %u, %u)\n", cr, cg, cb, ca);
    if (std::abs((int)cr - (int)Q1_R) > 2 ||
        std::abs((int)cg - (int)Q1_G) > 2 ||
        std::abs((int)cb - (int)Q1_B) > 2) {
        fprintf(stderr, "FAIL: Center pixel mismatch: expected RGBA(%u, %u, %u, 255), got RGBA(%u, %u, %u, %u)\n",
                Q1_R, Q1_G, Q1_B, cr, cg, cb, ca);
        return 1;
    }

    // Check 2: Top-left pixel (16, 16) inside Quad 3 (Z = 0.2) -> Must be Green (0, 255, 0, 255)
    // Proves closer geometry passed depth test and overwrote Quad 1!
    uint8_t tl_r, tl_g, tl_b, tl_a;
    get_pixel(16, 16, tl_r, tl_g, tl_b, tl_a);
    printf("Pixel (16, 16) Top-Left: RGBA(%u, %u, %u, %u)\n", tl_r, tl_g, tl_b, tl_a);
    if (std::abs((int)tl_r - (int)Q3_R) > 2 ||
        std::abs((int)tl_g - (int)Q3_G) > 2 ||
        std::abs((int)tl_b - (int)Q3_B) > 2) {
        fprintf(stderr, "FAIL: Top-left pixel mismatch: expected RGBA(%u, %u, %u, 255), got RGBA(%u, %u, %u, %u)\n",
                Q3_R, Q3_G, Q3_B, tl_r, tl_g, tl_b, tl_a);
        return 1;
    }

    // Check 3: Bottom-right background pixel (2, 60) outside quads -> Must be clear color (64, 0, 128, 255)
    uint8_t bg_r, bg_g, bg_b, bg_a;
    get_pixel(2, 60, bg_r, bg_g, bg_b, bg_a);
    printf("Pixel (2, 60) Background: RGBA(%u, %u, %u, %u)\n", bg_r, bg_g, bg_b, bg_a);
    if (std::abs((int)bg_r - (int)CLEAR_R) > 2 ||
        std::abs((int)bg_g - (int)CLEAR_G) > 2 ||
        std::abs((int)bg_b - (int)CLEAR_B) > 2) {
        fprintf(stderr, "FAIL: Background pixel mismatch: expected RGBA(%u, %u, %u, 255), got RGBA(%u, %u, %u, %u)\n",
                CLEAR_R, CLEAR_G, CLEAR_B, bg_r, bg_g, bg_b, bg_a);
        return 1;
    }

    vkUnmapMemory(device, stage_memory);

    // Cleanup
    vkDestroyBuffer(device, stage_buffer, nullptr);
    vkFreeMemory(device, stage_memory, nullptr);
    vkDestroyBuffer(device, vertex_buffer, nullptr);
    vkFreeMemory(device, vb_memory, nullptr);
    vkDestroyBuffer(device, index_buffer, nullptr);
    vkFreeMemory(device, ib_memory, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    vkDestroyShaderModule(device, vert_module, nullptr);
    vkDestroyShaderModule(device, frag_module, nullptr);
    vkDestroyFramebuffer(device, framebuffer, nullptr);
    vkDestroyRenderPass(device, render_pass, nullptr);
    vkDestroyImageView(device, color_view, nullptr);
    vkDestroyImage(device, color_image, nullptr);
    vkFreeMemory(device, color_memory, nullptr);
    vkDestroyImageView(device, depth_view, nullptr);
    vkDestroyImage(device, depth_image, nullptr);
    vkFreeMemory(device, depth_memory, nullptr);
    vkDestroyCommandPool(device, cmd_pool, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

    printf("Vulkan pipeline test OK\n");
    return 0;
}
