#include "devolucion.h"
#include "inicializador.h"
#include "estados.h"
#include "servidores_core.h"

#define DEVOLUCION_SIZE (sizeof(uint32_t) + CONTEXTO_SIZE + sizeof(uint8_t))

void recibir_devolucion(int identificador_core, t_buffer* buffer) {
    if (buffer->size != DEVOLUCION_SIZE) {
        log_error(planificador_logger, "DEVOLUCION_JOB mal formada del Core %d: %u bytes", identificador_core, buffer->size);
        return;
    }

    uint32_t jid = buffer_read_uint32(buffer);
    t_contexto contexto;
    buffer_read_contexto(buffer, &contexto);
    uint8_t motivo = buffer_read_uint8(buffer);

    if (!liberar_core(identificador_core, jid)) {
        log_error(planificador_logger, "El Core %d devolvio el Job %u, que no tenia asignado: se descarta", identificador_core, jid);
        return;
    }

    t_pcb* pcb = buscar_pcb_en_cola(ESTADO_EXEC, jid);
    if (pcb == NULL) {
        log_error(planificador_logger, "El Core %d devolvio el Job %u, que no esta en EXEC: se descarta", identificador_core, jid);
        return;
    }

    // la otra mitad del context switch: el contexto se guarda tal cual llega, sin tocar el PC
    pcb->contexto = contexto;
    pcb->core_asignado = -1;

    log_debug(planificador_logger, "DEVOLUCION_JOB JID %u <- Core %d | PC=%u - Motivo: %u", jid, identificador_core, contexto.pc, motivo);

    switch (motivo) {
        default:
            log_warning(planificador_logger, "Job %u devuelto con motivo %u, todavia no soportado", jid, motivo);
    }
}
