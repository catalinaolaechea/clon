#ifndef JOBS_H
#define JOBS_H

#include <stdint.h>
#include "pcb.h"

#define LOG_CREACION_JOB "## (%u) Se crea el Job - Estado: NEW"
#define LOG_FIN_JOB "## (%u) finalizo su ejecucion con motivo de %s. Estadisticas: " \
    "Tiempo total: %lld ms - Tiempo en READY: %lld ms - Tiempo en NEW: %lld ms - " \
    "Memoria maxima: %u B - Syscalls: %u - Page faults: %u - Transiciones READY->EXEC: %u"

typedef enum {
    FINALIZACION_EXIT,             // el Job ejecuto la syscall EXIT
    FINALIZACION_ERROR_CREAR_JOB   // la Placa no pudo crear el Job (ej. el archivo no existe)
} t_motivo_finalizacion;

uint32_t crear_job(char* archivo_pseudocodigo);

// Pasa el Job a EXIT, le avisa a la Placa, loguea las estadisticas y libera la PCB.
// Despues de llamarla no se usa mas 'pcb'.
void finalizar_job(t_pcb* pcb, t_motivo_finalizacion motivo);

#endif