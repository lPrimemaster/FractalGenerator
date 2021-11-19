#version 450

layout(local_size_x = 1, local_size_y = 1) in;
layout(rgba32f, binding = 0) uniform image2D img_output;

layout(std140, binding = 1) buffer ScreenData
{
    uint width;
    uint height;
};

float true_evap  = 0.20;  // Default: 0.2
float true_speed = 1.50;  // Default: 0.05

float diff_speed = 0.1 * true_speed;
float evap_speed = 0.005 * true_evap;

void main()
{
    ivec2 coord = ivec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y);
    vec4 oc = imageLoad(img_output, coord);

    // 3x3 Blur Kernel
    vec4 sum = vec4(0.0);
    for(int oX = -1; oX <= 1; oX++)
    {
        for(int oY = -1; oY <= 1; oY++)
        {
            int sX = coord.x + oX;
            int sY = coord.y + oY;

            if(sX >= 0 && sX < width && sY >= 0 && sY < height)
            {
                sum += imageLoad(img_output, ivec2(sX, sY));
            }
        }
    }

    sum /= 9;

    vec4 diffVal = mix(oc, sum, diff_speed);
    vec4 diffEvapVal = max(vec4(0.0), diffVal - vec4(evap_speed));

    imageStore(img_output, coord, vec4(diffEvapVal.rgb, 1.0));
}
