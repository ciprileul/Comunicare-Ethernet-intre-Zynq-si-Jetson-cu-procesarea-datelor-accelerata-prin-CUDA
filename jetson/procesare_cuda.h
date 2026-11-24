#ifndef PROCESARE_CUDA_H
#define PROCESARE_CUDA_H

#include <stdint.h>

typedef struct {
    float copiereIn;
    float grayscale;
    float blur;
    float sobel;
    float copiereOut;
} timpi_gpu_t;

#ifdef __cplusplus
extern "C" {
#endif

int gpu_proceseaza(const uint8_t *rgb, uint8_t *rezultat, int width, int height, timpi_gpu_t *timpi);
int gpu_initializeaza(void);

#ifdef __cplusplus
}
#endif

#endif