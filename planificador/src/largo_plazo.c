#include "largo_plazo.h"
#include <semaphore.h>
#include "inicializador.h"

#define ERROR_SEM_INIT -1

static sem_t sem_jobs_en_new;
static sem_t sem_grado_multiprogramacion;

void inicializar_largo_plazo(void) {
    int response = sem_init(&sem_jobs_en_new, 0, 0);
    if (response == ERROR_SEM_INIT) {
        log_error(planificador_logger, "Error al inicializar semaforo sem_jobs_en_new");
        exit(EXIT_FAILURE);
    }

    response = sem_init(&sem_grado_multiprogramacion, 0, config_planificador.grado_multiprogramacion);
    if (response == ERROR_SEM_INIT) {
        log_error(planificador_logger, "Error al inicializar semaforo sem_grado_multiprogramacion");
        exit(EXIT_FAILURE);
    }

    return;
}

void destruir_largo_plazo(void) {
    sem_destroy(&sem_jobs_en_new);
    sem_destroy(&sem_grado_multiprogramacion);
}

void largo_plazo_avisar_job_nuevo(void) {
    sem_post(&sem_jobs_en_new);
}