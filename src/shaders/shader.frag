#version 460
#extension GL_EXT_nonuniform_qualifier : require


layout(set = 1, binding = 0) uniform sampler2D bindlessTextures[];
layout(set = 2, binding = 0) uniform usampler3D chunkDataTextures[];

layout(push_constant) uniform constants
{
	mat4 model;
	uint blockAtlasIdx;
	uint chunkDataTextureIdx;
	uint atlasTilesPerRow;
} pc;

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inLocalPos;
layout(location = 2) in float inAO;

layout(location = 0) out vec4 fragColor;

// TEXTURE_SIZE / PADDING / CELL_STRIDE are injected as macros by Renderer::createShaderModule,
// single source of truth is BlockAtlas - don't hardcode them here. ATLAS_TILES_PER_ROW can't join
// them (BlockAtlas only discovers it at runtime, after shaders are already compiled), so it comes
// through the push constant instead - see pc.atlasTilesPerRow
const float PADDING_FRAC = float(PADDING) / float(CELL_STRIDE);
const float CONTENT_FRAC = float(TEXTURE_SIZE) / float(CELL_STRIDE);

vec2 atlasUV(uint tileIndex, vec2 localUV, uint tilesPerRow)
{
    vec2 wrapped = fract(localUV);
    uint col = tileIndex % tilesPerRow;
    uint row = tileIndex / tilesPerRow;

    vec2 insetUV = PADDING_FRAC + wrapped * CONTENT_FRAC;
    return (vec2(col, row) + insetUV) / float(tilesPerRow);
}

void main()
{
    // must match atlasUV()'s scaling exactly (CONTENT_FRAC then / tilesPerRow) - these derivatives
    // are in the same [0,1] atlas UV space as the coordinate passed to textureGrad below, otherwise
    // the GPU thinks the texture changes tilesPerRow times faster across the screen than it really
    // does and picks a much blurrier mip than needed
    float atlasScale = CONTENT_FRAC / float(pc.atlasTilesPerRow);
    vec2 ddxUV = dFdx(inUV) * atlasScale;
    vec2 ddyUV = dFdy(inUV) * atlasScale;

    ivec3 voxelCoord = ivec3(floor(inLocalPos));
    uint tileIndex = texelFetch(chunkDataTextures[nonuniformEXT(pc.chunkDataTextureIdx)], voxelCoord, 0).r;

    vec2 uv = atlasUV(tileIndex, inUV, pc.atlasTilesPerRow);
    fragColor = textureGrad(bindlessTextures[nonuniformEXT(pc.blockAtlasIdx)], uv, ddxUV, ddyUV);

    fragColor.rgb *= mix(0.0, 1.0, inAO * inAO);
}
