#version 450
// The one vertex program the Vulkan path runs. Every D3D9 flexible vertex format is decoded into
// the canonical 60-byte vertex before the draw, so this never changes per material.
//
// The four texture coordinate outputs are what the fixed-function texture stages would have
// generated: two pass-through sets from the FVF, and two the host computed from camera-space
// position through a stage's texture matrix (D3DTSS_TCI_CAMERASPACEPOSITION). The host picks which
// one each stage samples with the packed uvSource field.

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aUv0;
layout(location = 2) in vec4 aColor;
layout(location = 3) in vec2 aUv1;
layout(location = 4) in vec2 aProjected0;
layout(location = 5) in vec2 aProjected1;

layout(location = 0) out vec2 vTexCoord0;
layout(location = 1) out vec2 vTexCoord1;
layout(location = 2) out vec2 vTexCoord2;
layout(location = 3) out vec2 vTexCoord3;
layout(location = 4) out vec4 vColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 materialColor;
    // stage i: colourOp | colourArg1 << 8 | colourArg2 << 16 | uvSource << 24
    vec4 stageConfig;
    // stage i: alphaOp | alphaArg1 << 8 | alphaArg2 << 16 | colourArg0 << 24
    vec4 stageAlphaConfig;
    float alphaReference;   // D3DRS_ALPHAREF / 255
    float alphaConfig;      // bit 0 enable, bits 8..15 the D3DCMP_* function
    float textureFactor;    // D3DRS_TEXTUREFACTOR, packed D3DCOLOR
} pc;

vec2 pick_coordinate(uint source, vec2 uv0, vec2 uv1, vec2 projected0, vec2 projected1)
{
    if (source == 1u) return uv1;
    if (source == 2u) return projected0;
    if (source == 3u) return projected1;
    return uv0;
}

void main()
{
    gl_Position = pc.mvp * vec4(aPosition, 1.0);
    vColor = aColor;
    vTexCoord0 = pick_coordinate((floatBitsToUint(pc.stageConfig.x) >> 24) & 0x3u,
                                 aUv0, aUv1, aProjected0, aProjected1);
    vTexCoord1 = pick_coordinate((floatBitsToUint(pc.stageConfig.y) >> 24) & 0x3u,
                                 aUv0, aUv1, aProjected0, aProjected1);
    vTexCoord2 = pick_coordinate((floatBitsToUint(pc.stageConfig.z) >> 24) & 0x3u,
                                 aUv0, aUv1, aProjected0, aProjected1);
    vTexCoord3 = pick_coordinate((floatBitsToUint(pc.stageConfig.w) >> 24) & 0x3u,
                                 aUv0, aUv1, aProjected0, aProjected1);
}
