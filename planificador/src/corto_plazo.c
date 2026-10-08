#include "corto_plazo.h"
#include <semaphore.h>
#include "inicializador.h"
#include "estados.h"
#include "pcb.h"

#define ERROR_SEM_INIT -1

static sem_t sem_jobs_en_ready;
// Puede quedar adelantado si un Core avisa y despues se desconecta: el despacho vuelve a
// buscar el Core bajo el mutex antes de usarlo.
static sem_t sem_cores_libres;

static t_pcb* elegir_job_fifo(void) {
    return primer_pcb_de_cola(ESTADO_READY);
}

// El proximo Job de READY segun ALGORITMO_PLANIFICACION, o NULL si la cola esta vacia.
static t_pcb* elegir_job(void) {
    switch (config_planificador.algoritmo_planificacion) {
        case ALGORITMO_FIFO:
        case ALGORITMO_RR:  // RR elige igual que FIFO: lo que cambia es el desalojo por quantum
            return elegir_job_fifo();
        case ALGORITMO_HRRN:
        default:
            return elegir_job_fifo();  // TODO: HRRN, mientras tanto FIFO (avisado al inicializar)
    }
}

void inicializar_corto_plazo(void) {
    if (config_planificador.algoritmo_planificacion == ALGORITMO_HRRN) {
        log_warning(planificador_logger, "HRRN todavia no esta implementado: se planifica en FIFO");
    }

    int response = sem_init(&sem_jobs_en_ready, 0, 0);
    if (response == ERROR_SEM_INIT) {
        log_error(planificador_logger, "Error al inicializar semaforo sem_jobs_en_ready");
        exit(EXIT_FAILURE);
    }

    response = sem_init(&sem_cores_libres, 0, 0);
    if (response == ERROR_SEM_INIT) {
        log_error(planificador_logger, "Error al inicializar semaforo sem_cores_libres");
        exit(EXIT_FAILURE);
    }
}

void destruir_corto_plazo(void) {
    sem_destroy(&sem_jobs_en_ready);
    sem_destroy(&sem_cores_libres);
}

void corto_plazo_avisar_job_ready(void) {
    sem_post(&sem_jobs_en_ready);
}

void corto_plazo_avisar_core_libre(void) {
    sem_post(&sem_cores_libres);
}
