#ifndef CONTEXTO_H
#define CONTEXTO_H

#include <stdint.h>
#include "serializacion.h"

typedef struct {
    uint32_t pc;
    uint32_t p0, p1, p2, p3, p4, p5;
    uint32_t ax, bx, cx, dx;
    uint32_t e1, e2, e3;
} t_contexto;

void buffer_add_contexto(t_buffer* buffer, t_contexto* contexto);
void buffer_read_contexto(t_buffer* buffer, t_contexto* contexto);

#endif