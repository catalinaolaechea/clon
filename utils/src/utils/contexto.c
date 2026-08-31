#include "contexto.h"

void buffer_add_contexto(t_buffer* buffer, t_contexto* contexto) {
    buffer_add_uint32(buffer, contexto->pc);
    buffer_add_uint32(buffer, contexto->p0);
    buffer_add_uint32(buffer, contexto->p1);
    buffer_add_uint32(buffer, contexto->p2);
    buffer_add_uint32(buffer, contexto->p3);
    buffer_add_uint32(buffer, contexto->p4);
    buffer_add_uint32(buffer, contexto->p5);
    buffer_add_uint32(buffer, contexto->ax);
    buffer_add_uint32(buffer, contexto->bx);
    buffer_add_uint32(buffer, contexto->cx);
    buffer_add_uint32(buffer, contexto->dx);
    buffer_add_uint32(buffer, contexto->e1);
    buffer_add_uint32(buffer, contexto->e2);
    buffer_add_uint32(buffer, contexto->e3);
}

void buffer_read_contexto(t_buffer* buffer, t_contexto* contexto) {
    contexto->pc = buffer_read_uint32(buffer);
    contexto->p0 = buffer_read_uint32(buffer);
    contexto->p1 = buffer_read_uint32(buffer);
    contexto->p2 = buffer_read_uint32(buffer);
    contexto->p3 = buffer_read_uint32(buffer);
    contexto->p4 = buffer_read_uint32(buffer);
    contexto->p5 = buffer_read_uint32(buffer);
    contexto->ax = buffer_read_uint32(buffer);
    contexto->bx = buffer_read_uint32(buffer);
    contexto->cx = buffer_read_uint32(buffer);
    contexto->dx = buffer_read_uint32(buffer);
    contexto->e1 = buffer_read_uint32(buffer);
    contexto->e2 = buffer_read_uint32(buffer);
    contexto->e3 = buffer_read_uint32(buffer);
}