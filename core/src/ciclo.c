#include "ciclo.h"

char* fetch_instruccion(uint32_t jid, uint32_t pc) {

    log_info(core_logger, LOG_FETCH, jid, pc);

    t_paquete* paquete = crear_paquete(FETCH_INSTRUCCION);
    buffer_add_uint32(paquete->buffer, jid);
    buffer_add_uint32(paquete->buffer, pc);

    int resultado = enviar_paquete(fd_placa, paquete);
    eliminar_paquete(paquete);

    if (resultado != CONEXION_OK) {
        log_error(core_logger, "Core %d: no se pudo pedir la instruccion (JID %u, PC %u) a la Placa", identificador, jid, pc);
        return NULL;
    }

    uint8_t op_code;
    resultado = recibir_operacion(fd_placa, &op_code);
    if (resultado != CONEXION_OK) {
        log_error(core_logger, "Core %d: la Placa no respondio el Fetch (JID %u, PC %u)", identificador, jid, pc);
        return NULL;
    }

    // Siempre hay que leer el payload completo, sea cual sea la respuesta,
    // para no dejar bytes en el socket que desalineen el proximo mensaje.
    t_buffer* buffer;
    if (recibir_buffer(fd_placa, &buffer) != CONEXION_OK) {
        log_error(core_logger, "Core %d: payload incompleto en la respuesta del Fetch (JID %u, PC %u)", identificador, jid, pc);
        return NULL;
    }

    char* instruccion = NULL;

    switch (op_code) {
        case RESPUESTA_INSTRUCCION:
            instruccion = buffer_read_string(buffer);
            break;

        case FETCH_ERROR:
            log_error(core_logger, "Core %d: la Placa no pudo dar la instruccion (JID %u, PC %u)", identificador, jid, pc);
            break;

        default:
            log_error(core_logger, "Core %d: respuesta inesperada de la Placa al Fetch (op_code %u)", identificador, op_code);
            break;
    }

    eliminar_buffer(buffer);
    return instruccion;
}

t_resultado_ciclo ejecutar_ciclo(uint32_t jid, t_contexto* contexto, t_instruccion** pendiente) {

    *pendiente = NULL;

    // FETCH
    char* linea = fetch_instruccion(jid, contexto->pc);
    if (linea == NULL) return CICLO_ERROR;

    // DECODE
    t_instruccion* instruccion = decodificar_instruccion(linea);
    if (instruccion == NULL) {
        log_error(core_logger, "Core %d: instruccion invalida (JID %u, PC %u): \"%s\"", identificador, jid, contexto->pc, linea);
        free(linea);
        return CICLO_ERROR;
    }
    free(linea);

    char* parametros = parametros_a_string(instruccion);
    log_info(core_logger, LOG_EJECUTANDO, jid, instruccion->nombre, parametros);
    free(parametros);

    // EXECUTE
    switch (ejecutar_basica(instruccion, contexto)) {
        case EJECUCION_OK:
            contexto->pc++;   // fin del ciclo: se suma 1 salvo que la instruccion haya escrito el PC
            destruir_instruccion(instruccion);
            return CICLO_CONTINUA;

        case EJECUCION_PC_MODIFICADO:
            destruir_instruccion(instruccion);
            return CICLO_CONTINUA;

        case EJECUCION_NO_BASICA:
            *pendiente = instruccion;
            return CICLO_NO_BASICA;

        case EJECUCION_ERROR:
        default:
            log_error(core_logger, "Core %d: parametros invalidos en %s (JID %u, PC %u)", identificador, instruccion->nombre, jid, contexto->pc);
            destruir_instruccion(instruccion);
            return CICLO_ERROR;
    }
}