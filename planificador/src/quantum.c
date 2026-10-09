#include "quantum.h"
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "inicializador.h"
#include "servidores_core.h"

typedef struct {
    int identificador_core;
    uint32_t jid;
    uint32_t numero_despacho;
} t_args_quantum;

static void* esperar_quantum(void* arg) {
    t_args_quantum* args = arg;
    int identificador_core = args->identificador_core;
    uint32_t jid = args->jid;
    uint32_t numero_despacho = args->numero_despacho;
    free(args); 

    usleep(config_planificador.rr_quantum * 1000);  // RR_QUANTUM viene en ms, usleep recibe µs

    interrumpir_core(identificador_core, jid, numero_despacho);
    return NULL;
}

void iniciar_quantum(int identificador_core, uint32_t jid, uint32_t numero_despacho) {
    t_args_quantum* args = malloc(sizeof(t_args_quantum));
    args->identificador_core = identificador_core;
    args->jid = jid;
    args->numero_despacho = numero_despacho;

    pthread_t hilo_quantum;
    int resultado = pthread_create(&hilo_quantum, NULL, esperar_quantum, args);
    if (resultado != 0) {
        log_error(planificador_logger, "No se pudo iniciar el quantum del JID %u en el Core %d: %s", jid, identificador_core, strerror(resultado));
        free(args); 
        return;
    }

    pthread_detach(hilo_quantum);
}
