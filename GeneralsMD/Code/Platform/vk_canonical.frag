#version 450
// The fixed-function texture stages, evaluated in the order Direct3D 9 evaluated them, plus the
// alpha test it applied after them. This is the same arithmetic the fork's HLSL generator writes
// for the Direct3D 11 path (ffshader.cpp): arguments are selected, alpha-replicated and then
// complemented, "current" starts at the diffuse colour, and each stage's result becomes the next
// stage's "current".
//
// Stages past the fourth are not evaluated. -ffprobe counted the game's combiners across four maps
// at 28 programs, never more than two stages, and four only for water, so four is the whole
// vocabulary in practice.

layout(set = 0, binding = 0) uniform sampler2D stageTexture0;
layout(set = 1, binding = 0) uniform sampler2D stageTexture1;
layout(set = 2, binding = 0) uniform sampler2D stageTexture2;
layout(set = 3, binding = 0) uniform sampler2D stageTexture3;

layout(location = 0) in vec2 vTexCoord0;
layout(location = 1) in vec2 vTexCoord1;
layout(location = 2) in vec2 vTexCoord2;
layout(location = 3) in vec2 vTexCoord3;
layout(location = 4) in vec4 vColor;

layout(location = 0) out vec4 fragmentColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 materialColor;
    vec4 stageConfig;
    vec4 stageAlphaConfig;
    float alphaReference;
    float alphaConfig;
    float textureFactor;
} pc;

vec4 stage_argument(uint selector, vec4 texel, vec4 current, vec4 factor)
{
    uint base = selector & 0xFu;
    vec4 value;
    if (base == 2u) value = texel;              // D3DTA_TEXTURE
    else if (base == 3u) value = factor;        // D3DTA_TFACTOR
    else if (base == 4u) value = current;       // D3DTA_SPECULAR: the canonical vertex carries none
    else value = current;                       // D3DTA_DIFFUSE / D3DTA_CURRENT
    // Replicate first, complement second: the order the reference rasteriser applies them.
    if ((selector & 0x20u) != 0u) value = value.aaaa;
    if ((selector & 0x10u) != 0u) value = 1.0 - value;
    return value;
}

vec4 stage_operation(uint operation, uint arg0, uint arg1, uint arg2,
                     vec4 texel, vec4 current, vec4 factor)
{
    vec4 a0 = stage_argument(arg0, texel, current, factor);
    vec4 a1 = stage_argument(arg1, texel, current, factor);
    vec4 a2 = stage_argument(arg2, texel, current, factor);
    switch (operation) {
    case 2u: return a1;                                   // D3DTOP_SELECTARG1
    case 3u: return a2;                                   // D3DTOP_SELECTARG2
    case 4u: return a1 * a2;                              // D3DTOP_MODULATE
    case 5u: return a1 * a2 * 2.0;                        // D3DTOP_MODULATE2X
    case 6u: return a1 * a2 * 4.0;                        // D3DTOP_MODULATE4X
    case 7u: return a1 + a2;                              // D3DTOP_ADD
    case 8u: return a1 + a2 - 0.5;                        // D3DTOP_ADDSIGNED
    case 9u: return (a1 + a2 - 0.5) * 2.0;                // D3DTOP_ADDSIGNED2X
    case 10u: return a1 - a2;                             // D3DTOP_SUBTRACT
    case 11u: return a1 + a2 - a1 * a2;                   // D3DTOP_ADDSMOOTH
    case 12u: return mix(a2, a1, current.a);              // D3DTOP_BLENDDIFFUSEALPHA
    case 13u: return mix(a2, a1, texel.a);                // D3DTOP_BLENDTEXTUREALPHA
    case 14u: return mix(a2, a1, factor.a);               // D3DTOP_BLENDFACTORALPHA
    case 15u: return a1 + a2 * (1.0 - texel.a);           // D3DTOP_BLENDTEXTUREALPHAPM
    case 16u: return mix(a2, a1, current.a);              // D3DTOP_BLENDCURRENTALPHA
    case 18u: return a1 + a1.a * a2;                      // D3DTOP_MODULATEALPHA_ADDCOLOR
    case 19u: return a1 * a2 + a1.a;                      // D3DTOP_MODULATECOLOR_ADDALPHA
    case 20u: return (1.0 - a1.a) * a2 + a1;              // D3DTOP_MODULATEINVALPHA_ADDCOLOR
    case 21u: return (1.0 - a1) * a2 + a1.a;              // D3DTOP_MODULATEINVCOLOR_ADDALPHA
    case 24u:                                             // D3DTOP_DOTPRODUCT3
        return clamp(dot(a1.rgb * 2.0 - 1.0, a2.rgb * 2.0 - 1.0), 0.0, 1.0).xxxx;
    case 25u: return a0 + a1 * a2;                        // D3DTOP_MULTIPLYADD
    case 26u: return mix(a2, a1, a0);                     // D3DTOP_LERP
    default: return current;
    }
}

// One stage. Disabled stages (D3DTOP_DISABLE) pass the running value through, which is what the
// device did when the stage's operation was disabled.
vec4 run_stage(vec4 current, vec4 texel, vec4 factor, uint config, uint alphaConfig)
{
    uint colourOperation = config & 0xFFu;
    if (colourOperation > 1u) {
        // The colour operation writes RGB only. Direct3D 9 leaves the alpha channel to ALPHAOP,
        // and when that is disabled the alpha is the diffuse alpha, untouched. Multiplying alpha
        // here made every texture whose texels carry alpha 0 (the terrain atlas has one alpha bit
        // and the ship deck details have none) turn the whole surface transparent, so the ground
        // and the deck showed the background through them.
        vec4 colour = stage_operation(colourOperation,
                                      (alphaConfig >> 24) & 0xFFu,
                                      (config >> 8) & 0xFFu,
                                      (config >> 16) & 0xFFu,
                                      texel, current, factor);
        current.rgb = colour.rgb;
    }
    uint alphaOperation = alphaConfig & 0xFFu;
    if (alphaOperation > 1u) {
        vec4 alpha = stage_operation(alphaOperation, 0u,
                                     (alphaConfig >> 8) & 0xFFu,
                                     (alphaConfig >> 16) & 0xFFu,
                                     texel, current, factor);
        current.a = alpha.a;
    }
    return current;
}

bool alpha_test_passes(float alpha, uint function, float reference)
{
    if (function == 1u) return false;                       // D3DCMP_NEVER
    if (function == 2u) return alpha < reference;           // D3DCMP_LESS
    if (function == 3u) return alpha == reference;          // D3DCMP_EQUAL
    if (function == 5u) return alpha > reference;           // D3DCMP_GREATER
    if (function == 6u) return alpha != reference;          // D3DCMP_NOTEQUAL
    if (function == 7u) return alpha >= reference;          // D3DCMP_GREATEREQUAL
    if (function == 8u) return true;                        // D3DCMP_ALWAYS
    return alpha >= reference;                              // D3DCMP_LESSEQUAL, the default
}

void main()
{
    uint packedFactor = floatBitsToUint(pc.textureFactor);
    vec4 factor = vec4(float((packedFactor >> 16) & 0xFFu), float((packedFactor >> 8) & 0xFFu),
                       float(packedFactor & 0xFFu), float((packedFactor >> 24) & 0xFFu)) / 255.0;

    // Sample a stage only when that stage is enabled. The stage configuration is a push constant,
    // so the branch is uniform across the draw and costs nothing, while an unconditional sample
    // per stage makes every pixel fetch four textures - the disabled stages were the most
    // expensive thing in the frame.
    vec4 current = vColor * pc.materialColor;
    if ((floatBitsToUint(pc.stageConfig.x) & 0xFFu) > 1u)
        current = run_stage(current, texture(stageTexture0, vTexCoord0), factor,
                            floatBitsToUint(pc.stageConfig.x), floatBitsToUint(pc.stageAlphaConfig.x));
    if ((floatBitsToUint(pc.stageConfig.y) & 0xFFu) > 1u)
        current = run_stage(current, texture(stageTexture1, vTexCoord1), factor,
                            floatBitsToUint(pc.stageConfig.y), floatBitsToUint(pc.stageAlphaConfig.y));
    if ((floatBitsToUint(pc.stageConfig.z) & 0xFFu) > 1u)
        current = run_stage(current, texture(stageTexture2, vTexCoord2), factor,
                            floatBitsToUint(pc.stageConfig.z), floatBitsToUint(pc.stageAlphaConfig.z));
    if ((floatBitsToUint(pc.stageConfig.w) & 0xFFu) > 1u)
        current = run_stage(current, texture(stageTexture3, vTexCoord3), factor,
                            floatBitsToUint(pc.stageConfig.w), floatBitsToUint(pc.stageAlphaConfig.w));

    uint alphaConfig = floatBitsToUint(pc.alphaConfig);
    if ((alphaConfig & 1u) != 0u) {
        if (!alpha_test_passes(current.a, (alphaConfig >> 8) & 0xFFu, pc.alphaReference)) {
            discard;
        }
    }

    fragmentColor = current;
}
