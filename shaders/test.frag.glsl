#version 450

uniform sampler2D texture; 
in vec2 tcoord;

out vec4 outColor;

void main()
{
    outColor = texture2D(texture, tcoord);
}