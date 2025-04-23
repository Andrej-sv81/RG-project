#version 330 core

in vec2 chTex;

out vec4 outCol;

uniform sampler2D bottomTex;
uniform sampler2D topTex;

void main()
{
    vec4 texBot = texture(bottomTex, chTex);    
    vec4 texTop = texture(topTex, chTex);   

    if (texTop.a > 0.1) {
        outCol = texTop;
    } else if (texBot.a > 0.1) {
        outCol = texBot;
    } else {
        discard;
    }
}