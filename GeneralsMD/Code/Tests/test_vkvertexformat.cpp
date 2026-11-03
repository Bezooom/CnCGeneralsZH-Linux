/*
 * The one vertex layout the Vulkan pipeline reads, and the decode from every D3D9 flexible vertex
 * format into it.
 *
 * Two failure modes are what these tests aim at. A wrong offset reads the normal as the colour,
 * which paints the model green. Reading a packed D3DCOLOR as a float gives a number near zero,
 * which paints it black. Both look like a working renderer until a real mesh is drawn.
 */

#include "test_harness.h"
#include "ww3d_d3d9.h"
#include "vkvertexformat.h"
static const DWORD FVF_XYZ = D3DFVF_XYZ;
static const DWORD FVF_XYZN = D3DFVF_XYZ | D3DFVF_NORMAL;
static const DWORD FVF_XYZDUV1 = D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE;
static const DWORD FVF_XYZNDUV2 = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX2 | D3DFVF_DIFFUSE;
static const DWORD FVF_XYZNUV2DMAP = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX3
    | D3DFVF_TEXCOORDSIZE1(0) | D3DFVF_TEXCOORDSIZE4(1) | D3DFVF_TEXCOORDSIZE2(2);
static const DWORD FVF_XYZRHW_DU = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

TEST(vk_canonical_layout_matches_the_pipeline)
{
    // The pipeline is told the offsets by hand in PAL_Vulkan.cpp. If the decoder's struct and the
    // pipeline's stride stop agreeing, every vertex is read from the wrong bytes.
    CHECK_EQ(sizeof(VKCanonicalVertex), (size_t)VK_CANONICAL_VERTEX_STRIDE);
    CHECK_EQ(VK_CANONICAL_VERTEX_STRIDE, PLATFORM_CANONICAL_VERTEX_STRIDE);
}

TEST(vklayout_offsets_follow_the_field_order_d3d9_packs)
{
    VKVertexLayout layout;

    CHECK(VKLayout_From_FVF(FVF_XYZ, layout));
    CHECK_EQ(layout.positionOffset, 0);
    CHECK_EQ(layout.normalOffset, -1);
    CHECK_EQ(layout.colorOffset, -1);
    CHECK_EQ(layout.uvOffset, -1);
    CHECK_EQ(layout.stride, 12u);

    CHECK(VKLayout_From_FVF(FVF_XYZN, layout));
    CHECK_EQ(layout.normalOffset, 12);
    CHECK_EQ(layout.stride, 24u);

    CHECK(VKLayout_From_FVF(FVF_XYZDUV1, layout));
    CHECK_EQ(layout.positionOffset, 0);
    CHECK_EQ(layout.colorOffset, 12);
    CHECK_EQ(layout.uvOffset, 16);
    CHECK_EQ(layout.stride, 24u);

    CHECK(VKLayout_From_FVF(FVF_XYZNDUV2, layout));
    CHECK_EQ(layout.normalOffset, 12);
    CHECK_EQ(layout.colorOffset, 24);
    CHECK_EQ(layout.uvOffset, 28);
    CHECK_EQ(layout.stride, 44u);   // 12 + 12 + 4 + 8 + 8: both sets, not only the one the pipeline reads
    CHECK(VKLayout_From_FVF(FVF_XYZNDUV2, layout, 1));
    CHECK_EQ(layout.colorOffset, 24);
    CHECK_EQ(layout.uvOffset, 36);   // set 1 starts after set 0 (28 + 8)
    CHECK_EQ(layout.stride, 44u);

    // Screen-space vertices carry a w, so the fields after position start four bytes later.
    CHECK(VKLayout_From_FVF(FVF_XYZRHW_DU, layout));
    CHECK_EQ(layout.colorOffset, 16);
    CHECK_EQ(layout.uvOffset, 20);
    CHECK_EQ(layout.stride, 28u);
}

TEST(vklayout_texture_coordinate_sizes_use_the_two_bit_encoding)
{
    VKVertexLayout layout;
    CHECK(VKLayout_From_FVF(FVF_XYZNUV2DMAP, layout));
    CHECK_EQ(layout.uvFloatCount, 1u);   // set 0 is one float, not the default two
    CHECK_EQ(layout.colorOffset, -1);   // this format carries no colour
    CHECK_EQ(layout.stride, 52u);        // 12 + 12 + 4 + 16 + 8
}

TEST(vklayout_point_size_is_not_a_texture_coordinate_set)
{
    // D3DFVF_PSIZE has to sit below the texture-count bits. Read as part of them, a point-size
    // vertex looks like D3DFVF_TEX2 and the decoder skips four bytes the engine never wrote.
    VKVertexLayout layout;
    CHECK(VKLayout_From_FVF(D3DFVF_XYZ | D3DFVF_PSIZE | D3DFVF_TEX1, layout));
    CHECK_EQ(layout.uvOffset, 16);
    CHECK_EQ(layout.stride, 24u);   // 12 + 4 + 8
}

TEST(vklayout_rejects_blend_weight_forms_the_engine_never_fills)
{
    VKVertexLayout layout;
    // D3DFVF_XYZB1 is inside D3DFVF_POSITION_MASK but the engine never writes weights, so the
    // decoder cannot know how many bytes to skip.
    CHECK(!VKLayout_From_FVF(0x006 | D3DFVF_DIFFUSE | D3DFVF_TEX1, layout));
}

TEST(vkdecode_reads_the_normal_as_a_normal_and_not_as_the_colour)
{
    VKVertexLayout layout;
    CHECK(VKLayout_From_FVF(FVF_XYZNDUV2, layout));

    unsigned char vertex[44] = {};
    float position[3] = { 1.0f, 2.0f, 3.0f };
    float normal[3] = { 0.0f, 1.0f, 0.0f };
    float uv[2] = { 0.25f, 0.75f };
    memcpy(vertex + 0, position, sizeof(position));
    memcpy(vertex + 12, normal, sizeof(normal));
    vertex[24] = 255;   // B
    vertex[25] = 0;     // G
    vertex[26] = 0;     // R
    vertex[27] = 255;   // A
    memcpy(vertex + 28, uv, sizeof(uv));

    VKCanonicalVertex out;
    VKDecodeVertex(vertex, layout, out);

    CHECK_NEAR(out.position[0], 1.0f, 1e-6f);
    CHECK_NEAR(out.position[1], 2.0f, 1e-6f);
    CHECK_NEAR(out.position[2], 3.0f, 1e-6f);
    CHECK_NEAR(out.uv[0], 0.25f, 1e-6f);
    CHECK_NEAR(out.uv[1], 0.75f, 1e-6f);

    // A decoder that took the normal as the colour would report green here.
    CHECK_NEAR(out.color[0], 0.0f, 1e-6f);
    CHECK_NEAR(out.color[1], 0.0f, 1e-6f);
    CHECK_NEAR(out.color[2], 1.0f, 1e-6f);
    CHECK_NEAR(out.color[3], 1.0f, 1e-6f);
}

TEST(vkdecode_advances_by_the_whole_vertex_not_just_the_set_the_pipeline_reads)
{
    VKVertexLayout layout;
    CHECK(VKLayout_From_FVF(FVF_XYZNDUV2, layout));

    unsigned char buffer[2 * 44] = {};
    float first[3] = { 1.0f, 0.0f, 0.0f };
    float second[3] = { 0.0f, 1.0f, 0.0f };
    memcpy(buffer, first, sizeof(first));
    memcpy(buffer + 44, second, sizeof(second));

    VKCanonicalVertex out;
    VKDecodeVertex(buffer, layout, out);
    CHECK_NEAR(out.position[0], 1.0f, 1e-6f);
    VKDecodeVertex(buffer + 44, layout, out);
    CHECK_NEAR(out.position[1], 1.0f, 1e-6f);
}

TEST(vkdecode_unpacks_packed_bgra_bytes)
{
    VKVertexLayout layout;
    CHECK(VKLayout_From_FVF(FVF_XYZDUV1, layout));

    unsigned char vertex[24] = {};
    D3DCOLOR color = 0xFF0000FF;   // A=FF R=00 G=00 B=FF
    memcpy(vertex + 12, &color, sizeof(color));

    VKCanonicalVertex out;
    VKDecodeVertex(vertex, layout, out);

    CHECK_NEAR(out.color[0], 0.0f, 1e-6f);
    CHECK_NEAR(out.color[1], 0.0f, 1e-6f);
    CHECK_NEAR(out.color[2], 1.0f, 1e-6f);
    CHECK_NEAR(out.color[3], 1.0f, 1e-6f);

    // The same four bytes read as a float is not a colour at all: the top byte is the blue channel,
    // so the float is NaN for an opaque colour and a denormal for a transparent one. A decoder that
    // skips the unpack hands the pipeline that number and the model renders black or garbage.
    float asFloat = 0.0f;
    memcpy(&asFloat, vertex + 12, sizeof(asFloat));
    CHECK(asFloat != out.color[2]);
}

TEST(vkdecode_fills_missing_fields_with_the_disabled_stage_values)
{
    VKVertexLayout layout;

    // No colour: a fixed-function stage with the diffuse input disabled passes the material
    // through, which is white when the material is white.
    CHECK(VKLayout_From_FVF(FVF_XYZN, layout));
    unsigned char vertex[24] = {};
    VKCanonicalVertex out;
    VKDecodeVertex(vertex, layout, out);
    CHECK_NEAR(out.color[0], 1.0f, 1e-6f);
    CHECK_NEAR(out.color[3], 1.0f, 1e-6f);

    // One texture coordinate: the second component is not read, so it stays zero.
    CHECK(VKLayout_From_FVF(FVF_XYZ | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE1(0), layout));
    float u = 0.5f;
    memcpy(vertex + 12, &u, sizeof(u));
    VKDecodeVertex(vertex, layout, out);
    CHECK_NEAR(out.uv[0], 0.5f, 1e-6f);
    CHECK_NEAR(out.uv[1], 0.0f, 1e-6f);
}

/*
 * The draw path is the consumer-visible half: the engine hands the shim a vertex buffer and the FVF
 * it set, and the shim must hand the pipeline the decoded vertex plus the matrix. The PAL layer is
 * stubbed here because a unit test has no device; the stub records what it was given.
 */

static PlatformVulkanPushConstants g_capturedPushConstants;
static std::vector<VKCanonicalVertex> g_capturedVertices;
static int g_capturedBlendMode = -1;
static void *g_capturedBoundTexture = nullptr;
static void *g_capturedStageTextures[4] = { nullptr, nullptr, nullptr, nullptr };
static uint32_t g_capturedSamplerKeys[4] = { 0, 0, 0, 0 };

extern "C" {
void Platform_BindVulkanTexture(void *tex)
{
    g_capturedBoundTexture = tex;
}

void Platform_BindVulkanTextures(void *const textures[4], const uint32_t *samplerKeys)
{
    for (int stage = 0; stage < 4; ++stage) {
        g_capturedStageTextures[stage] = textures ? textures[stage] : nullptr;
        g_capturedSamplerKeys[stage] = samplerKeys ? samplerKeys[stage] : 0;
    }
}

uint32_t Platform_MakeSamplerKey(uint32_t addressU, uint32_t addressV, uint32_t minFilter,
                                 uint32_t magFilter, uint32_t mipFilter, int mipsReady)
{
    uint32_t key = (addressU & 0x7u) | ((addressV & 0x7u) << 3) | ((minFilter & 0x7u) << 6) |
                   ((magFilter & 0x7u) << 9) | ((mipFilter & 0x7u) << 12);
    if (mipsReady) key |= PLATFORM_SAMPLER_MIPS_READY;
    return key;
}

void Platform_SetVulkanPushConstants(const PlatformVulkanPushConstants *pc)
{
    g_capturedPushConstants = *pc;
}

bool Platform_DrawVulkanCanonical(const float *canonicalVertices, uint32_t vertexCount,
                                  const uint16_t *, uint32_t)
{
    const VKCanonicalVertex *decoded = (const VKCanonicalVertex *)canonicalVertices;
    g_capturedVertices.assign(decoded, decoded + vertexCount);
    return true;
}
PlatformVulkanContext *Platform_GetVulkanContext(void) { static PlatformVulkanContext ctx = {}; ctx.width = 800; ctx.height = 600; return &ctx; }
void Platform_SetVulkanBlendState(int mode) { g_capturedBlendMode = mode; }
void Platform_SetVulkanDepthState(int, int, int) {}
}


TEST(draw_canonical_hands_the_pipeline_the_decoded_vertex_and_the_transposed_matrix)
{
    IDirect3DDevice9 device;
    device.SetFVF(FVF_XYZNDUV2);
    device.SetRenderState(D3DRS_LIGHTING, FALSE);

    D3DMATRIX identity = { { { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 } } };
    device.SetTransform(D3DTS_WORLD, &identity);
    device.SetTransform(D3DTS_VIEW, &identity);
    device.SetTransform(D3DTS_PROJECTION, &identity);

    D3DMATERIAL9 material = { {1,1,1,1}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, 0 };
    device.SetMaterial(&material);

    unsigned char triangle[3 * 44] = {};
    for (int v = 0; v < 3; ++v) {
        unsigned char *p = triangle + v * 44;
        float position[3] = { (float)v, 2.0f, 3.0f };
        float normal[3] = { 0.0f, 1.0f, 0.0f };
        float uv[2] = { 0.25f, 0.75f };
        memcpy(p, position, sizeof(position));
        memcpy(p + 12, normal, sizeof(normal));
        p[24] = 255; p[25] = 0; p[26] = 0; p[27] = 255;   // BGRA
        memcpy(p + 28, uv, sizeof(uv));
    }

    g_capturedVertices.clear();
    device.DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, triangle, 44);

    CHECK_EQ(g_capturedVertices.size(), (size_t)3);
    CHECK_NEAR(g_capturedVertices[0].position[0], 0.0f, 1e-6f);
    CHECK_NEAR(g_capturedVertices[2].position[0], 2.0f, 1e-6f);
    CHECK_NEAR(g_capturedVertices[0].color[2], 1.0f, 1e-6f);

    // The shader multiplies the matrix by a column vector, so the push constant is the transpose of
    // the row-major D3D9 product, with the y row negated for Vulkan's flipped NDC y.
    CHECK_NEAR(g_capturedPushConstants.mvp[0], 1.0f, 1e-6f);
    CHECK_NEAR(g_capturedPushConstants.mvp[5], -1.0f, 1e-6f);
    CHECK_NEAR(g_capturedPushConstants.mvp[1], 0.0f, 1e-6f);
    CHECK_NEAR(g_capturedPushConstants.materialColor[0], 1.0f, 1e-6f);

    // The default stage configuration has to reach the shader: stage 0 modulates the texture by
    // the diffuse colour, stages 1..3 are disabled, and no stage projects.
    uint32_t stage0 = 0;
    memcpy(&stage0, &g_capturedPushConstants.stageConfig[0], sizeof(stage0));
    CHECK_EQ(stage0 & 0xFFu, (uint32_t)D3DTOP_MODULATE);
    CHECK_EQ((stage0 >> 8) & 0xFFu, (uint32_t)D3DTA_TEXTURE);
    CHECK_EQ((stage0 >> 16) & 0xFFu, (uint32_t)D3DTA_CURRENT);
    CHECK_EQ((stage0 >> 24) & 0x3u, (uint32_t)PLATFORM_UV_SOURCE_UV0);
    for (int stage = 1; stage < 4; ++stage) {
        uint32_t packed = 0;
        memcpy(&packed, &g_capturedPushConstants.stageConfig[stage], sizeof(packed));
        CHECK_EQ(packed & 0xFFu, (uint32_t)D3DTOP_DISABLE);
    }
    uint32_t alpha = 0;
    memcpy(&alpha, &g_capturedPushConstants.alphaConfig, sizeof(alpha));
    CHECK_EQ(alpha & 1u, 0u);   // alpha test off by default
}

TEST(draw_canonical_selects_multiply_for_the_shroud_blend)
{
    IDirect3DDevice9 device;
    device.SetFVF(FVF_XYZNDUV2);
    device.SetRenderState(D3DRS_LIGHTING, FALSE);
    D3DMATRIX identity = { { { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 } } };
    device.SetTransform(D3DTS_WORLD, &identity);
    device.SetTransform(D3DTS_VIEW, &identity);
    device.SetTransform(D3DTS_PROJECTION, &identity);
    D3DMATERIAL9 material = { {1,1,1,1}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, 0 };
    device.SetMaterial(&material);

    unsigned char triangle[3 * 44] = {};
    device.SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);

    // The shroud pass multiplies the terrain by the shroud texel: dest = src * dest, spelled
    // SRCBLEND=ZERO, DESTBLEND=SRCCOLOR. Selecting alpha blending here replaces the ground with
    // the shroud colour instead, which is what turned every map flat grey.
    device.SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
    device.SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
    g_capturedBlendMode = -1;
    device.DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, triangle, 44);
    CHECK_EQ(g_capturedBlendMode, 2);

    // The water mirror spells the same multiply the other way round.
    device.SetRenderState(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
    device.SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
    g_capturedBlendMode = -1;
    device.DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, triangle, 44);
    CHECK_EQ(g_capturedBlendMode, 2);

    // Plain alpha blending must stay alpha blending.
    device.SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device.SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    g_capturedBlendMode = -1;
    device.DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, triangle, 44);
    CHECK_EQ(g_capturedBlendMode, 1);
}

TEST(draw_canonical_lights_a_zero_diffuse_vertex_from_material_and_directional_light)
{
    IDirect3DDevice9 device;
    device.SetFVF(FVF_XYZNDUV2);

    D3DMATRIX identity = { { { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 } } };
    device.SetTransform(D3DTS_WORLD, &identity);
    device.SetTransform(D3DTS_VIEW, &identity);
    device.SetTransform(D3DTS_PROJECTION, &identity);

    D3DMATERIAL9 material = { {1,1,1,1}, {0,0,0,1}, {0,0,0,0}, {0,0,0,0}, 1.0f };
    device.SetMaterial(&material);
    device.SetRenderState(D3DRS_LIGHTING, TRUE);
    device.SetRenderState(D3DRS_COLORVERTEX, TRUE);
    device.SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL);
    device.SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
    device.SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
    device.SetRenderState(D3DRS_AMBIENT, 0);

    D3DLIGHT9 light;
    memset(&light, 0, sizeof(light));
    light.Type = D3DLIGHT_DIRECTIONAL;
    light.Direction.z = -1.0f;
    light.Diffuse.r = 0.5f;
    light.Diffuse.g = 0.25f;
    light.Diffuse.b = 0.125f;
    light.Diffuse.a = 1.0f;
    device.SetLight(0, &light);
    device.LightEnable(0, TRUE);

    unsigned char triangle[3 * 44] = {};
    for (int v = 0; v < 3; ++v) {
        unsigned char *p = triangle + v * 44;
        const float position[3] = { (float)v, 0.0f, 0.5f };
        const float normal[3] = { 0.0f, 0.0f, 1.0f };
        memcpy(p, position, sizeof(position));
        memcpy(p + 12, normal, sizeof(normal));
        p[27] = 0; // Zero vertex diffuse; D3DMCS_MATERIAL supplies the lit diffuse instead.
    }

    g_capturedVertices.clear();
    device.DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, triangle, 44);

    CHECK_EQ(g_capturedVertices.size(), (size_t)3);
    CHECK_NEAR(g_capturedVertices[0].color[0], 0.5f, 1e-4f);
    CHECK_NEAR(g_capturedVertices[0].color[1], 0.25f, 1e-4f);
    CHECK_NEAR(g_capturedVertices[0].color[2], 0.125f, 1e-4f);
    CHECK_NEAR(g_capturedVertices[0].color[3], 1.0f, 1e-6f);
    device.SetRenderState(D3DRS_LIGHTING, FALSE);
    g_capturedVertices.clear();
    device.DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, triangle, 44);
    CHECK_EQ(g_capturedVertices.size(), (size_t)3);
    CHECK_NEAR(g_capturedVertices[0].color[0], 0.0f, 1e-6f);
    CHECK_NEAR(g_capturedVertices[0].color[1], 0.0f, 1e-6f);
    CHECK_NEAR(g_capturedVertices[0].color[2], 0.0f, 1e-6f);
}

TEST(draw_canonical_projects_a_camera_space_stage_through_its_texture_matrix)
{
    IDirect3DDevice9 device;
    device.SetFVF(FVF_XYZNDUV2);

    // View and world are the identity, so camera space is the vertex position. The texture matrix
    // scales and offsets it, exactly as the shroud and cloud passes set one up.
    D3DMATRIX identity = { { { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 } } };
    device.SetTransform(D3DTS_WORLD, &identity);
    device.SetTransform(D3DTS_VIEW, &identity);
    device.SetTransform(D3DTS_PROJECTION, &identity);

    D3DMATRIX textureMatrix = { { { 0.5f,0,0,0, 0,0.25f,0,0, 0,0,1,0, 0.125f,0.5f,0,1 } } };
    device.SetTransform(D3DTS_TEXTURE0, &textureMatrix);

    device.SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
    device.SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

    unsigned char triangle[3 * 44] = {};
    for (int v = 0; v < 3; ++v) {
        unsigned char *p = triangle + v * 44;
        float position[3] = { (float)v * 4.0f, 2.0f, 0.0f };
        memcpy(p, position, sizeof(position));
    }

    g_capturedVertices.clear();
    device.DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, triangle, 44);

    CHECK_EQ(g_capturedVertices.size(), (size_t)3);
    // x = 0, 4, 8 through scale 0.5 and offset 0.125: 0.125, 2.125, 4.125.
    CHECK_NEAR(g_capturedVertices[0].projected0[0], 0.125f, 1e-5f);
    CHECK_NEAR(g_capturedVertices[1].projected0[0], 2.125f, 1e-5f);
    CHECK_NEAR(g_capturedVertices[2].projected0[0], 4.125f, 1e-5f);
    // y = 2 through scale 0.25 and offset 0.5.
    CHECK_NEAR(g_capturedVertices[0].projected0[1], 1.0f, 1e-5f);

    uint32_t stage0 = 0;
    memcpy(&stage0, &g_capturedPushConstants.stageConfig[0], sizeof(stage0));
    CHECK_EQ((stage0 >> 24) & 0x3u, (uint32_t)PLATFORM_UV_SOURCE_PROJECTED0);

    // Leave the stage state as the next test expects to find it.
    device.SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    device.SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
}

TEST(draw_canonical_refuses_a_stride_the_fvf_does_not_describe)
{
    IDirect3DDevice9 device;
    device.SetFVF(FVF_XYZNDUV2);
    unsigned char vertex[44] = {};
    g_capturedVertices.clear();
    device.DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, vertex, 36);
    CHECK(g_capturedVertices.empty());
}
