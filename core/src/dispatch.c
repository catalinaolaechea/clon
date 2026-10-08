#include "dispatch.h"

static const char* motivo_to_string(t_motivo_devolucion motivo) {
    switch (motivo) {
        case MOTIVO_FIN_QUANTUM:        
            return "FIN_QUANTUM";
        case MOTIVO_PAGE_FAULT:         
            return "PAGE_FAULT";
        case MOTIVO_SYSCALL_BLOQUEANTE: 
            return "SYSCALL_BLOQUEANTE";
        case MOTIVO_EXIT:               
            return "EXIT";
        default:                        
            return "DESCONOCIDO";
    }
}

t_motivo_devolucion ejecutar_job(uint32_t jid, t_contexto* contexto) {

    while (1) {
        t_instruccion* pendiente = NULL;
        t_resultado_ciclo resultado = ejecutar_ciclo(jid, contexto, &pendiente);

        if (resultado == CICLO_CONTINUA) {
            // CHECK INTERRUPT
            continue;
        }

        if (resultado == CICLO_ERROR) {
            log_error(core_logger, "Core %d: se aborta el Job %u por error en el ciclo (PC %u)", identificador, jid, contexto->pc);
            return MOTIVO_EXIT;
        }

        // CICLO_NO_BASICA
        if (strcmp(pendiente->nombre, "EXIT") == 0) {
            destruir_instruccion(pendiente);
            return MOTIVO_EXIT;
        }

        // TODO: syscalls (INIT_JOB, ALLOC, ...) e instrucciones de entrenamiento.
        log_error(core_logger, "Core %d: instruccion %s todavia no implementada (JID %u) - se finaliza el Job", identificador, pendiente->nombre, jid);
        destruir_instruccion(pendiente);
        return MOTIVO_EXIT;
    }
}

int devolver_job(uint32_t jid, t_contexto* contexto, t_motivo_devolucion motivo) {
    t_paquete* paquete = crear_paquete(DEVOLUCION_JOB);
    buffer_add_uint32(paquete->buffer, jid);
    buffer_add_contexto(paquete->buffer, contexto);
    buffer_add_uint8(paquete->buffer, (uint8_t) motivo);

    int resultado = enviar_paquete(fd_planificador, paquete);
    eliminar_paquete(paquete);

    if (resultado != CONEXION_OK) {
        log_error(core_logger, "Core %d: no se pudo devolver el Job %u al Planificador", identificador, jid);
        return resultado;
    }

    log_info(core_logger, LOG_JOB_DEVUELTO, identificador, jid, contexto->pc, motivo_to_string(motivo));
    return CONEXION_OK;
}

static int atender_dispatch_job(t_buffer* buffer) {
    uint32_t jid = buffer_read_uint32(buffer);
    t_contexto contexto;
    buffer_read_contexto(buffer, &contexto);

    log_info(core_logger, LOG_JOB_RECIBIDO, identificador, jid, contexto.pc);

    t_motivo_devolucion motivo = ejecutar_job(jid, &contexto);
    return devolver_job(jid, &contexto, motivo);
}

void esperar_dispatch(void) {
    log_info(core_logger, "Core %d: esperando dispatch del Planificador (bloqueado)", identificador);

    while (1) {
        uint8_t op_code;
        int resultado = recibir_operacion(fd_planificador, &op_code);

        if (resultado == CONEXION_DESCONECTADO) {
            log_info(core_logger, "Core %d: el Planificador cerro la conexion de dispatch", identificador);
            return;
        }

        if (resultado != CONEXION_OK) {
            log_error(core_logger, "Core %d: error esperando al Planificador (dispatch)", identificador);
            return;
        }

        // llega algo de verdad: se lee el payload completo
        t_buffer* buffer;
        if (recibir_buffer(fd_planificador, &buffer) != CONEXION_OK) {
            log_error(core_logger, "Core %d: error recibiendo el payload del Planificador", identificador);
            return;
        }

        int estado = CONEXION_OK;

        switch (op_code) {
            case DISPATCH_JOB:
                estado = atender_dispatch_job(buffer);
                break;
            default:
                log_warning(core_logger, "Core %d: op_code %u sin manejar todavia en dispatch", identificador, op_code);
        }

        eliminar_buffer(buffer);

        if (estado != CONEXION_OK) return;  
    }
}