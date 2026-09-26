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

// Same grade constants as default.frag so props and terrain share one palette.
const vec3 kShadowTint = vec3(0.55, 0.62, 0.95);
const vec3 kTorchTint = vec3(1.10, 0.92, 0.72);
const float kDesaturate = 0.18;

void main() {
    vec4 sampled = texture(u_Texture, v2f_TexCoord);
    if (sampled.a < 0.05) {
        discard;
    }

    float dist = length(v2f_WorldPos.xz - u_PlayerPos.xz);
    float torch = 1.0 - smoothstep(u_LightRadius * 0.82, u_LightRadius, dist);
    float ambientLevel = mix(u_AmbientDark, u_AmbientBright, torch);

    vec3 lit = sampled.rgb * ambientLevel * u_Tint.rgb;
    vec3 tint = mix(kShadowTint, kTorchTint, torch);
    lit *= tint;
    float luma = dot(lit, vec3(0.299, 0.587, 0.114));
    lit = mix(lit, vec3(luma), kDesaturate);

    float fog = smoothstep(u_LightRadius * 2.2, u_LightRadius * 5.5, dist);
    lit = mix(lit, vec3(0.015, 0.018, 0.03), fog * 0.75);

    FragColor = vec4(lit, sampled.a * u_Tint.a);
}
