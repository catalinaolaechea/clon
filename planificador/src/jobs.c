#include "jobs.h"
#include "inicializador.h"
#include "estados.h"
#include <pthread.h>
#include "largo_plazo.h"

static pthread_mutex_t mutex_jid = PTHREAD_MUTEX_INITIALIZER;
static uint32_t jid_siguiente = 0;

static uint32_t siguiente_jid(void) {
    pthread_mutex_lock(&mutex_jid);
    uint32_t jid = jid_siguiente;
    jid_siguiente++;
    pthread_mutex_unlock(&mutex_jid);
    return jid;
}

uint32_t crear_job(char* archivo_pseudocodigo) {
    uint32_t jid = 0;
    jid = siguiente_jid();

    t_pcb* pcb = pcb_crear(jid, archivo_pseudocodigo);

    log_info(planificador_logger, LOG_CREACION_JOB, jid);

    encolar_pcb(pcb);

    largo_plazo_avisar_job_nuevo();
    return jid;
}

void finalizar_job(t_pcb* pcb) {
    cambiar_estado(pcb, ESTADO_EXIT);

    largo_plazo_liberar_lugar();
}