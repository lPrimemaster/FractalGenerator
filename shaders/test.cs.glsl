#version 450

layout(local_size_x = 1, local_size_y = 1) in;
layout(rgba32f, binding = 0) uniform image2D img_output;

layout(std140, binding = 1) buffer ScreenData
{
    uint width;
    uint height;
};

layout(std140, binding = 2) buffer PoliData
{
    uint poli_size;
    double poli_data[];
};

uniform float px;
uniform float py;

uniform double pxd;
uniform double pyd;

uniform float zoom = 1;
uniform double zoomd = 1;
uniform uint iterations = 20;
uniform int d_prec = 0;
uniform uint set = 0;

uniform int cmode = 0;
uniform vec3 colorGrad;

vec3 HSVtoRGB(float H, float S, float V){
    float s = S/100;
    float v = V/100;
    float C = s*v;
    float X = C*(1-abs(mod(H/60.0, 2)-1));
    float m = v-C;
    float r,g,b;

    if(H < 5)
    {
        return vec3(0, 0, 0);
    }

    if(H >= 5 && H < 60){
        r = C,g = X,b = 0;
    }
    else if(H >= 60 && H < 120){
        r = X,g = C,b = 0;
    }
    else if(H >= 120 && H < 180){
        r = 0,g = C,b = X;
    }
    else if(H >= 180 && H < 240){
        r = 0,g = X,b = C;
    }
    else if(H >= 240 && H < 300){
        r = X,g = 0,b = C;
    }
    else{
        r = C,g = 0,b = X;
    }
    float R = (r+m);
    float G = (g+m);
    float B = (b+m);
    
    return vec3(R, G, B);
}

uint _mandelF(float x, float y, uint maxit) {
    float zr = 0;
    float zi = 0;
    float zrsqr = 0;
    float zisqr = 0;
    uint i = 0;

    for(i = 0; i < maxit; i++)
    {
        zi = zr * zi;
        zi += zi;
        zi += y;

        zr = zrsqr - zisqr + x;
        zrsqr = zr * zr;
        zisqr = zi * zi;

        if(zrsqr + zisqr > 4.0) break;
    }

    return i;
}

uint _mandelD(double x, double y, uint maxit) {
    double zr = 0;
    double zi = 0;
    double zrsqr = 0;
    double zisqr = 0;
    uint i = 0;

    for(i = 0; i < maxit; i++)
    {
        zi = zr * zi;
        zi += zi;
        zi += y;

        zr = zrsqr - zisqr + x;
        zrsqr = zr * zr;
        zisqr = zi * zi;

        if(zrsqr + zisqr > 4.0) break;
    }

    return i;
}

uint _shipF(float x, float y, uint maxit) {
    float zr = 0;
    float zi = 0;
    float zrsqr = 0;
    float zisqr = 0;
    uint i = 0;

    for(i = 0; i < maxit; i++)
    {

        zi = zr * zi;
        zi += zi;
        zi = abs(zi);
        zi += y;

        zr = zrsqr - zisqr + x;
        zrsqr = zr * zr;
        zisqr = zi * zi;

        if(zrsqr + zisqr > 4.0) break;
    }

    return i;
}

uint _shipD(double x, double y, uint maxit) {
    double zr = 0;
    double zi = 0;
    double zrsqr = 0;
    double zisqr = 0;
    uint i = 0;


    for(i = 0; i < maxit; i++)
    {
        zi = zr * zi;
        zi += zi;
        zi = abs(zi);
        zi += y;
        
        zr = zrsqr - zisqr + x;
        zrsqr = zr * zr;
        zisqr = zi * zi;

        if(zrsqr + zisqr > 4.0) break;
    }

    return i;
}

uint _mandel3F(float x, float y, uint maxit) {
    float zr = 0;
    float zi = 0;
    float zrsqr = 0;
    float zisqr = 0;
    float zrcub = 0;
    float zicub = 0;
    uint i = 0;

    for(i = 0; i < maxit; i++)
    {
        zi = 3 * zrsqr * zi - zicub + y;
        zr = zrcub - 3 * zr * zisqr + x;

        zrsqr = zr * zr;
        zisqr = zi * zi;
        zrcub = zrsqr * zr;
        zicub = zisqr * zi;

        if(zrsqr + zisqr > 4.0) break;
    }

    return i;
}

uint _mandel3D(double x, double y, uint maxit) {
    double zr = 0;
    double zi = 0;
    double zrsqr = 0;
    double zisqr = 0;
    double zrcub = 0;
    double zicub = 0;
    uint i = 0;

    for(i = 0; i < maxit; i++)
    {
        zi = 3 * zrsqr * zi - zicub + y;
        zr = zrcub - 3 * zr * zisqr + x;
        
        zrsqr = zr * zr;
        zisqr = zi * zi;
        zrcub = zrsqr * zr;
        zicub = zisqr * zi;

        if(zrsqr + zisqr > 4.0) break;
    }

    return i;
}

// uint _anyExpressionF(float x, float y, uint maxit) {
//     float zr = 0;
//     float zr_temp = 0;
//     float zi = 0;
//     float zi_temp = 0;
//     uint i = 0;

//     for(i = 0; i < maxit; i++)
//     {
//         for(int a = 0; a < poli_size; a++)
//         {
//             zr_temp += poli_data[2*a] * pow(float(zr), float(poli_data[2*a+1]));
//         }

//         if(zr * zr + zi * zi > 4.0) break;
//     }

//     return i;
// }

void main()
{
    uint it = 0;

    if(d_prec == 0)
    {
        float lx = ((float(gl_GlobalInvocationID.x) / width - 0.5) * 2 * zoom * (16.0 / 9.0) - px);
        float ly = ((float(gl_GlobalInvocationID.y) / height - 0.5) * 2 * zoom + py);
        if(set == 0)
            it = _mandelF(lx, ly, iterations);
        else if(set == 1)
            it = _shipF(lx, -ly, iterations);
        else if(set == 2)
            it = _mandel3F(lx, ly, iterations);
    }
    else
    {
        double lx = ((double(gl_GlobalInvocationID.x) / width - 0.5) * 2 * zoomd * (16.0 / 9.0) - pxd);
        double ly = ((double(gl_GlobalInvocationID.y) / height - 0.5) * 2 * zoomd + pyd);
        if(set == 0)
            it = _mandelD(lx, ly, iterations);
        else if(set == 1)
            it = _shipD(lx, -ly, iterations);
        else if(set == 3)
            it = _mandel3D(lx, ly, iterations);
    }

    float c = 1.0 - float(it) / float(iterations);

    if(cmode == 0)
    {
        vec3 color = HSVtoRGB(c * 360, 100, 100);
        imageStore(img_output, ivec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y), vec4(color, 1.0));
    }
    else
    {
        imageStore(img_output, ivec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y), vec4(c * colorGrad, 1.0));
    }
}
