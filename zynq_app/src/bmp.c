/*
 * bmp.c - conversia BMP <-> formatul din protocol
 *
 * Formatul BMP (BITMAPINFOHEADER), toate numerele sunt LITTLE-ENDIAN
 * (octetul cel mai putin semnificativ primul):
 *
 *   offset  marime  camp
 *     0       2     semnatura 'B','M'
 *     2       4     marimea fisierului
 *    10       4     offset-ul pixelilor (de unde incep datele imaginii)
 *    14       4     marimea header-ului de informatii (40 la BITMAPINFOHEADER)
 *    18       4     latimea (cu semn)
 *    22       4     inaltimea (cu semn: POZITIVA = randuri de jos in sus,
 *                                       NEGATIVA = randuri de sus in jos)
 *    26       2     numarul de planuri (1)
 *    28       2     biti pe pixel (ne intereseaza 24 sau 32)
 *    30       4     compresia (0 = necomprimat; la 32 de biti poate fi si 3)
 *
 * Pixelii: ordinea in fisier este B, G, R (si inca un octet la 32 de biti).
 * Fiecare RAND ocupa un multiplu de 4 octeti: la sfarsitul randului se adauga
 * 0-3 octeti de umplutura. Exemplu: latime 641, 24 biti -> 641*3 = 1923 octeti de
 * pixeli, rotunjit la 1924 => row_stride = 1924.
 */
#include "bmp.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/*  Functii ajutatoare (gata facute)                                         */
/* ------------------------------------------------------------------------- */

/* Citesc un numar little-endian de 16 / 32 de biti de la adresa p. */
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* ------------------------------------------------------------------------- */
/*  >>> SCRII TU <<<                                                          */
/* ------------------------------------------------------------------------- */

/*
 * TODO: completeaza *info din header si verifica ca fisierul e unul pe care il suportam.
 *
 *   1. Fisierul trebuie sa aiba cel putin 54 de octeti si sa inceapa cu 'B','M'.
 *   2. Citeste cu rd32 / rd16 campurile de la offset-urile din tabelul de sus.
 *   3. Verifica: biti pe pixel = 24 sau 32; compresia = 0 (sau 3 la 32 de biti);
 *      latimea > 0; inaltimea != 0; dimensiunile nu depasesc SSC_MAX_WIDTH / SSC_MAX_HEIGHT
 *      (definite in bmp.h).
 *   4. Inaltimea: rd32 intoarce un numar fara semn. Converteste-l la int32_t ca sa vezi
 *      daca e negativ. bottom_up = (inaltime > 0); in info pui valoarea absoluta.
 *   5. bytes_pp = biti / 8.
 *   6. row_stride = latime * bytes_pp, rotunjit in sus la un multiplu de 4.
 *   7. Verifica ca toate randurile incap in fisier:
 *      data_offset + row_stride * inaltime <= file_len.
 *      (Altfel conversia ar citi dincolo de sfarsitul bufferului.)
 *
 * Intoarce 0 la succes, sau un cod negativ diferit pentru fiecare problema
 * (ex. -1 nu e BMP, -2 format nesuportat, -3 fisier incomplet) - te ajuta la depanare.
 */
int bmp_parse(const uint8_t *file, uint32_t file_len, bmp_info_t *info)
{
    if(file_len < 54 || (file[0] != 'B' || file[1] != 'M'))
        return -1;

    short biti_pe_pixel = rd16(file + 28);
    if(biti_pe_pixel != 24 && biti_pe_pixel != 32)
        return -2;

    int compresie = rd32(file + 30);
    if (!(compresie == 0 || (compresie == 3 && biti_pe_pixel == 32)))
        return -2;

    int latime = rd32(file + 18);
    int inaltime = rd32(file + 22);
    if(latime <= 0 || inaltime == 0)                
        return -2;

    if (inaltime > 0)                                    
        info->bottom_up = 1;
    else
        info->bottom_up = 0;

    if (inaltime < 0)                                
        inaltime = -inaltime;

    if(latime > (int)SSC_MAX_WIDTH || inaltime > (int)SSC_MAX_HEIGHT)  
        return -2;

    info->width = latime;
    info->height = inaltime;                               
    info->data_offset = rd32(file + 10);
    info->bytes_pp = biti_pe_pixel / 8;
    info->row_stride = (latime * info->bytes_pp + 3) / 4 * 4;

    if(info->data_offset + info->row_stride * info->height > file_len)
        return -3;

    return 0;
}

/*
 * TODO: copiaza pixelii din fisier in rgb, in formatul din protocol.
 *
 *   Pentru fiecare rand y al imaginii FINALE (0 = randul de sus):
 *     - randul corespunzator din fisier: daca bottom_up, e randul (height - 1 - y),
 *       altfel e chiar y;
 *     - inceputul lui in fisier: data_offset + rand_fisier * row_stride
 *       (folosesti row_stride, NU width*3 - asa sari peste umplutura);
 *     - pentru fiecare pixel x din rand: in fisier e la  inceput + x * bytes_pp,
 *       in ordinea B, G, R; in rgb il scrii la (y * width + x) * 3, in ordinea R, G, B.
 *
 * Intoarce 0.
 */
int bmp_to_rgb(const uint8_t *file, const bmp_info_t *info, uint8_t *rgb)
{
    for(int y = 0; y < info->height; y++)
    {
        int r;
        if(info->bottom_up == 1)
            r = info->height - 1 - y;
        else
            r = y;
        int r_offset = info->data_offset + r * info->row_stride;
        for(int x = 0; x < info->width; x++)
        {
            int pos = r_offset + x * info->bytes_pp;
            int dst = (y * info->width + x) * 3;
            rgb[dst]     = file[pos + 2];
            rgb[dst + 1] = file[pos + 1];
            rgb[dst + 2] = file[pos];
        }
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
/*  Gata facuta: imagine grayscale -> fisier BMP de 24 de biti                */
/* ------------------------------------------------------------------------- */
int bmp_build_gray(const uint8_t *gray, int width, int height,
                   uint8_t *out, uint32_t out_max, uint32_t *out_len)
{
    uint32_t stride = ((uint32_t)width * 3u + 3u) & ~3u;
    uint32_t size = 54u + stride * (uint32_t)height;
    if (size > out_max) return -1;

    memset(out, 0, 54);
    out[0] = 'B'; out[1] = 'M';
    wr32(out + 2, size);
    wr32(out + 10, 54);
    wr32(out + 14, 40);
    wr32(out + 18, (uint32_t)width);
    wr32(out + 22, (uint32_t)height);       /* pozitiv: de jos in sus */
    wr16(out + 26, 1);
    wr16(out + 28, 24);
    wr32(out + 34, stride * (uint32_t)height);

    for (int y = 0; y < height; y++) {
        uint8_t *dst = out + 54 + stride * (uint32_t)(height - 1 - y);
        const uint8_t *src = gray + (uint32_t)y * (uint32_t)width;
        int x;
        for (x = 0; x < width; x++) {
            dst[3 * x] = dst[3 * x + 1] = dst[3 * x + 2] = src[x];
        }
        for (x = width * 3; x < (int)stride; x++) dst[x] = 0;
    }
    *out_len = size;
    return 0;
}
