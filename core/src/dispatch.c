#include "dispatch.h"
#include "syscalls.h"

static const char* motivo_to_string(t_motivo_devolucion motivo) {
    switch (motivo) {
        case MOTIVO_FIN_QUANTUM:        return "FIN_QUANTUM";
        case MOTIVO_PAGE_FAULT:         return "PAGE_FAULT";
        case MOTIVO_SYSCALL_BLOQUEANTE: return "SYSCALL_BLOQUEANTE";
        case MOTIVO_EXIT:               return "EXIT";
        default:                        return "DESCONOCIDO";
    }
}

// CHECK INTERRUPT: va al final de cada ciclo que deja seguir al Job, nunca en el medio de una instruccion.
static bool check_interrupt(void) {
    if (!hay_interrupcion()) return false;

    log_info(core_logger, LOG_INTERRUPCION);
    return true;
}

t_motivo_devolucion ejecutar_job(uint32_t jid, t_contexto* contexto) {

    while (1) {
        t_instruccion* pendiente = NULL;
        t_resultado_ciclo resultado = ejecutar_ciclo(jid, contexto, &pendiente);

        if (resultado == CICLO_CONTINUA) {
            if (check_interrupt()) return MOTIVO_FIN_QUANTUM;
            continue;
        }

        if (resultado == CICLO_ERROR) {
            // El motivo ya quedo logueado en ejecutar_ciclo. Se devuelve el Job
            // para no dejar al Planificador esperando a un Core que no responde.
            log_error(core_logger, "Core %d: se aborta el Job %u por error en el ciclo (PC %u)", identificador, jid, contexto->pc);
            return MOTIVO_EXIT;
        }

        // CICLO_NO_BASICA
        if (strcmp(pendiente->nombre, "EXIT") == 0) {
            destruir_instruccion(pendiente);
            return MOTIVO_EXIT;
        }

        // INIT_JOB no bloquea: se pide al Planificador, se espera la respuesta y sigue el mismo Job.
        if (strcmp(pendiente->nombre, "INIT_JOB") == 0) {
            if (pendiente->cantidad_parametros != 1) {
                log_error(core_logger, "Core %d: INIT_JOB espera 1 parametro y recibio %d (JID %u, PC %u)", identificador, pendiente->cantidad_parametros, jid, contexto->pc);
                destruir_instruccion(pendiente);
                return MOTIVO_EXIT;
            }

            bool ok = solicitar_init_job(jid, pendiente->parametros[0]);
            destruir_instruccion(pendiente);  // recien ahora: parametros[0] apunta adentro de la instruccion

            if (!ok) return MOTIVO_EXIT;  // el motivo ya quedo logueado

            contexto->pc++;  // ejecutar_ciclo no suma 1 en las no basicas: sin esto se repite el INIT_JOB
            if (check_interrupt()) return MOTIVO_FIN_QUANTUM;
            continue;
        }

        // TODO: syscalls (ALLOC, ...) e instrucciones de entrenamiento.
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

// DISPATCH_JOB = { uint32 jid, contexto }. Ejecuta el Job y lo devuelve.
static int atender_dispatch_job(t_buffer* buffer) {
    uint32_t jid = buffer_read_uint32(buffer);
    t_contexto contexto;
    buffer_read_contexto(buffer, &contexto);

    log_info(core_logger, LOG_JOB_RECIBIDO, identificador, jid, contexto.pc);

    // Una interrupcion que llego cuando no habia Job corriendo no es para este.
    limpiar_interrupcion();

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

        if (estado != CONEXION_OK) return;   // se cayo la conexion con el Planificador
    }
}