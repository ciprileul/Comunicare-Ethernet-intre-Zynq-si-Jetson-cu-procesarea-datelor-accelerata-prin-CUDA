#include <cuda_runtime.h>
#include <stdint.h>
#include "procesare_cuda.h"
#include <stdio.h>

__constant__ int ponderiBlur[3][3] = {
    {1, 2, 1},
    {2, 4, 2},
    {1, 2, 1}};
__constant__ int sobelX[3][3] = {
    {-1, 0, 1},
    {-2, 0, 2},
    {-1, 0, 1}};
__constant__ int sobelY[3][3] = {
    {-1, -2, -1},
    {0, 0, 0},
    {1, 2, 1}};

static uint8_t *d_rgb = NULL;
static uint8_t *d_gray = NULL;
static uint8_t *d_blur = NULL;
static uint8_t *d_sobel = NULL;

static int initGPU(int w, int h)
{
    cudaMalloc((void **)&d_rgb, w * h * 3);
    cudaMalloc((void **)&d_gray, w * h);
    cudaMalloc((void **)&d_blur, w * h);
    cudaMalloc((void **)&d_sobel, w * h);

    if (!d_rgb || !d_gray || !d_blur || !d_sobel)
    {
        cudaFree(d_rgb);
        cudaFree(d_gray);
        cudaFree(d_blur);
        cudaFree(d_sobel);
        d_rgb = NULL;
        d_gray = NULL;
        d_blur = NULL;
        d_sobel = NULL;
        return -1;
    }
    return 0;
}

static void freeGPU()
{
    cudaFree(d_rgb);
    cudaFree(d_gray);
    cudaFree(d_blur);
    cudaFree(d_sobel);
    d_rgb = NULL;
    d_gray = NULL;
    d_blur = NULL;
    d_sobel = NULL;
}

static int eroareKernel(const char *nume, cudaEvent_t start, cudaEvent_t stop)
{
    cudaError_t err = cudaGetLastError();
    if (err == cudaSuccess)
        return 0;

    printf("Eroare la kernelul %s: %s\n", nume, cudaGetErrorString(err));
    cudaEventDestroy(start);
    cudaEventDestroy(stop);
    freeGPU();
    return -1;
}

__device__ static int limiteaza(int v, int min, int max)
{
    if (v < min)
        return min;
    if (v > max)
        return max;
    return v;
}

__global__ void gpu_grayscale(const uint8_t *rgb, uint8_t *gray, int width, int height)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x < 0 || x >= width || y < 0 || y >= height)
        return;
    int pixel = y * width + x;
    int gri = (77 * rgb[3 * pixel] + 150 * rgb[3 * pixel + 1] + 29 * rgb[3 * pixel + 2]) >> 8;
    gray[pixel] = gri;
}

__global__ void gpu_blur(const uint8_t *in, uint8_t *out, int width, int height)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x < 0 || x >= width || y < 0 || y >= height)
        return;
    int s = 0;
    for (int dy = -1; dy <= 1; dy++)
    {
        for (int dx = -1; dx <= 1; dx++)
        {
            int nx = limiteaza(x + dx, 0, width - 1);
            int ny = limiteaza(y + dy, 0, height - 1);
            int pondere = ponderiBlur[dy + 1][dx + 1];
            s += pondere * in[ny * width + nx];
        }
    }
    out[y * width + x] = s >> 4;
}

__global__ void gpu_sobel(const uint8_t *in, uint8_t *out, int width, int height)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x < 0 || x >= width || y < 0 || y >= height)
        return;
    int gx = 0;
    int gy = 0;
    for (int dy = -1; dy <= 1; dy++)
    {
        for (int dx = -1; dx <= 1; dx++)
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

int gpu_proceseaza(const uint8_t *rgb, uint8_t *rezultat, int width, int height, timpi_gpu_t *timpi)
{
    if (initGPU(width, height) < 0)
        return -1;

    dim3 bloc(16, 16);
    dim3 grila((width + 15) / 16, (height + 15) / 16);

    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);

    cudaEventRecord(start, 0);
    cudaMemcpy(d_rgb, rgb, width * height * 3, cudaMemcpyHostToDevice);
    cudaEventRecord(stop, 0);
    cudaEventSynchronize(stop);
    cudaEventElapsedTime(&timpi->copiereIn, start, stop);

    cudaEventRecord(start, 0);
    gpu_grayscale<<<grila, bloc>>>(d_rgb, d_gray, width, height);
    if (eroareKernel("grayscale", start, stop) < 0) 
        return -1;
    cudaEventRecord(stop, 0);
    cudaEventSynchronize(stop);
    cudaEventElapsedTime(&timpi->grayscale, start, stop);

    cudaEventRecord(start, 0);
    gpu_blur<<<grila, bloc>>>(d_gray, d_blur, width, height);
    if (eroareKernel("blur", start, stop) < 0) 
        return -1;
    cudaEventRecord(stop, 0);
    cudaEventSynchronize(stop);
    cudaEventElapsedTime(&timpi->blur, start, stop);

    cudaEventRecord(start, 0);
    gpu_sobel<<<grila, bloc>>>(d_blur, d_sobel, width, height);
    if (eroareKernel("sobel", start, stop) < 0) 
        return -1;
    cudaEventRecord(stop, 0);
    cudaEventSynchronize(stop);
    cudaEventElapsedTime(&timpi->sobel, start, stop);

    cudaEventRecord(start, 0);
    cudaMemcpy(rezultat, d_sobel, width * height, cudaMemcpyDeviceToHost);
    cudaEventRecord(stop, 0);
    cudaEventSynchronize(stop);
    cudaEventElapsedTime(&timpi->copiereOut, start, stop);

    cudaEventDestroy(start);
    cudaEventDestroy(stop);

    freeGPU();
    return 0;
}

int gpu_initializeaza(void)
{
    cudaError_t err = cudaFree(0);
    if(err != cudaSuccess)
    {
        printf("%s\n", cudaGetErrorString(err));
        return -1;
    }
    return 0;
}