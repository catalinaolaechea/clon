#ifndef SERVICIOS_H
#define SERVICIOS_H

#include <stdint.h>
#include <commons/collections/list.h>
#include "pcb.h"

typedef struct {
    t_pcb* pcb;
    char* archivo;          // solo LOAD_BATCH, NULL en los demas
    uint32_t tamanio;
    t_list* direcciones;    // de t_direccion_fisica, con las paginas lockeadas
} t_pedido_servicio;

// Libera el pedido, su archivo y su lista de direcciones. La PCB no: es de las colas de estados.
void pedido_servicio_destruir(t_pedido_servicio* pedido);

#endif
