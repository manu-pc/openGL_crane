#version 330 core

in vec3 ourColor;

out vec4 FragColor;
uniform vec3 Color;
void main()
{
    FragColor = vec4(ourColor,1.0f);
} 