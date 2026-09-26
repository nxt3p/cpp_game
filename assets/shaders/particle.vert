#version 330 core

layout(location = 0) in vec3 a_Pos;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in vec4 a_Color;

uniform mat4 u_View;
uniform mat4 u_Projection;

out vec2 v2f_TexCoord;
out vec4 v2f_Color;

void main() {
    v2f_TexCoord = a_TexCoord;
    v2f_Color = a_Color;
    gl_Position = u_Projection * u_View * vec4(a_Pos, 1.0);
}
