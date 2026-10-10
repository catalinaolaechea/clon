#include "syscalls.h"
#include "inicializador.h"
#include "jobs.h"
#include <utils/protocolo.h>
#include <stdlib.h>

void atender_init_job(int fd, int identificador_core, t_buffer* buffer) {
    uint32_t jid = buffer_read_uint32(buffer);
    char* archivo = buffer_read_string(buffer);

    log_info(planificador_logger, LOG_SYSCALL, jid, "INIT_JOB");

    uint32_t jid_nuevo = crear_job(archivo);

    log_debug(planificador_logger, "Job %u creó al Job %u (%s)", jid, jid_nuevo, archivo);

    free(archivo);

    t_paquete* paquete = crear_paquete(SYSCALL_OK);
    int response = enviar_paquete(fd, paquete);
    eliminar_paquete(paquete);

    if (response != CONEXION_OK) {
        log_error(planificador_logger, "No se pudo responder INIT_JOB del Job %u al Core %d", jid, identificador_core);
    }
}
