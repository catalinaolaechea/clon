#ifndef JOBS_H
#define JOBS_H

#include <stdint.h>
#include "pcb.h"

#define LOG_CREACION_JOB "## (%u) Se crea el Job - Estado: NEW"

uint32_t crear_job(char* archivo_pseudocodigo);

// Pasa el Job a EXIT. Despues de llamarla no se usa mas 'pcb': SIS-54 la va a liberar aca.
void finalizar_job(t_pcb* pcb);

#endif