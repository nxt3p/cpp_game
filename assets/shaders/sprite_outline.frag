#version 330 core

in vec2 v2f_TexCoord;

uniform sampler2D u_Texture;
uniform vec4 u_OutlineColor;
uniform vec2 u_TexelStep;

out vec4 FragColor;

void main() {
    float centerAlpha = texture(u_Texture, v2f_TexCoord).a;

    float neighborAlpha = 0.0;
    neighborAlpha = max(neighborAlpha, texture(u_Texture, v2f_TexCoord + vec2(u_TexelStep.x, 0.0)).a);
    neighborAlpha = max(neighborAlpha, texture(u_Texture, v2f_TexCoord - vec2(u_TexelStep.x, 0.0)).a);
    neighborAlpha = max(neighborAlpha, texture(u_Texture, v2f_TexCoord + vec2(0.0, u_TexelStep.y)).a);
    neighborAlpha = max(neighborAlpha, texture(u_Texture, v2f_TexCoord - vec2(0.0, u_TexelStep.y)).a);
    neighborAlpha = max(neighborAlpha, texture(u_Texture, v2f_TexCoord + u_TexelStep).a);
    neighborAlpha = max(neighborAlpha, texture(u_Texture, v2f_TexCoord - u_TexelStep).a);
    neighborAlpha = max(neighborAlpha, texture(u_Texture, v2f_TexCoord + vec2(u_TexelStep.x, -u_TexelStep.y)).a);
    neighborAlpha = max(neighborAlpha, texture(u_Texture, v2f_TexCoord + vec2(-u_TexelStep.x, u_TexelStep.y)).a);

    float edgeMask = step(0.22, neighborAlpha) * (1.0 - step(0.42, centerAlpha));
    if (edgeMask < 0.5) {
        discard;
    }

    FragColor = u_OutlineColor;
}
