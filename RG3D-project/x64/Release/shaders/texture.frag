#version 330 core

in vec2 chTex;

out vec4 outCol;

uniform sampler2D sampTex;

void main()
{
	outCol = texture(sampTex, chTex);
}