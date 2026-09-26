#version 330 core

in vec2 v2f_TexCoord;
in vec4 v2f_Color;

uniform sampler2D u_Texture;
uniform int u_UseTexture;

out vec4 FragColor;

void main() {
    if (u_UseTexture != 0) {
        vec4 sampled = texture(u_Texture, v2f_TexCoord) * v2f_Color;
        if (sampled.a < 0.01) {
            discard;
        }
        FragColor = sampled;
    } else {
        FragColor = v2f_Color;
    }
}
