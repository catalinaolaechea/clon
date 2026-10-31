#ifndef REEMPLAZO_H_
#define REEMPLAZO_H_

#include "inicializador.h"

// [Enunciado pág. 22 - Reemplazo de páginas] El algoritmo se elige por config (LRU o CLOCK-M)
// una sola vez al arrancar; elegir_victima usa el que corresponda.

typedef enum {
    ALGORITMO_LRU,
    ALGORITMO_CLOCK_M
} t_algoritmo_reemplazo;

// Lee ALGORITMO_REEMPLAZO (ya validado en validar_config). Llamar una vez al arrancar.
void inicializar_reemplazo();

// Marco víctima: una página en RAM no lockeada, de cualquier Job. -1 si están todas lockeadas.
int elegir_victima();

#endif