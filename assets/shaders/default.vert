#version 330 core

layout(location = 0) in vec3 a_Pos;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoords;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

out vec3 v2f_WorldPos;
out vec3 v2f_Normal;
out vec2 v2f_TexCoords;

void main() {
    vec4 worldPos = u_Model * vec4(a_Pos, 1.0);
    v2f_WorldPos = worldPos.xyz;
    v2f_Normal = mat3(transpose(inverse(u_Model))) * a_Normal;
    v2f_TexCoords = a_TexCoords;

    gl_Position = u_Projection * u_View * worldPos;
}
