#include "pcb.h"
#include <stdlib.h>
#include <string.h>
#include "inicializador.h"

static void temporal_create_and_stop(t_temporal** temporal) {
    *temporal = temporal_create();
    temporal_stop(*temporal);
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
    pcb->core_asignado = -1; // Inicialmente no asignado a ningún core
    pcb->syscall_pendiente = 0; // Inicialmente no hay syscall pendiente
    pcb->estimacion_rafaga = config_planificador.estimacion_inicial; // Inicialmente se establece la estimación de ráfaga según la configuración
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