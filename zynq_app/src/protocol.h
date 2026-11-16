#ifndef PROTOCOL_H
#define PROTOCOL_H

#define MAGIC_SSC 0x53534337u
#include <stdint.h>

typedef struct {
    uint32_t magic;
    uint32_t lenText;
    uint32_t lenImagine;
} antet_t;

#endif