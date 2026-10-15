#ifndef DIRECCIONES_H
#define DIRECCIONES_H

#include <stdint.h>
#include <commons/collections/list.h>
#include "serializacion.h"

// Un pedazo de la traduccion de un rango: uno por cada pagina involucrada.
typedef struct {
    uint32_t direccion;
    uint32_t tamanio;
} t_direccion_fisica;

t_direccion_fisica* direccion_fisica_crear(uint32_t direccion, uint32_t tamanio);
void direcciones_destruir(t_list* direcciones);

void buffer_add_direcciones(t_buffer* buffer, t_list* direcciones);
t_list* buffer_read_direcciones(t_buffer* buffer);

#endif
