#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocol.h"
#include <stdlib.h>
#include <time.h>
#include "bmp.h"
#include "procesare_cpu.h"
#include "procesare_cuda.h"

static int folosesteGPU = 0;

static int salveazaFisier(const char *nume, const void *date, size_t len)
{
    FILE *f = fopen(nume, "wb");
    if (f == NULL)
        return -1;

    if (fwrite(date, 1, len, f) < len)
    {
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}

static int recvAll(int sock, void *buf, size_t len)
{
    uint8_t *data = (uint8_t *)buf;
    size_t octetiPrimiti = 0;
    while (octetiPrimiti < len)
    {
        ssize_t ct = recv(sock, data + octetiPrimiti, len - octetiPrimiti, 0);
        if (ct <= 0)
        {
            return -1;
        }
        octetiPrimiti += ct;
    }
    return 0;
}

static int sendAll(int sock, const void *buf, size_t len)
{
    const uint8_t *data = (const uint8_t *)buf;
    size_t octetiTrimisi = 0;
    while (octetiTrimisi < len)
    {
        ssize_t ct = send(sock, data + octetiTrimisi, len - octetiTrimisi, MSG_NOSIGNAL);
        if (ct <= 0)
        {
            return -1;
        }
        octetiTrimisi += ct;
    }
    return 0;
}

static void freeBuffers(uint8_t *rgb, uint8_t *gray,
                        uint8_t *blur, uint8_t *sobel)
{
    free(rgb);
    free(gray);
    free(blur);
    free(sobel);
}

static double durata_ms(struct timespec start, struct timespec stop)
{
    return (stop.tv_sec - start.tv_sec) * 1000.0 + (stop.tv_nsec - start.tv_nsec) / 1000000.0;
}

static uint8_t *proceseazaImagine(const uint8_t *bmp, uint32_t lenBmp, uint32_t *lenRezultat)
{
    bmp_info_t bmpFile;
    if (bmp_parse(bmp, lenBmp, &bmpFile) < 0)
    {
        printf("Fisierul nu e un BMP suportat\n");
        return NULL;
    }
    int w = bmpFile.width, h = bmpFile.height;
    printf("Imaginea: %d x %d\n", w, h);

    uint8_t *rgb = malloc(w * h * 3 * sizeof(uint8_t));
    uint8_t *gray = malloc(w * h * sizeof(uint8_t));
    uint8_t *blur = malloc(w * h * sizeof(uint8_t));
    uint8_t *sobel = malloc(w * h * sizeof(uint8_t));

    if (!rgb || !gray || !blur || !sobel)
    {
        printf("Eroare la alocarea memoriei pentru filtre\n");
        freeBuffers(rgb, gray, blur, sobel);
        return NULL;
    }

    bmp_to_rgb(bmp, &bmpFile, rgb);

    struct timespec start, stop;
    double tGray, tBlur, tSobel;

    if (folosesteGPU)
    {
        timpi_gpu_t timpi;
        if(gpu_proceseaza(rgb, sobel, w, h, &timpi) < 0)
        {
            printf("Eroare la procesarea pe GPU\n");
            freeBuffers(rgb, gray, blur, sobel);
            return NULL;
        }

        printf("Timp copiereIn %.3f ms\n", timpi.copiereIn);
        printf("Timp grayscale %.3f ms\n", timpi.grayscale);
        printf("Timp blur %.3f ms\n", timpi.blur);
        printf("Timp sobel %.3f ms\n", timpi.sobel);
        printf("Timp copiereOut %.3f ms\n", timpi.copiereOut);
        printf("Timp total %.3f ms\n", timpi.copiereIn + timpi.grayscale + timpi.blur + timpi.sobel + timpi.copiereOut);
    }
    else
    {
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
    }

    uint32_t outMax = 54 + (w * 3 + 3) * h;
    uint8_t *out = malloc(outMax * sizeof(uint8_t));
    if (!out)
    {
        printf("Eroare la alocarea fisierului de iesire\n");
        freeBuffers(rgb, gray, blur, sobel);
        return NULL;
    }

    uint32_t outLen;

    if (bmp_build_gray(sobel, w, h, out, outMax, &outLen) < 0)
    {
        printf("Eroare la crearea fisierului de iesire\n");
        free(out);
        freeBuffers(rgb, gray, blur, sobel);
        return NULL;
    }

    *lenRezultat = outLen;
    freeBuffers(rgb, gray, blur, sobel);
    return out;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Utilizare: ./server cpu|CPU|gpu|GPU\n");
        return 1;
    }
    if (strcmp(argv[1], "gpu") == 0 || strcmp(argv[1], "GPU") == 0)
    {
        if(gpu_initializeaza() < 0)
        {
            printf("Eroare la initializare GPU\n");
            return -1;
        }
        printf("Procesare pe GPU\n");
        folosesteGPU = 1;
    }
    else if (strcmp(argv[1], "cpu") == 0 || strcmp(argv[1], "CPU") == 0)
    {
        printf("Procesare pe CPU\n");
        folosesteGPU = 0;
    }
    else
    {
        printf("Utilizare: ./server cpu|CPU|gpu|GPU\n");
        return 1;
    }
    int socketNr = socket(AF_INET, SOCK_STREAM, 0);
    if (socketNr < 0)
    {
        perror("Nu s-a creat socket-ul");
        return 1;
    }

    /* permite repornirea imediata a serverului pe acelasi port (fara "Address already in use") */
    int unu = 1;
    setsockopt(socketNr, SOL_SOCKET, SO_REUSEADDR, &unu, sizeof(unu));

    struct sockaddr_in socketAddr;
    memset(&socketAddr, 0, sizeof(socketAddr));
    socketAddr.sin_family = AF_INET;
    socketAddr.sin_port = htons(5000);
    socketAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(socketNr, (struct sockaddr *)&socketAddr, sizeof(socketAddr)) < 0)
    {
        perror("Eroare la legarea socket-ului de adresa si port");
        return 1;
    }
    else
        printf("Socket legat de portul 5000\n");

    if (listen(socketNr, 1) < 0)
    {
        perror("Eroare la trecerea in modul de asteptare a conexiunii");
        return 1;
    }

    while (1)
    {
        int socketConnection = accept(socketNr, NULL, NULL);
        if (socketConnection < 0)
        {
            perror("Eroare la conectare");
            continue;
        }
        else
            printf("Client conectat\n");

        antet_t antet;
        if (recvAll(socketConnection, &antet, sizeof(antet_t)) < 0)
        {
            printf("Clientul a plecat\n");
            close(socketConnection);
            continue;
        }

        if (antet.magic != MAGIC_SSC)
        {
            printf("Pachet invalid\n");
            close(socketConnection);
            continue;
        }

        if (antet.lenText > 65536 || antet.lenImagine > 8000000)
        {
            printf("Lungime invalida\n");
            close(socketConnection);
            continue;
        }

        printf("Lungime text: %u\nLungime imagine: %u\n", antet.lenText, antet.lenImagine);

        uint8_t *text = malloc(antet.lenText * sizeof(uint8_t));
        uint8_t *imagine = malloc(antet.lenImagine * sizeof(uint8_t));

        if (!text || !imagine)
        {
            free(text);
            free(imagine);
            close(socketConnection);
            continue;
        }

        if (recvAll(socketConnection, text, antet.lenText) < 0)
        {
            printf("Eroare la primire\n");
            free(text);
            free(imagine);
            close(socketConnection);
            continue;
        }

        cpu_majuscule(text, antet.lenText);

        if (recvAll(socketConnection, imagine, antet.lenImagine) < 0)
        {
            printf("Eroare la primire\n");
            free(text);
            free(imagine);
            close(socketConnection);
            continue;
        }

        uint32_t lenImagineProcesata;
        uint8_t *imagineProcesata = proceseazaImagine(imagine, antet.lenImagine, &lenImagineProcesata);
        if (!imagineProcesata)
        {
            printf("Imaginea nu s-a putut procesa\n");
            free(text);
            free(imagine);
            close(socketConnection);
            continue;
        }
        antet_t antetRaspuns;
        antetRaspuns.magic = MAGIC_SSC;
        antetRaspuns.lenText = antet.lenText;
        antetRaspuns.lenImagine = lenImagineProcesata;

        if(salveazaFisier("rezultat.txt", text, antet.lenText) < 0)
        {
            printf("Nu s-a putut salva rezultat.txt\n");
        }
        else
        {
            printf("S-a salvat rezultat.txt cu succes\n");
        }

        if(salveazaFisier("rezultat.bmp", imagineProcesata, lenImagineProcesata) < 0)
        {
            printf("Nu s-a putut salva rezultat.bmp\n");
        }
        else
        {
            printf("S-a salvat rezultat.bmp cu succes\n");
        }

        if (sendAll(socketConnection, &antetRaspuns, sizeof(antet_t)) < 0)
        {
            printf("Eroare la trimitere\n");
            free(text);
            free(imagine);
            free(imagineProcesata);
            close(socketConnection);
            continue;
        }

        if (sendAll(socketConnection, text, antet.lenText) < 0)
        {
            printf("Eroare la trimitere\n");
            free(text);
            free(imagine);
            free(imagineProcesata);
            close(socketConnection);
            continue;
        }

        if (sendAll(socketConnection, imagineProcesata, lenImagineProcesata) < 0)
        {
            printf("Eroare la trimitere\n");
            free(text);
            free(imagine);
            free(imagineProcesata);
            close(socketConnection);
            continue;
        }

        free(text);
        free(imagine);
        free(imagineProcesata);

        close(socketConnection);
        printf("Client deconectat\n");
    }
    close(socketNr);
    return 0;
}