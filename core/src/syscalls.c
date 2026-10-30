#include "syscalls.h"
#include "conexiones.h"
#include <utils/protocolo.h>

// Sin mutex: el unico que usa fd_planificador es el hilo principal (las interrupciones van por fd_interrupt).
bool solicitar_init_job(uint32_t jid, char* archivo) {
    t_paquete* paquete = crear_paquete(SYSCALL_INIT_JOB);
    buffer_add_uint32(paquete->buffer, jid);
    buffer_add_string(paquete->buffer, archivo);

    int resultado = enviar_paquete(fd_planificador, paquete);
    eliminar_paquete(paquete);

    if (resultado != CONEXION_OK) {
        log_error(core_logger, "Core %d: no se pudo pedir INIT_JOB %s (JID %u) al Planificador", identificador, archivo, jid);
        return false;
    }

    uint8_t op_code;
    if (recibir_operacion(fd_planificador, &op_code) != CONEXION_OK) {
        log_error(core_logger, "Core %d: el Planificador no respondio INIT_JOB (JID %u)", identificador, jid);
        return false;
    }

    // Se lee el payload aunque venga vacio, para no desalinear el socket.
    t_buffer* buffer;
    if (recibir_buffer(fd_planificador, &buffer) != CONEXION_OK) {
        log_error(core_logger, "Core %d: respuesta incompleta de INIT_JOB (JID %u)", identificador, jid);
        return false;
    }
    eliminar_buffer(buffer);

    if (op_code != SYSCALL_OK) {
        log_error(core_logger, "Core %d: respuesta inesperada a INIT_JOB (JID %u): op_code %u", identificador, jid, op_code);
        return false;
    }

    return true;
}

// registro puntero: P0..P5
static bool es_puntero(const char* nombre) {
    return strlen(nombre) == 2 && nombre[0] == 'P' && nombre[1] >= '0' && nombre[1] <= '5';
}

// manda { jid, valor } y lee la respuesta completa. false si falla la red
static bool pedir_al_planificador(uint8_t op_code, const char* nombre, uint32_t jid, uint32_t valor,
                                  uint8_t* op_respuesta, t_buffer** respuesta) {
    t_paquete* paquete = crear_paquete(op_code);
    buffer_add_uint32(paquete->buffer, jid);
    buffer_add_uint32(paquete->buffer, valor);

    int resultado = enviar_paquete(fd_planificador, paquete);
    eliminar_paquete(paquete);

    if (resultado != CONEXION_OK) {
        log_error(core_logger, "Core %d: no se pudo pedir %s (JID %u) al Planificador", identificador, nombre, jid);
        return false;
    }

    if (recibir_operacion(fd_planificador, op_respuesta) != CONEXION_OK) {
        log_error(core_logger, "Core %d: el Planificador no respondio %s (JID %u)", identificador, nombre, jid);
        return false;
    }

    if (recibir_buffer(fd_planificador, respuesta) != CONEXION_OK) {
        log_error(core_logger, "Core %d: respuesta incompleta de %s (JID %u)", identificador, nombre, jid);
        return false;
    }

    return true;
}

bool ejecutar_alloc(uint32_t jid, const t_instruccion* instruccion, t_contexto* contexto) {
    if (instruccion->cantidad_parametros != 2) {
        log_error(core_logger, "Core %d: ALLOC espera 2 parametros y recibio %d (JID %u, PC %u)", identificador, instruccion->cantidad_parametros, jid, contexto->pc);
        return false;
    }

    uint32_t* registro = es_puntero(instruccion->parametros[0]) ? registro_por_nombre(contexto, instruccion->parametros[0]) : NULL;
    uint32_t tamanio;

    if (registro == NULL || !valor_de(contexto, instruccion->parametros[1], &tamanio)) {
        log_error(core_logger, "Core %d: ALLOC con parametros invalidos (%s, %s) (JID %u, PC %u)",
                  identificador, instruccion->parametros[0], instruccion->parametros[1], jid, contexto->pc);
        return false;
    }

    uint8_t op_code;
    t_buffer* buffer;
    if (!pedir_al_planificador(SYSCALL_ALLOC, "ALLOC", jid, tamanio, &op_code, &buffer)) return false;

    bool ok = false;

    if (op_code == RESPUESTA_ALLOC && buffer->size >= sizeof(uint32_t)) {
        *registro = buffer_read_uint32(buffer);
        ok = true;

    } else if (op_code == SYSCALL_ERROR) {
        log_error(core_logger, "Core %d: el Planificador no pudo hacer el ALLOC de %u bytes (JID %u)", identificador, tamanio, jid);

    } else {
        log_error(core_logger, "Core %d: respuesta inesperada a ALLOC (JID %u): op_code %u", identificador, jid, op_code);
    }

    eliminar_buffer(buffer);
    return ok;
}

bool ejecutar_free(uint32_t jid, const t_instruccion* instruccion, t_contexto* contexto) {
    if (instruccion->cantidad_parametros != 1) {
        log_error(core_logger, "Core %d: FREE espera 1 parametro y recibio %d (JID %u, PC %u)", identificador, instruccion->cantidad_parametros, jid, contexto->pc);
        return false;
    }

    uint32_t* registro = es_puntero(instruccion->parametros[0]) ? registro_por_nombre(contexto, instruccion->parametros[0]) : NULL;

    if (registro == NULL) {
        log_error(core_logger, "Core %d: FREE con un parametro invalido (%s) (JID %u, PC %u)", identificador, instruccion->parametros[0], jid, contexto->pc);
        return false;
    }

    uint8_t op_code;
    t_buffer* buffer;
    if (!pedir_al_planificador(SYSCALL_FREE, "FREE", jid, *registro, &op_code, &buffer)) return false;
    eliminar_buffer(buffer);

    if (op_code == SYSCALL_ERROR) {
        log_error(core_logger, "Core %d: el Planificador no pudo hacer el FREE de la direccion %u (JID %u)", identificador, *registro, jid);
        return false;
    }
    if (op_code != SYSCALL_OK) {
        log_error(core_logger, "Core %d: respuesta inesperada a FREE (JID %u): op_code %u", identificador, jid, op_code);
        return false;
    }

    return true;
}