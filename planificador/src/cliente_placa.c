#include "cliente_placa.h"
#include "conexiones.h"
#include <utils/serializacion.h>
#include <utils/protocolo.h>
#include "inicializador.h"

bool placa_crear_job(uint32_t jid, char* archivo_pseudocodigo) {
    pthread_mutex_lock(&conexiones_planificador.mutex_placa);

    t_paquete* paquete = crear_paquete(CREAR_JOB);
    buffer_add_uint32(paquete->buffer, jid);
    buffer_add_string(paquete->buffer, archivo_pseudocodigo);

    int resultado = enviar_paquete(conexiones_planificador.fd_placa, paquete);
    eliminar_paquete(paquete);

    if (resultado != CONEXION_OK) {
        pthread_mutex_unlock(&conexiones_planificador.mutex_placa);
        log_error(planificador_logger, "Envio de paquete para JID %u fallido", jid);
        return false;
    }

    uint8_t op_code;
    resultado = recibir_operacion(conexiones_planificador.fd_placa, &op_code);

    if (resultado != CONEXION_OK) {
        pthread_mutex_unlock(&conexiones_planificador.mutex_placa);
        log_error(planificador_logger, "la Placa no respondio CREAR_JOB del JID %u", jid);
        return false;
    }

    t_buffer* buffer;
    resultado = recibir_buffer(conexiones_planificador.fd_placa, &buffer);

    if (resultado != CONEXION_OK) {
        pthread_mutex_unlock(&conexiones_planificador.mutex_placa);
        log_error(planificador_logger, "la Placa no respondio CREAR_JOB del JID %u", jid);
        return false;
    }

    eliminar_buffer(buffer);

    pthread_mutex_unlock(&conexiones_planificador.mutex_placa);

    if (op_code != CREAR_JOB_OK) {
        log_error(planificador_logger, "Error al crear el JOB con JID %u - archivo %s", jid, archivo_pseudocodigo);
        return false;
    }

    return true;
}