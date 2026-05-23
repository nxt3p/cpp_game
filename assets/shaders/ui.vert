#version 330 core

layout(location = 0) in vec2 a_Pos;
layout(location = 1) in vec2 a_TexCoord;

uniform mat4 u_Projection;

out vec2 v2f_TexCoord;

void main() {
    v2f_TexCoord = a_TexCoord;
    gl_Position = u_Projection * vec4(a_Pos, 0.0, 1.0);
}
