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
