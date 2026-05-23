#version 330 core

in vec2 v2f_TexCoord;

uniform vec4 u_Color;
uniform sampler2D u_Texture;
uniform int u_UseTexture;

out vec4 FragColor;

void main() {
    if (u_UseTexture != 0) {
        vec4 sampled = texture(u_Texture, v2f_TexCoord);
        FragColor = sampled * u_Color;
        if (FragColor.a < 0.01) {
            discard;
        }
    } else {
        FragColor = u_Color;
    }
}
