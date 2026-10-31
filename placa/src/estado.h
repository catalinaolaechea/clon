#ifndef ESTADO_H_
#define ESTADO_H_

#include "inicializador.h"

// [Enunciado pág. 23-24 - Comandos locales] La "foto" del estado de la Placa para INFO y TLS:
// todos los números se toman en el mismo instante (con los mutex tomados una sola vez) y se
// devuelven copiados, para que la consola los imprima sin tener ningún mutex tomado.

//INFO
typedef struct {
    int marcos_totales;
    int marcos_libres;
    int marcos_lockeados;
    uint32_t slots_totales;
    uint32_t slots_ocupados;
    int cant_jobs;
} t_estado_memoria;


//TLS una por job
typedef struct {
    uint32_t jid;
    uint32_t paginas_residentes;
    uint32_t paginas_totales;
} t_estado_job;

//Toma los mutex adentro y devuelve una copia
t_estado_memoria obtener_estado_memoria();



#endif