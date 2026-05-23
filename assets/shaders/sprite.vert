#version 330 core

layout(location = 0) in vec3 a_Pos;
layout(location = 1) in vec2 a_TexCoord;

uniform mat4 u_View;
uniform mat4 u_Projection;

out vec3 v2f_WorldPos;
out vec2 v2f_TexCoord;

void main() {
    v2f_WorldPos = a_Pos;
    v2f_TexCoord = a_TexCoord;
    gl_Position = u_Projection * u_View * vec4(a_Pos, 1.0);
}
