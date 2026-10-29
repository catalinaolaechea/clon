#include "estados.h"
#include "inicializador.h"

t_list* colas_estados[CANTIDAD_ESTADOS];

pthread_mutex_t mutex_estados = PTHREAD_MUTEX_INITIALIZER;

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

void encolar_pcb(t_pcb* pcb) {
    pthread_mutex_lock(&mutex_estados);

    list_add(colas_estados[pcb->estado], pcb);

    pthread_mutex_unlock(&mutex_estados);
}

void cambiar_estado(t_pcb* pcb, t_estado estado_siguiente) {    
    t_estado estado_anterior = pcb->estado;

    pthread_mutex_lock(&mutex_estados);

    // Remover el PCB de su estado actual
    bool result = list_remove_element(colas_estados[estado_anterior], pcb);

    if (!result) {
        log_error(planificador_logger, "Error al cambiar estado del PCB con JID %u: no se encontro en la cola de estado %d", pcb->jid, estado_anterior);
        pthread_mutex_unlock(&mutex_estados);
        return;
    }

    pcb->estado = estado_siguiente;
    pcb_actualizar_estadisticas(pcb, estado_anterior, estado_siguiente);  // bajo el mismo mutex que el cambio
    pcb_actualizar_planificacion(pcb, estado_anterior, estado_siguiente);

    // Agregar el PCB a la nueva cola de estado
    list_add(colas_estados[estado_siguiente], pcb);

    pthread_mutex_unlock(&mutex_estados);

    log_info(planificador_logger, LOG_CAMBIO_ESTADO, pcb->jid, estado_to_string(estado_anterior), estado_to_string(estado_siguiente));
}

t_pcb* primer_pcb_de_cola(t_estado estado) {
    t_pcb* pcb = NULL;
    pthread_mutex_lock(&mutex_estados);

    if (list_size(colas_estados[estado]) > 0) {
        pcb = list_get(colas_estados[estado], 0);
    }

    pthread_mutex_unlock(&mutex_estados);
    return pcb;
}

t_pcb* buscar_pcb_en_cola(t_estado estado, uint32_t jid) {
    t_pcb* encontrado = NULL;
    pthread_mutex_lock(&mutex_estados);

    for (int i = 0; i < list_size(colas_estados[estado]); i++) {
        t_pcb* pcb = list_get(colas_estados[estado], i);
        if (pcb->jid == jid) {
            encontrado = pcb;
            break;
        }
    }

    pthread_mutex_unlock(&mutex_estados);
    return encontrado;
}

bool sacar_pcb_de_cola(t_pcb* pcb) {
    pthread_mutex_lock(&mutex_estados);

    bool sacado = list_remove_element(colas_estados[pcb->estado], pcb);

    pthread_mutex_unlock(&mutex_estados);
    return sacado;
}

// El Job del Core caido lo pueden querer sacar de EXEC dos hilos a la vez: el del Core y el corto plazo.
// Gana el primero que lo reclama: le pone core_asignado en -1 y el otro ya no lo encuentra.
t_pcb* reclamar_job_en_exec(uint32_t jid, int identificador_core) {
    t_pcb* reclamado = NULL;
    pthread_mutex_lock(&mutex_estados);

    for (int i = 0; i < list_size(colas_estados[ESTADO_EXEC]); i++) {
        t_pcb* pcb = list_get(colas_estados[ESTADO_EXEC], i);
        if (pcb->jid == jid && pcb->core_asignado == identificador_core) {
            pcb->core_asignado = -1;
            reclamado = pcb;
            break;
        }
    }

    pthread_mutex_unlock(&mutex_estados);
    return reclamado;
}

// Sirve para recorrer una cola sin tener el mutex tomado todo ese tiempo
t_list* copiar_cola(t_estado estado) {
    pthread_mutex_lock(&mutex_estados);

    t_list* copia = list_duplicate(colas_estados[estado]);

    pthread_mutex_unlock(&mutex_estados);
    return copia;
}