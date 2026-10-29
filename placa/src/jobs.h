#ifndef JOBS_H_
#define JOBS_H_

#include "inicializador.h"

// [Enunciado pág. 21 - Memoria de Instrucciones] + [pág. 22 - Creación de Job]
// Estructura administrativa de cada Job en la Placa. Asocia el Job con sus instrucciones.
// En el Check 3 se suma la tabla de páginas del Job, que por enunciado (pág. 21) es una estructura
// administrativa y NO se guarda dentro de la memoria de datos.
typedef struct {
    uint32_t jid;
    char** instrucciones;
    uint32_t cant_instrucciones;
    t_list* tabla_paginas; //indice en la lista de nr de pagina
    t_list* asignaciones; // para que se sepa que liberar
} t_job_placa;

extern t_list* lista_jobs;

extern pthread_mutex_t mutex_jobs;


t_job_placa* inicializar_job(uint32_t jid, char* nombre_archivo);
void finalizar_job(uint32_t jid);

// Devuelve una copia de la instrucción (hay que liberarla) o NULL; si es NULL, en motivo_error
// queda el t_fetch_error correspondiente.
char* obtener_instruccion(uint32_t jid, uint32_t pc, int* motivo_error);

// Busca el Job en lista_jobs. Devuelve NULL si no existe.
t_job_placa* buscar_job(uint32_t jid_buscado);

#endif
