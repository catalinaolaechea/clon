#include "dispatch.h"
#include "syscalls.h"
#include "mmu.h"
#include "entrenamiento.h"

static const char* motivo_to_string(t_motivo_devolucion motivo) {
    switch (motivo) {
        case MOTIVO_FIN_QUANTUM:        return "FIN_QUANTUM";
        case MOTIVO_PAGE_FAULT:         return "PAGE_FAULT";
        case MOTIVO_SYSCALL_BLOQUEANTE: return "SYSCALL_BLOQUEANTE";
        case MOTIVO_EXIT:               return "EXIT";
        default:                        return "DESCONOCIDO";
    }
}

static bool check_interrupt(uint32_t jid) {
    uint32_t jid_recibido = 0;

    switch (consumir_interrupcion(jid, &jid_recibido)) {
        case INTERRUPCION_PARA_ESTE_JOB:
            log_info(core_logger, LOG_INTERRUPCION);
            return true;

        case INTERRUPCION_DE_OTRO_JOB:
            log_info(core_logger, LOG_INTERRUPCION_DESCARTADA, identificador, jid_recibido, jid);
            return false;

        case INTERRUPCION_NINGUNA:
        default:
            return false;
    }
}

t_motivo_devolucion ejecutar_job(uint32_t jid, t_contexto* contexto, uint32_t* pagina_faltante) {

    *pagina_faltante = 0;   // solo vale si se devuelve MOTIVO_PAGE_FAULT

    while (1) {
        t_instruccion* pendiente = NULL;
        t_resultado_ciclo resultado = ejecutar_ciclo(jid, contexto, &pendiente);

        if (resultado == CICLO_CONTINUA) {
            if (!liberar_paginas_del_ciclo(jid)) return MOTIVO_EXIT;   // fin del ciclo: nada lockeado
            if (check_interrupt(jid)) return MOTIVO_FIN_QUANTUM;
            continue;
        }

        if (resultado == CICLO_ERROR) {
            // El motivo ya quedo logueado en ejecutar_ciclo. Se devuelve el Job y no deja esperando al Planificador
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
            destruir_instruccion(pendiente);  

            if (!ok) return MOTIVO_EXIT;  // motivo ya logueado

            contexto->pc++;  
            if (!liberar_paginas_del_ciclo(jid)) return MOTIVO_EXIT;   // fin del ciclo: nada lockeado
            if (check_interrupt(jid)) return MOTIVO_FIN_QUANTUM;
            continue;
        }

        // FORWARD / BACKWARD / UPDATE: leen y escriben la memoria del Job a traves de la MMU.
        if (es_instruccion_entrenamiento(pendiente->nombre)) {
            t_resultado_entrenamiento entrenamiento = ejecutar_entrenamiento(jid, pendiente, contexto, pagina_faltante);
            destruir_instruccion(pendiente);

            if (entrenamiento == ENTRENAMIENTO_PAGE_FAULT) return MOTIVO_PAGE_FAULT;   // PC sin sumar: se reintenta entera
            if (entrenamiento == ENTRENAMIENTO_ERROR) return MOTIVO_EXIT;              // el motivo ya quedo logueado

            contexto->pc++;  // ejecutar_ciclo no suma 1 en las no basicas
            if (!liberar_paginas_del_ciclo(jid)) return MOTIVO_EXIT;   // fin del ciclo: nada lockeado
            if (check_interrupt(jid)) return MOTIVO_FIN_QUANTUM;
            continue;
        }

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

int devolver_page_fault(uint32_t jid, t_contexto* contexto, uint32_t nro_pagina) {
    t_paquete* paquete = crear_paquete(DEVOLUCION_PAGE_FAULT);
    buffer_add_uint32(paquete->buffer, jid);
    buffer_add_contexto(paquete->buffer, contexto);
    buffer_add_uint32(paquete->buffer, nro_pagina);

    int resultado = enviar_paquete(fd_planificador, paquete);
    eliminar_paquete(paquete);

    if (resultado != CONEXION_OK) {
        log_error(core_logger, "Core %d: no se pudo devolver el Job %u por Page Fault al Planificador", identificador, jid);
        return resultado;
    }

    log_info(core_logger, LOG_JOB_DEVUELTO, identificador, jid, contexto->pc, motivo_to_string(MOTIVO_PAGE_FAULT));
    return CONEXION_OK;
}

// DISPATCH_JOB = { uint32 jid, contexto }. Ejecuta el Job y lo devuelve.
static int atender_dispatch_job(t_buffer* buffer) {
    uint32_t jid = buffer_read_uint32(buffer);
    t_contexto contexto;
    buffer_read_contexto(buffer, &contexto);

    log_info(core_logger, LOG_JOB_RECIBIDO, identificador, jid, contexto.pc);

    uint32_t pagina_faltante = 0;
    t_motivo_devolucion motivo = ejecutar_job(jid, &contexto, &pagina_faltante);

    // Antes de devolver el Job no puede quedar nada lockeado. La unica excepcion son las syscalls de
    // servicio (COR-13): dejan las paginas lockeadas hasta que el Planificador termina el servicio.
    if (motivo != MOTIVO_SYSCALL_BLOQUEANTE && !liberar_paginas_del_ciclo(jid)) motivo = MOTIVO_EXIT;

    if (motivo == MOTIVO_PAGE_FAULT) return devolver_page_fault(jid, &contexto, pagina_faltante);
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

        // Se lee el payload completo
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