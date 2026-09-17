#include "estados.h"

t_list* colas_estados[CANTIDAD_ESTADOS];

pthread_mutex_t mutex_estados = PTHREAD_MUTEX_INITIALIZER;

// Como list_destroy_and_destroy_elements recibe un puntero a void, necesitamos un wrapper para poder pasarle
// la función pcb_destruir, funcionaría igual pero con warning, en cambio, pcb_destruir recibe un puntero
// a t_pcb, y no a void, pero no tira warning porque el puntero a t_pcb es compatible con el 
// puntero a void, entonces podemos hacer un cast y listo.
static void destruir_pcb(void* pcb) {
    pcb_destruir(pcb);
}

void inicializar_estados(void) {
    for (int i = 0; i < CANTIDAD_ESTADOS; i++) {
        colas_estados[i] = list_create();
    }
}

void destruir_estados(void) {
    pthread_mutex_lock(&mutex_estados);

    for (int i = 0; i < CANTIDAD_ESTADOS; i++) {
        list_destroy_and_destroy_elements(colas_estados[i], destruir_pcb);
    }

    pthread_mutex_unlock(&mutex_estados);
}