/*
 * bmp.h - lucrul cu fisiere BMP pe Zynq
 */
#ifndef BMP_H
#define BMP_H

#include <stdint.h>

/* Dimensiunile maxime acceptate pentru imagine (1920 x 1080). */
#ifndef SSC_MAX_WIDTH
#define SSC_MAX_WIDTH   1920u
#endif
#ifndef SSC_MAX_HEIGHT
#define SSC_MAX_HEIGHT  1080u
#endif

/* Informatiile din header-ul unui BMP, de care ai nevoie la conversie. */
typedef struct {
    int width;            /* latimea in pixeli                                       */
    int height;           /* inaltimea in pixeli (mereu pozitiva aici)                */
    int bytes_pp;         /* octeti pe pixel in fisier: 3 (24 biti) sau 4 (32 biti)   */
    int bottom_up;        /* 1 = randurile sunt de jos in sus (inaltime pozitiva in fisier) */
    uint32_t data_offset; /* de la ce octet din fisier incep pixelii                 */
    uint32_t row_stride;  /* cati octeti ocupa un rand in fisier, cu tot cu umplutura */
} bmp_info_t;

/* >>> SCRII TU <<<  (bmp.c)
 * Citeste header-ul BMP din fisierul aflat in memorie si completeaza *info.
 * Intoarce 0 daca fisierul e un BMP valid pe care il suportam, altfel un cod negativ. */
int bmp_parse(const uint8_t *file, uint32_t file_len, bmp_info_t *info);

/* >>> SCRII TU <<<  (bmp.c)
 * Converteste pixelii din fisier in formatul din protocol: RGB, 3 octeti pe pixel,
 * randuri de sus in jos, fara umplutura. rgb are loc pentru width*height*3 octeti. */
int bmp_to_rgb(const uint8_t *file, const bmp_info_t *info, uint8_t *rgb);

/* GATA FACUTA: construieste in memorie un fisier BMP de 24 de biti dintr-o imagine
 * grayscale (1 canal), ca sa o poti salva pe SD si verifica pe PC.
 * Intoarce 0 si *out_len = marimea fisierului, sau -1 daca nu incape in out_max. */
int bmp_build_gray(const uint8_t *gray, int width, int height,
                   uint8_t *out, uint32_t out_max, uint32_t *out_len);

#endif /* BMP_H */
