#include "corto_plazo.h"
#include <semaphore.h>
#include <pthread.h>
#include "inicializador.h"
#include "estados.h"
#include "pcb.h"
#include "servidores_core.h"
#include "quantum.h"

#define ERROR_SEM_INIT -1
#define ESTIMACION_MINIMA_MS 1.0  // para no dividir por 0 si la estimacion llega a 0

static sem_t sem_jobs_en_ready;
static sem_t sem_cores_libres;

static t_pcb* elegir_job_fifo(void) {
    return primer_pcb_de_cola(ESTADO_READY);
}

// (W + S) / S: W es lo que lleva esperando en READY y S la estimacion de su proxima rafaga, en ms
static double response_ratio(t_pcb* pcb) {
    double espera = temporal_gettime(pcb->espera_en_ready);
    double estimacion = pcb->estimacion_rafaga;

    if (estimacion < ESTIMACION_MINIMA_MS) {
        estimacion = ESTIMACION_MINIMA_MS;
    }

    return (espera + estimacion) / estimacion;
}

// Loguea el ratio de cada Job de READY y devuelve el de mayor ratio. La cola esta en orden de
// llegada y se compara con '>', asi que en un empate gana el que llego antes.
static t_pcb* elegir_job_hrrn(void) {
    t_list* ready = copiar_cola(ESTADO_READY);

    t_pcb* elegido = NULL;
    double mayor_ratio = 0;

    for (int i = 0; i < list_size(ready); i++) {
        t_pcb* pcb = list_get(ready, i);
        double ratio = response_ratio(pcb);

        log_info(planificador_logger, LOG_PRIORIDAD_HRRN, pcb->jid, ratio, pcb->estimacion_rafaga);

        if (elegido == NULL || ratio > mayor_ratio) {
            elegido = pcb;
            mayor_ratio = ratio;
        }
    }

    list_destroy(ready);
    return elegido;
}

// El proximo Job de READY segun ALGORITMO_PLANIFICACION, o NULL si la cola esta vacia.
static t_pcb* elegir_job(void) {
    switch (config_planificador.algoritmo_planificacion) {
        case ALGORITMO_FIFO:
        case ALGORITMO_RR:  // RR elige igual que FIFO: lo que cambia es el desalojo por quantum
            return elegir_job_fifo();
        case ALGORITMO_HRRN:  // sin desalojo: solo se arranca el quantum con RR
            return elegir_job_hrrn();
        default:
            return elegir_job_fifo();  // no pasa: el algoritmo se valida al leer el config
    }
}

static void loguear_contexto_enviado(t_pcb* pcb, int identificador_core) {
    t_contexto* c = &pcb->contexto;
    log_debug(planificador_logger,
              "DISPATCH_JOB JID %u -> Core %d | PC=%u P0=%u P1=%u P2=%u P3=%u P4=%u P5=%u AX=%u BX=%u CX=%u DX=%u E1=%u E2=%u E3=%u",
              pcb->jid, identificador_core, c->pc, c->p0, c->p1, c->p2, c->p3, c->p4, c->p5,
              c->ax, c->bx, c->cx, c->dx, c->e1, c->e2, c->e3);
}

// DISPATCH_JOB = { uint32 jid, contexto } por el canal dispatch del Core (docs/protocolo.md).
static void despachar_job(t_pcb* pcb, t_core_conectado* core) {
    // EXEC antes de mandar: si no, el Core podria devolver el Job antes de que figure en EXEC
    pcb->core_asignado = core->identificador;
    cambiar_estado(pcb, ESTADO_EXEC);

    t_paquete* paquete = crear_paquete(DISPATCH_JOB);
    buffer_add_uint32(paquete->buffer, pcb->jid);
    buffer_add_contexto(paquete->buffer, &pcb->contexto);

    int resultado = enviar_paquete(core->fd_dispatch, paquete);
    eliminar_paquete(paquete);

    if (resultado != CONEXION_OK) {
        // el Core se cayo: queda ocupado hasta que se desregistre, el Job vuelve a READY
        log_error(planificador_logger, "No se pudo despachar el Job %u al Core %d", pcb->jid, core->identificador);
        pcb->core_asignado = -1;
        cambiar_estado(pcb, ESTADO_READY);
        corto_plazo_avisar_job_ready();
        return;
    }

    if (config_planificador.algoritmo_planificacion == ALGORITMO_RR) {
        iniciar_quantum(core->identificador, pcb->jid, core->numero_despacho);
    }

    log_info(planificador_logger, "Job %u despachado al Core %d", pcb->jid, core->identificador);

    loguear_contexto_enviado(pcb, core->identificador);
}

static void* planificar_corto_plazo(void* arg) {
    (void) arg;  // firma exigida por pthread_create, no se usa

    while (1) {
        sem_wait(&sem_jobs_en_ready);
        sem_wait(&sem_cores_libres);

        t_pcb* pcb = elegir_job();
        if (pcb == NULL) {
            log_error(planificador_logger, "Error, no hay Jobs en READY");
            sem_post(&sem_cores_libres);  // devolver el Core: no se despacho a nadie
            continue;
        }

        t_core_conectado core;
        if (!tomar_core_libre(pcb->jid, &core)) {
            // el aviso era de un Core que ya se desconecto: el Job sigue primero en READY
            sem_post(&sem_jobs_en_ready);
            continue;
        }

        despachar_job(pcb, &core);
    }

    return NULL;
}

void iniciar_corto_plazo(void) {
    pthread_t hilo_corto_plazo;
    int resultado = pthread_create(&hilo_corto_plazo, NULL, planificar_corto_plazo, NULL);
    if (resultado != 0) {
        log_error(planificador_logger, "Error al crear el hilo de corto plazo: %s", strerror(resultado));
        exit(EXIT_FAILURE);
    }

    pthread_detach(hilo_corto_plazo);  // no termina nunca: nadie le hace join
}

void inicializar_corto_plazo(void) {
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
