#version 330 core

out vec4 FragColor;

uniform vec3 overlay; // RGB color input

void main() {
    FragColor = vec4(overlay, 0.3); // full alpha
}
