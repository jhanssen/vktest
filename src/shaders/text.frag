#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragTexColor;

layout(location = 0) out vec4 outColor;

void main() {
    float dist = texture(texSampler, fragTexCoord).r;
    float a = smoothstep(0.507 - 0.07, 0.507 + 0.07, dist);
    a = pow(a, 1.0 / 1.8);
    outColor = fragTexColor * a;
}
