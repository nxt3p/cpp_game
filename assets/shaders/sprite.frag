#version 330 core

in vec3 v2f_WorldPos;
in vec2 v2f_TexCoord;

uniform sampler2D u_Texture;
uniform vec3 u_PlayerPos;
uniform float u_LightRadius;
uniform float u_AmbientDark;
uniform float u_AmbientBright;
uniform vec4 u_Tint;

out vec4 FragColor;

void main() {
    vec4 sampled = texture(u_Texture, v2f_TexCoord);
    if (sampled.a < 0.05) {
        discard;
    }

    float dist = length(v2f_WorldPos.xz - u_PlayerPos.xz);
    float torch = 1.0 - smoothstep(u_LightRadius * 0.82, u_LightRadius, dist);
    float ambientLevel = mix(u_AmbientDark, u_AmbientBright, torch);

    vec3 lit = sampled.rgb * ambientLevel * u_Tint.rgb;
    FragColor = vec4(lit, sampled.a * u_Tint.a);
}
