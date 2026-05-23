#version 330 core

layout(location = 0) in vec3 a_Pos;
layout(location = 1) in vec4 a_Color;

uniform mat4 u_Projection;

out vec4 v2f_Color;

void main() {
    gl_Position = u_Projection * vec4(a_Pos, 1.0);
    v2f_Color = a_Color;
}
