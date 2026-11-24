#ifndef PROCESARE_CPU_H

#define PROCESARE_CPU_H

#include <stdint.h>

void cpu_majuscule(uint8_t *text, uint32_t len);
void cpu_grayscale(const uint8_t *rgb, uint8_t *gray, int width, int height);
void cpu_blur(const uint8_t *in, uint8_t *out, int width, int height);
void cpu_sobel(const uint8_t *in, uint8_t *out, int width, int height);

#endif