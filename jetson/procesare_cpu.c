#include "procesare_cpu.h"
#include <stdlib.h>

static const int ponderiBlur[3][3] = {
    {1, 2, 1},
    {2, 4, 2},
    {1, 2, 1}
};

static const int sobelX[3][3] = {
    {-1, 0, 1},
    {-2, 0, 2},
    {-1, 0, 1}
};
static const int sobelY[3][3] = {
    {-1, -2, -1},
    { 0,  0,  0},
    { 1,  2,  1}
};

void cpu_majuscule(uint8_t *text, uint32_t len)
{
    for(uint32_t i = 0; i < len; i++)
        if(text[i] >= 'a' && text[i] <= 'z')
            text[i] -= 'a' - 'A';
}

void cpu_grayscale(const uint8_t *rgb, uint8_t *gray, int width, int height)
{
    for(int y = 0; y < height; y++)
    {
        for(int x = 0; x < width; x++)
        {
            int pixel = y * width + x;
            int gri = (77 * rgb[3 * pixel] + 150 * rgb[3 * pixel + 1] + 29 * rgb[3 * pixel + 2]) >> 8;
            gray[pixel] = gri;
        }
    }
}

static int limiteaza(int v, int min, int max)
{
    if(v < min)
        return min;
    if(v > max)
        return max;
    return v;
}

void cpu_blur(const uint8_t *in, uint8_t *out, int width, int height)
{
    for(int y = 0; y < height; y++)
    {
        for(int x = 0; x < width; x++)
        {
            int s = 0;
            for(int dy = -1; dy <= 1; dy++)
            {
                for(int dx = -1; dx <= 1; dx++)
                {
                    int nx = limiteaza(x + dx, 0, width - 1);
                    int ny = limiteaza(y + dy, 0, height - 1);
                    int pondere = ponderiBlur[dy + 1][dx + 1];
                    s += pondere * in[ny * width + nx];
                }
            }
            out[y * width + x] = s >> 4;
        }
    }
}

void cpu_sobel(const uint8_t *in, uint8_t *out, int width, int height)
{
    for(int y = 0; y < height; y++)
    {
        for(int x = 0; x < width; x++)
        {
            int gx = 0;
            int gy = 0;
            for(int dy = -1; dy <= 1; dy++)
            {
                for(int dx = -1; dx <= 1; dx++)
                {
                    int nx = limiteaza(x + dx, 0, width - 1);
                    int ny = limiteaza(y + dy, 0, height - 1);
                    gx += sobelX[dy + 1][dx + 1] * in[ny * width + nx];
                    gy += sobelY[dy + 1][dx + 1] * in[ny * width + nx];
                }
            }
            int val = abs(gx) + abs(gy);
            out[y * width + x] = limiteaza(val, 0, 255);
        }
    }
}