#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragTexColor;

layout(binding = 0) uniform UniformBufferObject {
    vec4 color;
} ubo;

layout(location = 0) in vec2 inDst;
layout(location = 1) in vec2 inSrc;

void main() {
    gl_Position = vec4(inDst.x, inDst.y, 0.0, 1.0);
    fragTexCoord = inSrc;
    fragTexColor = ubo.color;
}
