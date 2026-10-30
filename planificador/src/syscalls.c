#include "syscalls.h"
#include "inicializador.h"
#include "jobs.h"
#include "estados.h"
#include "devolucion.h"
#include "servicios.h"
#include <utils/protocolo.h>
#include <utils/contexto.h>
#include <utils/direcciones.h>
#include <stdlib.h>

void atender_init_job(int fd, int identificador_core, t_buffer* buffer) {
    uint32_t jid = buffer_read_uint32(buffer);
    char* archivo = buffer_read_string(buffer);

    log_info(planificador_logger, LOG_SYSCALL, jid, "INIT_JOB");

    // el que la pide esta en EXEC: solo lo toca el hilo de su Core, que es este
    t_pcb* pcb = buscar_pcb_en_cola(ESTADO_EXEC, jid);
    if (pcb != NULL) {
        pcb->estadisticas.cantidad_syscalls++;
    } else {
        log_warning(planificador_logger, "INIT_JOB del Job %u, que no esta en EXEC: no se cuenta la syscall", jid);
    }

    uint32_t jid_nuevo = crear_job(archivo);

    log_debug(planificador_logger, "Job %u creo al Job %u (%s)", jid, jid_nuevo, archivo);

    free(archivo);

    t_paquete* paquete = crear_paquete(SYSCALL_OK);
    int response = enviar_paquete(fd, paquete);
    eliminar_paquete(paquete);

    if (response != CONEXION_OK) {
        log_error(planificador_logger, "No se pudo responder INIT_JOB del Job %u al Core %d", jid, identificador_core);
    }
}

static uint32_t sumar_tamanios(t_list* direcciones) {
    uint32_t total = 0;
    for (int i = 0; i < list_size(direcciones); i++) {
        t_direccion_fisica* pedazo = list_get(direcciones, i);
        total += pedazo->tamanio;
    }
    return total;
}

void atender_syscall_servicio(int identificador_core, uint8_t op_code, t_buffer* buffer) {
    char* nombre_syscall;
    t_servicio_id servicio;

    switch (op_code) {
        case SYSCALL_LOAD_BATCH: nombre_syscall = "LOAD_BATCH"; servicio = SERVICIO_LOADER;  break;
        case SYSCALL_LABEL:      nombre_syscall = "LABEL";      servicio = SERVICIO_LABELER; break;
        case SYSCALL_REPORT:     nombre_syscall = "REPORT";     servicio = SERVICIO_LOGGER;  break;
        default:
            log_error(planificador_logger, "op_code %u no es una syscall de servicio", op_code);
            return;
    }

    uint32_t jid = buffer_read_uint32(buffer);
    t_contexto contexto;
    buffer_read_contexto(buffer, &contexto);
    char* archivo = (op_code == SYSCALL_LOAD_BATCH) ? buffer_read_string(buffer) : NULL;
    uint32_t tamanio = buffer_read_uint32(buffer);
    t_list* direcciones = buffer_read_direcciones(buffer);

    // igual que un DEVOLUCION_JOB mal formado: se descarta
    if (buffer->offset != buffer->size || sumar_tamanios(direcciones) != tamanio) {
        log_error(planificador_logger, "%s mal formada del Core %d (JID %u): tamanio %u, se leyeron %u de %u bytes",
                  nombre_syscall, identificador_core, jid, tamanio, buffer->offset, buffer->size);
        free(archivo);
        direcciones_destruir(direcciones);
        return;
    }

    log_info(planificador_logger, LOG_SYSCALL, jid, nombre_syscall);

    t_pcb* pcb = recibir_job_de_core(identificador_core, jid, &contexto);
    if (pcb == NULL) {
        free(archivo);
        direcciones_destruir(direcciones);
        return;
    }

    pcb->estadisticas.cantidad_syscalls++;

    t_pedido_servicio* pedido = malloc(sizeof(t_pedido_servicio));
    pedido->pcb = pcb;
    pedido->archivo = archivo;
    pedido->tamanio = tamanio;
    pedido->direcciones = direcciones;

    // primero BLOCKED y despues encolar: si no, el servicio podria pasarlo a READY desde EXEC
    cambiar_estado(pcb, ESTADO_BLOCKED);
    encolar_en_servicio(servicio, pedido);
}
