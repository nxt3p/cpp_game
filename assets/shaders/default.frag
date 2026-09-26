#version 330 core

in vec3 v2f_WorldPos;
in vec3 v2f_Normal;
in vec2 v2f_TexCoords;

uniform vec3 u_PlayerPos;
uniform float u_LightRadius;
uniform float u_AmbientDark;
uniform float u_AmbientBright;
uniform vec3 u_LightColor;
uniform vec3 u_ObjectColor;

out vec4 FragColor;

// Dark-fantasy grade: cold blue-violet shadows, warm amber torch, gentle desaturation.
const vec3 kShadowTint = vec3(0.55, 0.62, 0.95);
const vec3 kTorchTint = vec3(1.10, 0.92, 0.72);
const float kDesaturate = 0.18;

void main() {
    vec3 normal = normalize(v2f_Normal);
    if (length(normal) < 0.0001) {
        normal = vec3(0.0, 1.0, 0.0);
    }

    float dist = length(v2f_WorldPos.xz - u_PlayerPos.xz);
    float torch = 1.0 - smoothstep(u_LightRadius * 0.82, u_LightRadius, dist);
    float ambientLevel = mix(u_AmbientDark, u_AmbientBright, torch);
    vec3 ambient = vec3(ambientLevel) * u_ObjectColor;

    vec3 lightCenter = vec3(u_PlayerPos.x, u_PlayerPos.y + 2.5, u_PlayerPos.z);
    vec3 lightDir = normalize(lightCenter - v2f_WorldPos);
    float diffuseStrength = max(dot(normal, lightDir), 0.0) * torch;
    vec3 diffuse = diffuseStrength * u_LightColor * u_ObjectColor;

    vec3 result = ambient + diffuse;

    // Palette grade: tint by how deep in shadow we are, then desaturate slightly.
    vec3 tint = mix(kShadowTint, kTorchTint, torch);
    result *= tint;
    float luma = dot(result, vec3(0.299, 0.587, 0.114));
    result = mix(result, vec3(luma), kDesaturate);

    // Far fog into the void so the world edge fades instead of clipping.
    float fog = smoothstep(u_LightRadius * 2.2, u_LightRadius * 5.5, dist);
    result = mix(result, vec3(0.015, 0.018, 0.03), fog * 0.75);

    // v2f_TexCoords reserved for future texture sampling
    float texMix = clamp(v2f_TexCoords.x * 0.0 + v2f_TexCoords.y * 0.0, 0.0, 1.0);
    result = mix(result, result, texMix);

    FragColor = vec4(result, 1.0);
}
