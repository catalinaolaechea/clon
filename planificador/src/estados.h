#ifndef ESTADOS_H
#define ESTADOS_H

#include <commons/collections/list.h>
#include <pthread.h>
#include "pcb.h"

#define CANTIDAD_ESTADOS 5

extern t_list* colas_estados[CANTIDAD_ESTADOS];

extern pthread_mutex_t mutex_estados;

void inicializar_estados(void);
void destruir_estados(void);
void cambiar_estado(t_pcb* pcb, t_estado estado_siguiente);
void encolar_pcb(t_pcb* pcb);

#endif