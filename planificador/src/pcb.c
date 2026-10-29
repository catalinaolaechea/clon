#include "pcb.h"
#include <stdlib.h>
#include <string.h>
#include "inicializador.h"

static void temporal_create_and_stop(t_temporal** temporal) {
    *temporal = temporal_create();
    temporal_stop(*temporal);
}

// t_temporal no se puede poner en 0: se tira el viejo y se arranca uno nuevo
static void reiniciar_cronometro(t_temporal** temporal) {
    temporal_destroy(*temporal);
    *temporal = temporal_create();
}

static void inicializar_tiempos(t_estadisticas* estadisticas) {
    estadisticas->tiempo_total = temporal_create();
    estadisticas->tiempo_new = temporal_create();
    temporal_create_and_stop(&estadisticas->tiempo_ready);
}

char* estado_to_string(t_estado estado) {
    switch (estado) {
        case ESTADO_NEW:      return "NEW";
        case ESTADO_READY:    return "READY";
        case ESTADO_EXEC:     return "EXEC";
        case ESTADO_BLOCKED:  return "BLOCKED";
        case ESTADO_EXIT:     return "EXIT";
        default:              return "DESCONOCIDO";
    }
}

t_pcb* pcb_crear(uint32_t jid, char* archivo_pseudocodigo) {
    t_pcb* pcb = malloc(sizeof(t_pcb));

    pcb->jid = jid;
    pcb->estado = ESTADO_NEW;
    pcb->archivo_pseudocodigo = strdup(archivo_pseudocodigo);
    memset(&pcb->contexto, 0, sizeof(t_contexto));
    pcb->core_asignado = -1; // Inicialmente no asignado a ningun core
    pcb->syscall_pendiente = 0; // Inicialmente no hay syscall pendiente
    pcb->estimacion_rafaga = config_planificador.estimacion_inicial; // Inicialmente se establece la estimacion de rafaga segun la configuracion
    memset(&pcb->estadisticas, 0, sizeof(t_estadisticas));
    inicializar_tiempos(&pcb->estadisticas);
    temporal_create_and_stop(&pcb->espera_en_ready);
    temporal_create_and_stop(&pcb->rafaga_actual);

    return pcb;
}

void pcb_destruir(t_pcb* pcb) {
    if (pcb == NULL)
        return;
    
    free(pcb->archivo_pseudocodigo);
    temporal_destroy(pcb->espera_en_ready);
    temporal_destroy(pcb->rafaga_actual);
    temporal_destroy(pcb->estadisticas.tiempo_total);
    temporal_destroy(pcb->estadisticas.tiempo_ready);
    temporal_destroy(pcb->estadisticas.tiempo_new);
    free(pcb);
}

// Frenar un cronometro que ya esta frenado no cambia nada, por eso no hace falta chequearlo antes
void pcb_actualizar_estadisticas(t_pcb* pcb, t_estado anterior, t_estado siguiente) {
    t_estadisticas* estadisticas = &pcb->estadisticas;

    if (anterior == ESTADO_NEW) {
        temporal_stop(estadisticas->tiempo_new);
    }

    if (anterior == ESTADO_READY) {
        temporal_stop(estadisticas->tiempo_ready);
    }

    if (siguiente == ESTADO_READY) {
        temporal_resume(estadisticas->tiempo_ready);
    }

    if (anterior == ESTADO_READY && siguiente == ESTADO_EXEC) {
        estadisticas->transiciones_ready_exec++;
    }

    if (siguiente == ESTADO_EXIT) {
        temporal_stop(estadisticas->tiempo_total);
    }
}

// La estimacion nueva mezcla la rafaga que acaba de terminar con la estimacion anterior:
// S = alfa * rafaga real + (1 - alfa) * S anterior
static void recalcular_estimacion(t_pcb* pcb) {
    double alfa = config_planificador.hrrn_alfa;
    double rafaga_real = temporal_gettime(pcb->rafaga_actual);  // en ms, igual que ESTIMACION_INICIAL

    pcb->estimacion_rafaga = alfa * rafaga_real + (1 - alfa) * pcb->estimacion_rafaga;
}

// La espera cuenta desde la ultima vez que entro a READY y la rafaga desde la ultima vez que entro a EXEC
void pcb_actualizar_planificacion(t_pcb* pcb, t_estado anterior, t_estado siguiente) {
    if (anterior == ESTADO_READY) {
        temporal_stop(pcb->espera_en_ready);
    }

    if (anterior == ESTADO_EXEC) {
        temporal_stop(pcb->rafaga_actual);
        recalcular_estimacion(pcb);
    }

    if (siguiente == ESTADO_READY) {
        reiniciar_cronometro(&pcb->espera_en_ready);
    }

    if (siguiente == ESTADO_EXEC) {
        reiniciar_cronometro(&pcb->rafaga_actual);
    }
}