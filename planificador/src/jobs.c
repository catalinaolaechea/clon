#include "jobs.h"
#include "inicializador.h"
#include "estados.h"
#include <pthread.h>
#include "largo_plazo.h"
#include "cliente_placa.h"

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

static char* motivo_finalizacion_to_string(t_motivo_finalizacion motivo) {
    switch (motivo) {
        case FINALIZACION_EXIT:            return "EXIT";
        case FINALIZACION_ERROR_CREAR_JOB: return "ERROR_CREAR_JOB";
        default:                           return "DESCONOCIDO";
    }
}

static void loguear_fin_job(t_pcb* pcb, t_motivo_finalizacion motivo) {
    t_estadisticas* estadisticas = &pcb->estadisticas;

    log_info(planificador_logger, LOG_FIN_JOB, pcb->jid, motivo_finalizacion_to_string(motivo),
             (long long) temporal_gettime(estadisticas->tiempo_total),
             (long long) temporal_gettime(estadisticas->tiempo_ready),
             (long long) temporal_gettime(estadisticas->tiempo_new),
             estadisticas->memoria_maxima,
             estadisticas->cantidad_syscalls,
             estadisticas->cantidad_page_faults,
             estadisticas->transiciones_ready_exec);
}

void finalizar_job(t_pcb* pcb, t_motivo_finalizacion motivo) {
    cambiar_estado(pcb, ESTADO_EXIT);  // frena el tiempo total

    // aca no hay ningun mutex de estados ni de Cores tomado: la Placa tarda RETARDO_MEMORIA en responder
    if (!placa_finalizar_job(pcb->jid)) {
        log_error(planificador_logger, "La Placa no confirmo la finalizacion del Job %u: se libera igual", pcb->jid);
    }

    loguear_fin_job(pcb, motivo);

    // la cola EXIT es el unico lugar que guarda la PCB: despues de sacarla nadie la apunta
    sacar_pcb_de_cola(pcb);
    pcb_destruir(pcb);

    largo_plazo_liberar_lugar();  // si hay un Job esperando en NEW, el largo plazo lo admite
}