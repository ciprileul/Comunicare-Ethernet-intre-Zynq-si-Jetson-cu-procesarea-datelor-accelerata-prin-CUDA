#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "bmp.h"
#include "procesare_cpu.h"
#include "procesare_cuda.h"
#include <string.h>

static void freeBuffers(uint8_t *buffer, uint8_t *rgb, uint8_t *gray,
                        uint8_t *blur, uint8_t *sobel)
{
    free(buffer);
    free(rgb);
    free(gray);
    free(blur);
    free(sobel);
}

static double durata_ms(struct timespec start, struct timespec stop)
{
    return (stop.tv_sec - start.tv_sec) * 1000.0
         + (stop.tv_nsec - start.tv_nsec) / 1000000.0;
}

static void afiseazaAccelerare(double cpu, double gpu)
{
    if (gpu > 0.0)
        printf("%9.2fx\n", cpu / gpu);
    else
        printf("%10s\n", "-");
}

static void afiseazaComparatie(double tGray, double tBlur, double tSobel, const timpi_gpu_t *gpu)
{
    double calculGPU = gpu->grayscale + gpu->blur + gpu->sobel;
    double totalGPU  = calculGPU + gpu->copiereIn + gpu->copiereOut;
    double totalCPU  = tGray + tBlur + tSobel;

    printf("\n%-12s %10s %10s %10s\n", "Operatie", "CPU (ms)", "GPU (ms)", "Accelerare");
    printf("%-12s %10.3f %10.3f ", "grayscale", tGray, gpu->grayscale);
    afiseazaAccelerare(tGray, gpu->grayscale);
    printf("%-12s %10.3f %10.3f ", "blur", tBlur, gpu->blur);
    afiseazaAccelerare(tBlur, gpu->blur);
    printf("%-12s %10.3f %10.3f ", "sobel", tSobel, gpu->sobel);
    afiseazaAccelerare(tSobel, gpu->sobel);

    printf("\nCopiere CPU->GPU: %.3f ms\n", gpu->copiereIn);
    printf("Copiere GPU->CPU: %.3f ms\n", gpu->copiereOut);

    printf("\nTotal CPU:               %8.3f ms\n", totalCPU);
    printf("Total GPU (doar calcul): %8.3f ms ", calculGPU);
    afiseazaAccelerare(totalCPU, calculGPU);
    printf("Total GPU (cu copieri):  %8.3f ms ", totalGPU);
    afiseazaAccelerare(totalCPU, totalGPU);
}

int main(int argc, char** argv)
{
    if(argc < 3)
    {
        printf("Utilizare: ./test_cpu fisierIntrare fisierIesire\n");
        return 1;
    }

    FILE* f = fopen(argv[1], "rb");
    if(f == NULL)
    {
        printf("Eroare la deschiderea fisierului\n");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long fileLen = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t* buffer = malloc(fileLen * sizeof(uint8_t));
    if(buffer == NULL)
    {
        printf("Eroare la alocarea memoriei\n");
        fclose(f);
        return 1;
    }

    if(fread(buffer, 1, fileLen, f) != (size_t)fileLen)
    {
        printf("Eroare la citirea fisierului\n");
        fclose(f);
        free(buffer);
        return 1;
    }

    fclose(f);

    printf("S-au citit %ld octeti\n", fileLen);

    bmp_info_t bmpFile;
    if(bmp_parse(buffer, fileLen, &bmpFile) < 0)
    {
        printf("Fisierul nu e un BMP suportat\n");
        free(buffer);
        return 1;
    }
    int w = bmpFile.width, h = bmpFile.height;
    printf("Imaginea: %d x %d\n", w, h);

    uint8_t *rgb = malloc(w * h * 3 * sizeof(uint8_t));
    uint8_t *gray = malloc(w * h * sizeof(uint8_t));
    uint8_t *blur = malloc(w * h * sizeof(uint8_t));
    uint8_t *sobel = malloc(w * h * sizeof(uint8_t));

    if(!rgb || !gray || !blur || !sobel)
    {
        printf("Eroare la alocarea memoriei pentru filtre\n");
        free(buffer);
        free(rgb);
        free(gray);
        free(blur);
        free(sobel);
        return 1;
    }

    bmp_to_rgb(buffer, &bmpFile, rgb);
    
    struct timespec start, stop;
    double tGray, tBlur, tSobel;

    clock_gettime(CLOCK_MONOTONIC, &start);
    cpu_grayscale(rgb, gray, w, h);
    clock_gettime(CLOCK_MONOTONIC, &stop);
    tGray = durata_ms(start, stop);

    clock_gettime(CLOCK_MONOTONIC, &start);
    cpu_blur(gray, blur, w, h);
    clock_gettime(CLOCK_MONOTONIC, &stop);
    tBlur = durata_ms(start, stop);

    clock_gettime(CLOCK_MONOTONIC, &start);
    cpu_sobel(blur, sobel, w, h);
    clock_gettime(CLOCK_MONOTONIC, &stop);
    tSobel = durata_ms(start, stop);

    printf("Timp grayscale %.3f ms\n", tGray);
    printf("Timp blur %.3f ms\n", tBlur);
    printf("Timp sobel %.3f ms\n", tSobel);
    printf("Timp total %.3f ms\n", tGray + tBlur + tSobel);

    uint32_t outMax = 54 + (w * 3 + 3) * h;
    uint8_t *out = malloc(outMax * sizeof(uint8_t));
    if(!out)
    {
        printf("Eroare la alocarea fisierului de iesire\n");
        freeBuffers(buffer, rgb, gray, blur, sobel);
        return 1;
    }

    uint32_t outLen;

    if(bmp_build_gray(sobel, w, h, out, outMax, &outLen) < 0)
    {
        printf("Eroare la crearea fisierului de iesire\n");
        free(out);
        freeBuffers(buffer, rgb, gray, blur, sobel);
        return 1;
    }

    f = fopen(argv[2], "wb");
    if(f == NULL)
    {
        printf("Eroare la deschiderea fisierului de iesire\n");
        free(out);
        freeBuffers(buffer, rgb, gray, blur, sobel);
        return 1;
    }

    if(fwrite(out, 1, outLen, f) < outLen)
    {
        printf("Eroare la scriere in fisierul de iesire\n");
        free(out);
        freeBuffers(buffer, rgb, gray, blur, sobel);
        fclose(f);
        return 1;
    }

    fclose(f);

    printf("Rezultat salvat in %s\n", argv[2]);

    uint8_t *sobelGPU = NULL;
    sobelGPU = malloc(w * h * sizeof(uint8_t));
    if(!sobelGPU)
    {
        printf("Eroare la alocarea memoriei pentru rezultatul GPU\n");
        free(out);
        freeBuffers(buffer, rgb, gray, blur, sobel);
        return 1;
    }

    timpi_gpu_t tIncalzire;
    if(gpu_proceseaza(rgb, sobelGPU, w, h, &tIncalzire) < 0)
    {
        printf("Eroare la rularea pe GPU\n");
        free(sobelGPU);
        free(out);
        freeBuffers(buffer, rgb, gray, blur, sobel);
        return 1;
    }

    timpi_gpu_t tGPU;
    if(gpu_proceseaza(rgb, sobelGPU, w, h, &tGPU) < 0)
    {
        printf("Eroare la rularea pe GPU\n");
        free(sobelGPU);
        free(out);
        freeBuffers(buffer, rgb, gray, blur, sobel);
        return 1;
    }

    afiseazaComparatie(tGray, tBlur, tSobel, &tGPU);

    if(memcmp(sobel, sobelGPU, w * h) != 0)
    {
        printf("Rezultate diferite intre CPU si GPU\n");
    }
    else
    {
        printf("Rezultate identice intre CPU si GPU\n");
    }

    free(sobelGPU);
    free(out);
    freeBuffers(buffer, rgb, gray, blur, sobel);

    return 0;
}