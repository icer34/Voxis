#version 460
#extension GL_EXT_nonuniform_qualifier : require


layout(set = 1, binding = 0) uniform sampler2D bindlessTextures[];

layout(push_constant) uniform constants
{
	mat4 model;
	uint blockAtlasIdx;
} pc;

layout(location = 0) flat in uint inTileIdx;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec4 fragColor;

// TEXTURE_SIZE / PADDING / CELL_STRIDE are injected as macros by Renderer::createShaderModule,
// single source of truth is BlockAtlas - don't hardcode them here
const uint ATLAS_TILES_PER_ROW = 2u; // ceil(sqrt(4)) for the 4 textures currently in assets/textures/block
const float ATLAS_TILE_UV_SIZE = 1.0 / float(ATLAS_TILES_PER_ROW);
const float PADDING_FRAC = float(PADDING) / float(CELL_STRIDE);
const float CONTENT_FRAC = float(TEXTURE_SIZE) / float(CELL_STRIDE);

vec2 atlasUV(uint tileIndex, vec2 localUV)
{
    vec2 wrapped = fract(localUV);
    uint col = tileIndex % ATLAS_TILES_PER_ROW;
    uint row = tileIndex / ATLAS_TILES_PER_ROW;

    vec2 insetUV = PADDING_FRAC + wrapped * CONTENT_FRAC;
    return (vec2(col, row) + insetUV) * ATLAS_TILE_UV_SIZE;
}

void main()
{
    vec2 ddxUV = dFdx(inUV) * CONTENT_FRAC;
    vec2 ddyUV = dFdy(inUV) * CONTENT_FRAC;

    vec2 uv = atlasUV(inTileIdx, inUV);
    fragColor = textureGrad(bindlessTextures[nonuniformEXT(pc.blockAtlasIdx)], uv, ddxUV, ddyUV);
}
