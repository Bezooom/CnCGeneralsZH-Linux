#include "test_harness.h"
#include "PAL_Vulkan.h"

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

extern "C" {
static PlatformWindowInfo s_stubWinInfo = {};
PlatformWindowInfo *Platform_GetWindowInfo(void) { return &s_stubWinInfo; }
}

// The embedded modules are what the game draws with. NVIDIA accepts a depth image and a matrix
// with no layout decorations and still produces a picture; lavapipe rejects the image and loads a
// zero matrix, so the same bytes draw nothing. The checks below are the ones that split the two.

static bool walk_complete(const uint32_t *m, uint32_t n)
{
	if (!m || n < 5 || m[0] != 0x07230203) return false;
	uint32_t i = 5;
	while (i < n) {
		uint32_t wc = m[i] >> 16;
		if (wc < 1 || i + wc > n) return false;
		i += wc;
	}
	return i == n;
}

static bool is_matrix_type(const uint32_t *m, uint32_t n, uint32_t id)
{
	for (uint32_t i = 5; i < n; ) {
		uint32_t wc = m[i] >> 16;
		if ((m[i] & 0xFFFFu) == 24 && wc >= 2 && m[i + 1] == id) return true;
		i += wc;
	}
	return false;
}

static bool member_decorated(const uint32_t *m, uint32_t n, uint32_t structId, uint32_t member,
                             uint32_t decoration, uint32_t *literal)
{
	for (uint32_t i = 5; i < n; ) {
		uint32_t wc = m[i] >> 16;
		if ((m[i] & 0xFFFFu) == 72 && wc >= 4 && m[i + 1] == structId && m[i + 2] == member
		    && m[i + 3] == decoration) {
			if (literal) *literal = wc >= 5 ? m[i + 4] : 0;
			return true;
		}
		i += wc;
	}
	return false;
}

TEST(pal_push_constant_matches_the_shader_block)
{
	CHECK(sizeof(PlatformVulkanPushConstants) == 124);
	CHECK(offsetof(PlatformVulkanPushConstants, materialColor) == 64);
	CHECK(offsetof(PlatformVulkanPushConstants, stageConfig) == 80);
	CHECK(offsetof(PlatformVulkanPushConstants, alphaConfig) == 116);

	uint32_t n = 0;
	const uint32_t *m = Platform_GetVertexShaderSPIRV(&n);
	CHECK(walk_complete(m, n));

	bool sawMatrixStruct = false;
	for (uint32_t i = 5; i < n; ) {
		uint32_t wc = m[i] >> 16;
		if ((m[i] & 0xFFFFu) == 30 && wc >= 3 && is_matrix_type(m, n, m[i + 2])) {
			uint32_t stride = 0;
			sawMatrixStruct = true;
			CHECK(member_decorated(m, n, m[i + 1], 0, 5, nullptr));
			CHECK(member_decorated(m, n, m[i + 1], 0, 7, &stride));
			CHECK(stride == 16);
		}
		i += wc;
	}
	CHECK(sawMatrixStruct);
}

TEST(pal_fragment_samples_a_color_image)
{
	uint32_t n = 0;
	const uint32_t *m = Platform_GetFragmentShaderSPIRV(&n);
	CHECK(walk_complete(m, n));

	bool sawImage = false;
	for (uint32_t i = 5; i < n; ) {
		uint32_t wc = m[i] >> 16;
		if ((m[i] & 0xFFFFu) == 25) {
			// Result, sampled type, dim, depth, arrayed, multisampled, sampled, format.
			CHECK(wc >= 9);
			CHECK(m[i + 3] == 1);
			CHECK(m[i + 4] == 0);
			CHECK(m[i + 7] == 1);
			sawImage = true;
		}
		i += wc;
	}
	CHECK(sawImage);
}

static uint32_t find_mem(VkPhysicalDevice pd, uint32_t filter, VkMemoryPropertyFlags props)
{
	VkPhysicalDeviceMemoryProperties mp;
	vkGetPhysicalDeviceMemoryProperties(pd, &mp);
	for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
		if ((filter & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props) return i;
	}
	return UINT32_MAX;
}

static bool readback_texel(void)
{
	VkApplicationInfo app = {};
	app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app.apiVersion = VK_API_VERSION_1_1;
	VkInstanceCreateInfo ici = {};
	ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ici.pApplicationInfo = &app;
	VkInstance instance = VK_NULL_HANDLE;
	if (vkCreateInstance(&ici, nullptr, &instance) != VK_SUCCESS) {
		fprintf(stderr, "no vulkan instance\n");
		return false;
	}

	uint32_t nd = 0;
	vkEnumeratePhysicalDevices(instance, &nd, nullptr);
	if (nd == 0) {
		fprintf(stderr, "no vulkan device\n");
		return false;
	}
	std::vector<VkPhysicalDevice> devs(nd);
	vkEnumeratePhysicalDevices(instance, &nd, devs.data());
	VkPhysicalDevice pd = devs[0];
	for (uint32_t i = 0; i < nd; ++i) {
		VkPhysicalDeviceProperties p;
		vkGetPhysicalDeviceProperties(devs[i], &p);
		if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) { pd = devs[i]; break; }
	}

	uint32_t qn = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(pd, &qn, nullptr);
	std::vector<VkQueueFamilyProperties> qf(qn);
	vkGetPhysicalDeviceQueueFamilyProperties(pd, &qn, qf.data());
	uint32_t fam = UINT32_MAX;
	for (uint32_t i = 0; i < qn; ++i) {
		if (qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { fam = i; break; }
	}
	if (fam == UINT32_MAX) return false;

	float prio = 1.0f;
	VkDeviceQueueCreateInfo qci = {};
	qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	qci.queueFamilyIndex = fam;
	qci.queueCount = 1;
	qci.pQueuePriorities = &prio;
	VkDeviceCreateInfo dci = {};
	dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	dci.queueCreateInfoCount = 1;
	dci.pQueueCreateInfos = &qci;
	VkDevice device = VK_NULL_HANDLE;
	if (vkCreateDevice(pd, &dci, nullptr, &device) != VK_SUCCESS) return false;
	VkQueue queue = VK_NULL_HANDLE;
	vkGetDeviceQueue(device, fam, 0, &queue);

	const uint32_t W = 64, H = 64;
	VkImageCreateInfo ii = {};
	ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ii.imageType = VK_IMAGE_TYPE_2D;
	ii.format = VK_FORMAT_R8G8B8A8_UNORM;
	ii.extent = { W, H, 1 };
	ii.mipLevels = 1;
	ii.arrayLayers = 1;
	ii.samples = VK_SAMPLE_COUNT_1_BIT;
	ii.tiling = VK_IMAGE_TILING_OPTIMAL;
	ii.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	VkImage img = VK_NULL_HANDLE;
	if (vkCreateImage(device, &ii, nullptr, &img) != VK_SUCCESS) return false;
	VkMemoryRequirements mr;
	vkGetImageMemoryRequirements(device, img, &mr);
	uint32_t mt = find_mem(pd, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (mt == UINT32_MAX) mt = find_mem(pd, mr.memoryTypeBits, 0);
	VkMemoryAllocateInfo mai = {};
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = mr.size;
	mai.memoryTypeIndex = mt;
	VkDeviceMemory imgmem = VK_NULL_HANDLE;
	if (vkAllocateMemory(device, &mai, nullptr, &imgmem) != VK_SUCCESS) return false;
	vkBindImageMemory(device, img, imgmem, 0);

	VkImageViewCreateInfo iv = {};
	iv.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	iv.image = img;
	iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
	iv.format = VK_FORMAT_R8G8B8A8_UNORM;
	iv.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	VkImageView view = VK_NULL_HANDLE;
	if (vkCreateImageView(device, &iv, nullptr, &view) != VK_SUCCESS) return false;

	VkAttachmentDescription att = {};
	att.format = VK_FORMAT_R8G8B8A8_UNORM;
	att.samples = VK_SAMPLE_COUNT_1_BIT;
	att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	att.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkAttachmentReference ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkSubpassDescription sp = {};
	sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sp.colorAttachmentCount = 1;
	sp.pColorAttachments = &ref;
	VkRenderPassCreateInfo rpi = {};
	rpi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rpi.attachmentCount = 1;
	rpi.pAttachments = &att;
	rpi.subpassCount = 1;
	rpi.pSubpasses = &sp;
	VkRenderPass rp = VK_NULL_HANDLE;
	if (vkCreateRenderPass(device, &rpi, nullptr, &rp) != VK_SUCCESS) return false;

	VkFramebufferCreateInfo fbi = {};
	fbi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbi.renderPass = rp;
	fbi.attachmentCount = 1;
	fbi.pAttachments = &view;
	fbi.width = W;
	fbi.height = H;
	fbi.layers = 1;
	VkFramebuffer fb = VK_NULL_HANDLE;
	if (vkCreateFramebuffer(device, &fbi, nullptr, &fb) != VK_SUCCESS) return false;

	// The canonical 60-byte vertex: position, uv0, colour, uv1, and the two projected coordinates.
	struct CV { float p[3]; float uv[2]; float c[4]; float uv1[2]; float prj0[2]; float prj1[2]; };
	CV verts[6] = {
		{ { -1, -1, 0 }, { 0, 0 }, { 1, 1, 1, 1 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
		{ {  1, -1, 0 }, { 1, 0 }, { 1, 1, 1, 1 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
		{ {  1,  1, 0 }, { 1, 1 }, { 1, 1, 1, 1 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
		{ { -1, -1, 0 }, { 0, 0 }, { 1, 1, 1, 1 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
		{ {  1,  1, 0 }, { 1, 1 }, { 1, 1, 1, 1 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
		{ { -1,  1, 0 }, { 0, 1 }, { 1, 1, 1, 1 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
	};
	VkBufferCreateInfo bi = {};
	bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bi.size = sizeof(verts);
	bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	VkBuffer vb = VK_NULL_HANDLE;
	if (vkCreateBuffer(device, &bi, nullptr, &vb) != VK_SUCCESS) return false;
	vkGetBufferMemoryRequirements(device, vb, &mr);
	mt = find_mem(pd, mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	if (mt == UINT32_MAX) return false;
	mai.allocationSize = mr.size;
	mai.memoryTypeIndex = mt;
	VkDeviceMemory vbmem = VK_NULL_HANDLE;
	if (vkAllocateMemory(device, &mai, nullptr, &vbmem) != VK_SUCCESS) return false;
	vkBindBufferMemory(device, vb, vbmem, 0);
	void *mapped = nullptr;
	vkMapMemory(device, vbmem, 0, sizeof(verts), 0, &mapped);
	memcpy(mapped, verts, sizeof(verts));
	vkUnmapMemory(device, vbmem);

	bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bi.size = W * H * 4;
	VkBuffer sb = VK_NULL_HANDLE;
	if (vkCreateBuffer(device, &bi, nullptr, &sb) != VK_SUCCESS) return false;
	vkGetBufferMemoryRequirements(device, sb, &mr);
	mt = find_mem(pd, mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	mai.allocationSize = mr.size;
	mai.memoryTypeIndex = mt;
	VkDeviceMemory sbmem = VK_NULL_HANDLE;
	if (vkAllocateMemory(device, &mai, nullptr, &sbmem) != VK_SUCCESS) return false;
	vkBindBufferMemory(device, sb, sbmem, 0);

	ii.extent = { 1, 1, 1 };
	ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	VkImage tex = VK_NULL_HANDLE;
	if (vkCreateImage(device, &ii, nullptr, &tex) != VK_SUCCESS) return false;
	vkGetImageMemoryRequirements(device, tex, &mr);
	mt = find_mem(pd, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (mt == UINT32_MAX) mt = find_mem(pd, mr.memoryTypeBits, 0);
	mai.allocationSize = mr.size;
	mai.memoryTypeIndex = mt;
	VkDeviceMemory texmem = VK_NULL_HANDLE;
	if (vkAllocateMemory(device, &mai, nullptr, &texmem) != VK_SUCCESS) return false;
	vkBindImageMemory(device, tex, texmem, 0);

	const unsigned char texel[4] = { 10, 20, 30, 40 };
	VkBufferCreateInfo tbi = {};
	tbi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	tbi.size = 4;
	tbi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	VkBuffer tbuf = VK_NULL_HANDLE;
	if (vkCreateBuffer(device, &tbi, nullptr, &tbuf) != VK_SUCCESS) return false;
	vkGetBufferMemoryRequirements(device, tbuf, &mr);
	mt = find_mem(pd, mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	mai.allocationSize = mr.size;
	mai.memoryTypeIndex = mt;
	VkDeviceMemory tmem = VK_NULL_HANDLE;
	if (vkAllocateMemory(device, &mai, nullptr, &tmem) != VK_SUCCESS) return false;
	vkBindBufferMemory(device, tbuf, tmem, 0);
	unsigned char *tpx = nullptr;
	vkMapMemory(device, tmem, 0, 4, 0, (void **)&tpx);
	memcpy(tpx, texel, 4);
	vkUnmapMemory(device, tmem);

	VkImageViewCreateInfo tiv = {};
	tiv.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	tiv.image = tex;
	tiv.viewType = VK_IMAGE_VIEW_TYPE_2D;
	tiv.format = VK_FORMAT_R8G8B8A8_UNORM;
	tiv.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	VkImageView texview = VK_NULL_HANDLE;
	if (vkCreateImageView(device, &tiv, nullptr, &texview) != VK_SUCCESS) return false;
	VkSamplerCreateInfo sci = {};
	sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sci.magFilter = VK_FILTER_NEAREST;
	sci.minFilter = VK_FILTER_NEAREST;
	VkSampler sampler = VK_NULL_HANDLE;
	if (vkCreateSampler(device, &sci, nullptr, &sampler) != VK_SUCCESS) return false;

	VkDescriptorSetLayoutBinding db = {};
	db.binding = 0;
	db.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	db.descriptorCount = 1;
	db.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	VkDescriptorSetLayoutCreateInfo dli = {};
	dli.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dli.bindingCount = 1;
	dli.pBindings = &db;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	if (vkCreateDescriptorSetLayout(device, &dli, nullptr, &dsl) != VK_SUCCESS) return false;
	VkDescriptorPoolSize dps = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 };
	VkDescriptorPoolCreateInfo dpi = {};
	dpi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpi.maxSets = 4;
	dpi.poolSizeCount = 1;
	dpi.pPoolSizes = &dps;
	VkDescriptorPool pool = VK_NULL_HANDLE;
	if (vkCreateDescriptorPool(device, &dpi, nullptr, &pool) != VK_SUCCESS) return false;
	// One single-sampler set per texture stage, as the game's pipeline layout has: sets 0..3 are
	// stage 0..3. Every stage samples the same texel here.
	VkDescriptorSetLayout setLayouts[4] = { dsl, dsl, dsl, dsl };
	VkDescriptorSetAllocateInfo dsai = {};
	dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	dsai.descriptorPool = pool;
	dsai.descriptorSetCount = 4;
	dsai.pSetLayouts = setLayouts;
	VkDescriptorSet sets[4] = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE };
	if (vkAllocateDescriptorSets(device, &dsai, sets) != VK_SUCCESS) return false;
	VkDescriptorImageInfo dii = { sampler, texview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	VkWriteDescriptorSet wd[4] = {};
	for (int stage = 0; stage < 4; ++stage) {
		wd[stage].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		wd[stage].dstSet = sets[stage];
		wd[stage].dstBinding = 0;
		wd[stage].descriptorCount = 1;
		wd[stage].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		wd[stage].pImageInfo = &dii;
	}
	vkUpdateDescriptorSets(device, 4, wd, 0, nullptr);

	VkPushConstantRange pcr = {};
	pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pcr.size = sizeof(PlatformVulkanPushConstants);
	VkPipelineLayoutCreateInfo pli = {};
	pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pli.setLayoutCount = 4;
	pli.pSetLayouts = setLayouts;
	pli.pushConstantRangeCount = 1;
	pli.pPushConstantRanges = &pcr;
	VkPipelineLayout pl = VK_NULL_HANDLE;
	if (vkCreatePipelineLayout(device, &pli, nullptr, &pl) != VK_SUCCESS) return false;

	uint32_t vw = 0, fw = 0;
	const uint32_t *vs = Platform_GetVertexShaderSPIRV(&vw);
	const uint32_t *fs = Platform_GetFragmentShaderSPIRV(&fw);
	VkShaderModuleCreateInfo smi = {};
	smi.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	smi.codeSize = vw * 4;
	smi.pCode = vs;
	VkShaderModule vsm = VK_NULL_HANDLE;
	if (vkCreateShaderModule(device, &smi, nullptr, &vsm) != VK_SUCCESS) return false;
	smi.codeSize = fw * 4;
	smi.pCode = fs;
	VkShaderModule fsm = VK_NULL_HANDLE;
	if (vkCreateShaderModule(device, &smi, nullptr, &fsm) != VK_SUCCESS) return false;

	VkVertexInputBindingDescription bind = { 0, 60, VK_VERTEX_INPUT_RATE_VERTEX };
	VkVertexInputAttributeDescription attrs[6] = {
		{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
		{ 1, 0, VK_FORMAT_R32G32_SFLOAT, 12 },
		{ 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 20 },
		{ 3, 0, VK_FORMAT_R32G32_SFLOAT, 36 },
		{ 4, 0, VK_FORMAT_R32G32_SFLOAT, 44 },
		{ 5, 0, VK_FORMAT_R32G32_SFLOAT, 52 },
	};
	VkPipelineVertexInputStateCreateInfo vin = {};
	vin.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vin.vertexBindingDescriptionCount = 1;
	vin.pVertexBindingDescriptions = &bind;
	vin.vertexAttributeDescriptionCount = 6;
	vin.pVertexAttributeDescriptions = attrs;
	VkPipelineInputAssemblyStateCreateInfo ia = {};
	ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPipelineViewportStateCreateInfo vp = {};
	vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vp.viewportCount = 1;
	vp.scissorCount = 1;
	VkPipelineRasterizationStateCreateInfo rs = {};
	rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;
	rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rs.lineWidth = 1.0f;
	VkPipelineMultisampleStateCreateInfo ms = {};
	ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineColorBlendAttachmentState cba = {};
	cba.colorWriteMask = 0xF;
	VkPipelineColorBlendStateCreateInfo cb = {};
	cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	cb.attachmentCount = 1;
	cb.pAttachments = &cba;
	VkDynamicState dyn[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo di = {};
	di.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	di.dynamicStateCount = 2;
	di.pDynamicStates = dyn;
	VkPipelineShaderStageCreateInfo stages[2] = {};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vsm;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = fsm;
	stages[1].pName = "main";
	VkGraphicsPipelineCreateInfo gpi = {};
	gpi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gpi.stageCount = 2;
	gpi.pStages = stages;
	gpi.pVertexInputState = &vin;
	gpi.pInputAssemblyState = &ia;
	gpi.pViewportState = &vp;
	gpi.pRasterizationState = &rs;
	gpi.pMultisampleState = &ms;
	gpi.pColorBlendState = &cb;
	gpi.pDynamicState = &di;
	gpi.layout = pl;
	gpi.renderPass = rp;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkResult pres = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpi, nullptr, &pipeline);
	if (pres != VK_SUCCESS) {
		fprintf(stderr, "pipeline=%d\n", (int)pres);
		return false;
	}

	VkCommandPoolCreateInfo cpi = {};
	cpi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cpi.queueFamilyIndex = fam;
	VkCommandPool cmdpool = VK_NULL_HANDLE;
	if (vkCreateCommandPool(device, &cpi, nullptr, &cmdpool) != VK_SUCCESS) return false;
	VkCommandBufferAllocateInfo cbai = {};
	cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbai.commandPool = cmdpool;
	cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cbai.commandBufferCount = 1;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	if (vkAllocateCommandBuffers(device, &cbai, &cmd) != VK_SUCCESS) return false;
	VkCommandBufferBeginInfo begin = {};
	begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	if (vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS) return false;

	VkImageMemoryBarrier tb = {};
	tb.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	tb.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	tb.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	tb.image = tex;
	tb.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	tb.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	tb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	tb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
	                     0, 0, nullptr, 0, nullptr, 1, &tb);
	VkBufferImageCopy tcopy = {};
	tcopy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	tcopy.imageExtent = { 1, 1, 1 };
	vkCmdCopyBufferToImage(cmd, tbuf, tex, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &tcopy);
	tb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	tb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	tb.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	tb.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
	                     0, 0, nullptr, 0, nullptr, 1, &tb);

	VkClearValue clear;
	clear.color = { { 0.0f, 0.0f, 1.0f, 1.0f } };
	VkRenderPassBeginInfo rpb = {};
	rpb.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpb.renderPass = rp;
	rpb.framebuffer = fb;
	rpb.renderArea = { { 0, 0 }, { W, H } };
	rpb.clearValueCount = 1;
	rpb.pClearValues = &clear;
	vkCmdBeginRenderPass(cmd, &rpb, VK_SUBPASS_CONTENTS_INLINE);
	VkViewport viewport = { 0, 0, (float)W, (float)H, 0.0f, 1.0f };
	vkCmdSetViewport(cmd, 0, 1, &viewport);
	VkRect2D sc = { { 0, 0 }, { W, H } };
	vkCmdSetScissor(cmd, 0, 1, &sc);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	VkDeviceSize off = 0;
	vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &off);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pl, 0, 4, sets, 0, nullptr);
	PlatformVulkanPushConstants pc = {};
	pc.mvp[0] = 1;
	pc.mvp[5] = -1;
	pc.mvp[10] = 1;
	pc.mvp[15] = 1;
	pc.materialColor[0] = pc.materialColor[1] = pc.materialColor[2] = pc.materialColor[3] = 1;
	// Stage 0 modulates the texture by the running value (the diffuse colour); stages 1..3 are
	// disabled, and the alpha test is off - the default fixed-function state.
	const uint32_t stage0 = (uint32_t)4 /* MODULATE */ | ((uint32_t)2 /* TEXTURE */ << 8) |
	                        ((uint32_t)1 /* CURRENT */ << 16);
	const uint32_t disabled = 1u;   // D3DTOP_DISABLE
	uint32_t packed[4] = { stage0, disabled, disabled, disabled };
	for (int stage = 0; stage < 4; ++stage) memcpy(&pc.stageConfig[stage], &packed[stage], 4);
	// Stage 0 takes its alpha from the texture (D3DTOP_SELECTARG1 over D3DTA_TEXTURE), which is how
	// a fixed-function stage gets a texel alpha at all: the colour operation writes RGB only.
	uint32_t alpha0 = (uint32_t)2 /* SELECTARG1 */ | ((uint32_t)2 /* TEXTURE */ << 8);
	memcpy(&pc.stageAlphaConfig[0], &alpha0, 4);
	for (int stage = 1; stage < 4; ++stage) {
		uint32_t alpha = 1u;   // ALPHAOP disable
		memcpy(&pc.stageAlphaConfig[stage], &alpha, 4);
	}
	const uint32_t white = 0xFFFFFFFFu;
	memcpy(&pc.textureFactor, &white, 4);
	vkCmdPushConstants(cmd, pl, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
	                   0, sizeof(pc), &pc);
	vkCmdDraw(cmd, 6, 1, 0, 0);
	vkCmdEndRenderPass(cmd);

	VkImageMemoryBarrier bar = {};
	bar.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	bar.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	bar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	bar.image = img;
	bar.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	bar.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	bar.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	bar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
	                     0, 0, nullptr, 0, nullptr, 1, &bar);
	VkBufferImageCopy bic = {};
	bic.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	bic.imageExtent = { W, H, 1 };
	vkCmdCopyImageToBuffer(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sb, 1, &bic);
	if (vkEndCommandBuffer(cmd) != VK_SUCCESS) return false;
	VkSubmitInfo si = {};
	si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cmd;
	if (vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE) != VK_SUCCESS) return false;
	if (vkQueueWaitIdle(queue) != VK_SUCCESS) return false;

	unsigned char *px = nullptr;
	if (vkMapMemory(device, sbmem, 0, W * H * 4, 0, (void **)&px) != VK_SUCCESS) return false;
	const int spots[2][2] = { { 32, 32 }, { 2, 61 } };
	bool ok = true;
	for (int s = 0; s < 2; ++s) {
		unsigned char *c = px + (spots[s][1] * W + spots[s][0]) * 4;
		fprintf(stderr, "pixel %d,%d: %u %u %u %u\n", spots[s][0], spots[s][1], c[0], c[1], c[2], c[3]);
		if (c[0] != 10 || c[1] != 20 || c[2] != 30 || c[3] != 40) ok = false;
	}
	return ok;
}

TEST(pal_shaders_read_back_the_uploaded_texel)
{
	CHECK(readback_texel());
}
