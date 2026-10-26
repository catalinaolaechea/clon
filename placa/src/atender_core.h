#ifndef ATENDER_CORE_H_
#define ATENDER_CORE_H_

#include "inicializador.h"

typedef struct {
    int socket_core;
    int id_core;
}t_core_placa;

extern t_list* lista_core;

extern pthread_mutex_t mutex_lista_core;


void* atender_core(void* core);

#endif
