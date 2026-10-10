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
t_pcb* primer_pcb_de_cola(t_estado estado);
t_pcb* buscar_pcb_en_cola(t_estado estado, uint32_t jid);  // NULL si no esta en esa cola
bool sacar_pcb_de_cola(t_pcb* pcb);  // de la cola de su estado actual, false si no estaba

#endif