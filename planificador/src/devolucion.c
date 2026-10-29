#include "devolucion.h"
#include "inicializador.h"
#include "estados.h"
#include "servidores_core.h"
#include "corto_plazo.h"
#include "jobs.h"
#include "syscalls.h"
#include "cliente_placa.h"

#define DEVOLUCION_SIZE (sizeof(uint32_t) + CONTEXTO_SIZE + sizeof(uint8_t))

// Libera el Core y guarda el contexto en la PCB. NULL si la devolucion no corresponde y se descarta.
static t_pcb* recibir_job_de_core(int identificador_core, uint32_t jid, t_contexto* contexto) {
    if (!liberar_core(identificador_core, jid)) {
        log_error(planificador_logger, "El Core %d devolvio el Job %u, que no tenia asignado: se descarta", identificador_core, jid);
        return NULL;
    }

    t_pcb* pcb = buscar_pcb_en_cola(ESTADO_EXEC, jid);
    if (pcb == NULL) {
        log_error(planificador_logger, "El Core %d devolvio el Job %u, que no esta en EXEC: se descarta", identificador_core, jid);
        return NULL;
    }

    pcb->contexto = *contexto;
    pcb->core_asignado = -1;

    return pcb;
}

void recibir_devolucion(int identificador_core, t_buffer* buffer) {
    if (buffer->size != DEVOLUCION_SIZE) {
        log_error(planificador_logger, "DEVOLUCION_JOB mal formada del Core %d: %u bytes", identificador_core, buffer->size);
        return;
    }

    uint32_t jid = buffer_read_uint32(buffer);
    t_contexto contexto;
    buffer_read_contexto(buffer, &contexto);
    uint8_t motivo = buffer_read_uint8(buffer);

    t_pcb* pcb = recibir_job_de_core(identificador_core, jid, &contexto);
    if (pcb == NULL) return;

    log_debug(planificador_logger, "DEVOLUCION_JOB JID %u <- Core %d | PC=%u - Motivo: %u", jid, identificador_core, contexto.pc, motivo);

    switch (motivo) {
        case MOTIVO_FIN_QUANTUM:
            log_info(planificador_logger, LOG_DESALOJO_QUANTUM, jid);
            cambiar_estado(pcb, ESTADO_READY);  // queda al final de la cola
            corto_plazo_avisar_job_ready();
            break;
        case MOTIVO_EXIT:
            log_info(planificador_logger, LOG_SYSCALL, jid, "EXIT");
            pcb->estadisticas.cantidad_syscalls++;  // EXIT tambien cuenta: antes de imprimir las estadisticas
            finalizar_job(pcb, FINALIZACION_EXIT);  // ultimo uso de 'pcb': se libera adentro
            break;
        default:
            log_warning(planificador_logger, "Job %u devuelto con motivo %u, todavia no soportado", jid, motivo);
    }
}

void recuperar_job_de_core_caido(uint32_t jid, int identificador_core) {
    t_pcb* pcb = reclamar_job_en_exec(jid, identificador_core);
    if (pcb == NULL) {
        // ya lo devolvio el corto plazo (le fallo el despacho) o el Job ya no estaba en EXEC
        log_debug(planificador_logger, "El Job %u ya no estaba en EXEC con el Core %d", jid, identificador_core);
        return;
    }

    // el deslockeo va antes de READY: si no, otro Core podria tomar el Job y perder sus locks nuevos
    if (!placa_deslockear_paginas(jid)) {
        log_error(planificador_logger, "No se pudieron deslockear las paginas del Job %u: vuelve a READY igual", jid);
    }

    // el contexto queda el del ultimo despacho: lo que el Core ejecuto despues se perdio con el
    cambiar_estado(pcb, ESTADO_READY);
    corto_plazo_avisar_job_ready();
}
