#include "largo_plazo.h"
#include <semaphore.h>
#include <pthread.h>
#include "inicializador.h"
#include "estados.h"
#include "pcb.h"
#include "cliente_placa.h"

#define ERROR_SEM_INIT -1

static sem_t sem_jobs_en_new;
static sem_t sem_grado_multiprogramacion;

static void* planificar_largo_plazo(void* arg) {
    (void) arg;  // firma exigida por pthread_create, no se usa

    while(1) {
        sem_wait(&sem_jobs_en_new);
        sem_wait(&sem_grado_multiprogramacion);
        t_pcb* pcb = primer_pcb_de_cola(ESTADO_NEW);
        if (pcb == NULL) {
            log_error(planificador_logger, "Error, no hay pcbs encolados");
            sem_post(&sem_grado_multiprogramacion);  // devolver el lugar: no se admitió a nadie
            continue;
        }
        bool job_creado = placa_crear_job(pcb->jid, pcb->archivo_pseudocodigo);

        if (!job_creado) {
            log_error(planificador_logger, "Error en la placa al crear un job");
            exit(EXIT_FAILURE);
        }

        cambiar_estado(pcb, ESTADO_READY);
    }
}

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

void iniciar_largo_plazo(void) {
    pthread_t hilo_largo_plazo;
    int resultado = pthread_create(&hilo_largo_plazo, NULL, planificar_largo_plazo, NULL);
    if (resultado != 0) {
        log_error(planificador_logger, "Error al crear el hilo de largo plazo: %s", strerror(resultado));
        exit(EXIT_FAILURE);
    }

    pthread_detach(hilo_largo_plazo);  // no termina nunca: nadie le hace join
}

void largo_plazo_liberar_lugar() {
    sem_post(&sem_grado_multiprogramacion);
}