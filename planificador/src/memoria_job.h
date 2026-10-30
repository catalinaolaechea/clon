#ifndef MEMORIA_JOB_H
#define MEMORIA_JOB_H

#include <stdint.h>
#include <stdbool.h>
#include <commons/collections/list.h>

void* leer_memoria_job(uint32_t jid, t_list* direcciones, uint32_t tamanio);

bool escribir_memoria_job(uint32_t jid, t_list* direcciones, void* datos);

#endif
