#version 460

layout (set=0, binding=0) uniform CameraUBO
{
    mat4 view;
    mat4 proj;
} camera;

layout (push_constant) uniform constants
{
    mat4 model;
} pc;

layout(location = 0) in uint inPackedData1;
layout(location = 1) in uint inPackedData2;

layout(location = 0) flat out uint outTileIdx;
layout(location = 1) out vec2 outUV;

const vec3 NORMALS[6] = vec3[](
    vec3(0, 0, -1), // North
    vec3(0, 0, 1),  // South
    vec3(1, 0, 0),  // East
    vec3(-1, 0, 0), // West
    vec3(0, 1, 0),  // Up
    vec3(0, -1, 0) // Down
);

void main() {

    uint baseX = inPackedData1 & 0xFu;
    uint baseY = (inPackedData1 >> 4) & 0xFu;
    uint baseZ = (inPackedData1 >> 8) & 0xFu;
    uint direction = (inPackedData1 >> 12) & 0x7u;
    uint cornerID = (inPackedData1 >> 15) & 0x3u;
    uint tileIndex = (inPackedData1 >> 17) & 0x7FFFu;

    uint width = ((inPackedData2 & 0xFu)) + 1u;
    uint height = ((inPackedData2 >> 4) & 0xFu) + 1u;

    vec2 corner2D = vec2(
        (cornerID == 1u || cornerID == 2u) ? float(width)  : 0.0,
        (cornerID == 2u || cornerID == 3u) ? float(height) : 0.0
    );

    vec3 offset;
    if (direction == 0u)      offset = vec3(corner2D.y, corner2D.x, 0.0); // North (-Z)
    else if (direction == 1u) offset = vec3(corner2D.x, corner2D.y, 1.0); // South (+Z) 
    else if (direction == 2u) offset = vec3(1.0, corner2D.x, corner2D.y); // East  (+X) 
    else if (direction == 3u) offset = vec3(0.0, corner2D.y, corner2D.x); // West  (-X)
    else if (direction == 4u) offset = vec3(corner2D.y, 1.0, corner2D.x); // Up    (+Y)
    else                       offset = vec3(corner2D.x, 0.0, corner2D.y); // Down  (-Y) 


    vec3 localPos = vec3(float(baseX), float(baseY), float(baseZ)) + offset;


    gl_Position = camera.proj * camera.view * pc.model * vec4(localPos, 1.0);
    outUV = corner2D;
    outTileIdx = tileIndex;
}