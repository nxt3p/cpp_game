#version 330 core

in vec2 v2f_TexCoord;
in vec4 v2f_Color;

out vec4 FragColor;

void main() {
    vec2 centered = v2f_TexCoord * 2.0 - 1.0;
    float radial = dot(centered, centered);
    if (radial > 1.0) {
        discard;
    }
    // Soft disc with a hot core: glow falls off quadratically toward the rim.
    float falloff = 1.0 - radial;
    float alpha = falloff * falloff;
    float core = smoothstep(0.55, 0.0, radial) * 0.6;
    vec3 color = v2f_Color.rgb + vec3(core);
    FragColor = vec4(color, v2f_Color.a * alpha);
}
