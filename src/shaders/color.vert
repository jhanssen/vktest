#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec3 fragColor;

layout(binding = 0) uniform UniformBufferObject {
    vec4 color;
    vec4 geometry;
} ubo;

vec2 positions[4] = vec2[](
    vec2(-1.0, +1.0),
    vec2(+1.0, +1.0),
    vec2(-1.0, -1.0),
    vec2(+1.0, -1.0)
);

void main() {
    vec2 position = positions[gl_VertexIndex];
    int x = position.x == -1.0 ? 0 : 2;
    int y = position.y == +1.0 ? 1 : 3;
    gl_Position = vec4(ubo.geometry[x], ubo.geometry[y], 0.0, 1.0);
    fragColor = ubo.color.rgb;
}
