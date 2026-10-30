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

bool placa_finalizar_job(uint32_t jid) {
    pthread_mutex_lock(&conexiones_planificador.mutex_placa);

    t_paquete* paquete = crear_paquete(FINALIZAR_JOB);
    buffer_add_uint32(paquete->buffer, jid);

    int resultado = enviar_paquete(conexiones_planificador.fd_placa, paquete);
    eliminar_paquete(paquete);

    if (resultado != CONEXION_OK) {
        pthread_mutex_unlock(&conexiones_planificador.mutex_placa);
        log_error(planificador_logger, "No se pudo enviar FINALIZAR_JOB del JID %u a la Placa", jid);
        return false;
    }

    uint8_t op_code;
    resultado = recibir_operacion(conexiones_planificador.fd_placa, &op_code);

    if (resultado != CONEXION_OK) {
        pthread_mutex_unlock(&conexiones_planificador.mutex_placa);
        log_error(planificador_logger, "la Placa no respondio FINALIZAR_JOB del JID %u", jid);
        return false;
    }

    t_buffer* buffer;
    resultado = recibir_buffer(conexiones_planificador.fd_placa, &buffer);

    if (resultado != CONEXION_OK) {
        pthread_mutex_unlock(&conexiones_planificador.mutex_placa);
        log_error(planificador_logger, "la Placa no respondio FINALIZAR_JOB del JID %u", jid);
        return false;
    }

    eliminar_buffer(buffer);  // FINALIZAR_JOB_OK viene vacio, pero hay que sacarlo del socket

    pthread_mutex_unlock(&conexiones_planificador.mutex_placa);

    if (op_code != FINALIZAR_JOB_OK) {
        log_error(planificador_logger, "La Placa respondio %u a FINALIZAR_JOB del JID %u", op_code, jid);
        return false;
    }

    return true;
}

bool placa_deslockear_paginas(uint32_t jid) {
    log_info(planificador_logger, LOG_DESLOCKEO, jid);

    pthread_mutex_lock(&conexiones_planificador.mutex_placa);

    t_paquete* paquete = crear_paquete(DESLOCKEAR_PAGINAS);
    buffer_add_uint32(paquete->buffer, jid);

    int resultado = enviar_paquete(conexiones_planificador.fd_placa, paquete);
    eliminar_paquete(paquete);

    if (resultado != CONEXION_OK) {
        pthread_mutex_unlock(&conexiones_planificador.mutex_placa);
        log_error(planificador_logger, "No se pudo enviar DESLOCKEAR_PAGINAS del JID %u a la Placa", jid);
        return false;
    }

    uint8_t op_code;
    resultado = recibir_operacion(conexiones_planificador.fd_placa, &op_code);

    if (resultado != CONEXION_OK) {
        pthread_mutex_unlock(&conexiones_planificador.mutex_placa);
        log_error(planificador_logger, "la Placa no respondio DESLOCKEAR_PAGINAS del JID %u", jid);
        return false;
    }

    t_buffer* buffer;
    resultado = recibir_buffer(conexiones_planificador.fd_placa, &buffer);

    if (resultado != CONEXION_OK) {
        pthread_mutex_unlock(&conexiones_planificador.mutex_placa);
        log_error(planificador_logger, "la Placa no respondio DESLOCKEAR_PAGINAS del JID %u", jid);
        return false;
    }

    eliminar_buffer(buffer);  // DESLOCKEO_OK viene vacio, pero hay que sacarlo del socket

    pthread_mutex_unlock(&conexiones_planificador.mutex_placa);

    if (op_code != DESLOCKEO_OK) {
        log_error(planificador_logger, "La Placa respondio %u a DESLOCKEAR_PAGINAS del JID %u", op_code, jid);
        return false;
    }

    return true;
}

// mutex_placa queda tomado mientras la Placa carga: puede tardar RETARDO_OFFLOAD x 2 si baja una victima
bool placa_cargar_pagina(uint32_t jid, uint32_t nro_pagina) {
    pthread_mutex_lock(&conexiones_planificador.mutex_placa);

    t_paquete* paquete = crear_paquete(CARGAR_PAGINA);
    buffer_add_uint32(paquete->buffer, jid);
    buffer_add_uint32(paquete->buffer, nro_pagina);

    int resultado = enviar_paquete(conexiones_planificador.fd_placa, paquete);
    eliminar_paquete(paquete);

    if (resultado != CONEXION_OK) {
        pthread_mutex_unlock(&conexiones_planificador.mutex_placa);
        log_error(planificador_logger, "No se pudo enviar CARGAR_PAGINA de la pagina %u del JID %u a la Placa", nro_pagina, jid);
        return false;
    }

    uint8_t op_code;
    resultado = recibir_operacion(conexiones_planificador.fd_placa, &op_code);

    if (resultado != CONEXION_OK) {
        pthread_mutex_unlock(&conexiones_planificador.mutex_placa);
        log_error(planificador_logger, "la Placa no respondio CARGAR_PAGINA de la pagina %u del JID %u", nro_pagina, jid);
        return false;
    }

    t_buffer* buffer;
    resultado = recibir_buffer(conexiones_planificador.fd_placa, &buffer);

    if (resultado != CONEXION_OK) {
        pthread_mutex_unlock(&conexiones_planificador.mutex_placa);
        log_error(planificador_logger, "la Placa no respondio CARGAR_PAGINA de la pagina %u del JID %u", nro_pagina, jid);
        return false;
    }

    eliminar_buffer(buffer);  // CARGAR_PAGINA_OK viene vacio, pero hay que sacarlo del socket

    pthread_mutex_unlock(&conexiones_planificador.mutex_placa);

    if (op_code != CARGAR_PAGINA_OK) {
        log_error(planificador_logger, "La Placa respondio %u a CARGAR_PAGINA de la pagina %u del JID %u", op_code, nro_pagina, jid);
        return false;
    }

    return true;
}

bool placa_leer_memoria(uint32_t jid, t_direccion_fisica* pedazo, void* destino) {
    log_info(planificador_logger, LOG_ACCESO_MEMORIA, jid, "Lectura", pedazo->direccion, pedazo->tamanio);

    pthread_mutex_lock(&conexiones_planificador.mutex_placa);

    t_paquete* paquete = crear_paquete(LEER_MEMORIA);
    buffer_add_uint32(paquete->buffer, jid);
    buffer_add_uint32(paquete->buffer, pedazo->direccion);
    buffer_add_uint32(paquete->buffer, pedazo->tamanio);

    int resultado = enviar_paquete(conexiones_planificador.fd_placa, paquete);
    eliminar_paquete(paquete);

    if (resultado != CONEXION_OK) {
        pthread_mutex_unlock(&conexiones_planificador.mutex_placa);
        log_error(planificador_logger, "No se pudo enviar LEER_MEMORIA del JID %u (direccion %u) a la Placa", jid, pedazo->direccion);
        return false;
    }

    uint8_t op_code;
    resultado = recibir_operacion(conexiones_planificador.fd_placa, &op_code);

    if (resultado != CONEXION_OK) {
        pthread_mutex_unlock(&conexiones_planificador.mutex_placa);
        log_error(planificador_logger, "la Placa no respondio LEER_MEMORIA del JID %u (direccion %u)", jid, pedazo->direccion);
        return false;
    }

    t_buffer* buffer;
    resultado = recibir_buffer(conexiones_planificador.fd_placa, &buffer);

    if (resultado != CONEXION_OK) {
        pthread_mutex_unlock(&conexiones_planificador.mutex_placa);
        log_error(planificador_logger, "la Placa no respondio LEER_MEMORIA del JID %u (direccion %u)", jid, pedazo->direccion);
        return false;
    }

    pthread_mutex_unlock(&conexiones_planificador.mutex_placa);  // la respuesta ya salio del socket

    bool ok = false;

    if (op_code == RESPUESTA_LEER_MEMORIA) {
        // el bloque es { uint32 tamanio, bytes }: se verifica que traiga justo lo pedido antes de copiar
        uint32_t declarado = 0;
        if (buffer->size >= sizeof(uint32_t)) memcpy(&declarado, buffer->stream, sizeof(uint32_t));

        if (declarado == pedazo->tamanio && buffer->size == sizeof(uint32_t) + declarado) {
            uint32_t recibido;
            void* datos = buffer_read_bloque(buffer, &recibido);
            memcpy(destino, datos, recibido);
            free(datos);
            ok = true;
        } else {
            log_error(planificador_logger, "La Placa devolvio un bloque de tamanio inesperado al leer del JID %u (direccion %u): se pidio %u", jid, pedazo->direccion, pedazo->tamanio);
        }
    } else if (op_code == MEMORIA_ERROR && buffer->size == sizeof(uint32_t)) {
        log_error(planificador_logger, "La Placa rechazo la lectura del JID %u (direccion %u): motivo %u", jid, pedazo->direccion, buffer_read_uint32(buffer));
    } else {
        log_error(planificador_logger, "La Placa respondio %u a LEER_MEMORIA del JID %u (direccion %u)", op_code, jid, pedazo->direccion);
    }

    eliminar_buffer(buffer);
    return ok;
}

bool placa_escribir_memoria(uint32_t jid, t_direccion_fisica* pedazo, void* origen) {
    log_info(planificador_logger, LOG_ACCESO_MEMORIA, jid, "Escritura", pedazo->direccion, pedazo->tamanio);

    pthread_mutex_lock(&conexiones_planificador.mutex_placa);

    t_paquete* paquete = crear_paquete(ESCRIBIR_MEMORIA);
    buffer_add_uint32(paquete->buffer, jid);
    buffer_add_uint32(paquete->buffer, pedazo->direccion);
    buffer_add_bloque(paquete->buffer, origen, pedazo->tamanio);

    int resultado = enviar_paquete(conexiones_planificador.fd_placa, paquete);
    eliminar_paquete(paquete);

    if (resultado != CONEXION_OK) {
        pthread_mutex_unlock(&conexiones_planificador.mutex_placa);
        log_error(planificador_logger, "No se pudo enviar ESCRIBIR_MEMORIA del JID %u (direccion %u) a la Placa", jid, pedazo->direccion);
        return false;
    }

    uint8_t op_code;
    resultado = recibir_operacion(conexiones_planificador.fd_placa, &op_code);

    if (resultado != CONEXION_OK) {
        pthread_mutex_unlock(&conexiones_planificador.mutex_placa);
        log_error(planificador_logger, "la Placa no respondio ESCRIBIR_MEMORIA del JID %u (direccion %u)", jid, pedazo->direccion);
        return false;
    }

    t_buffer* buffer;
    resultado = recibir_buffer(conexiones_planificador.fd_placa, &buffer);

    if (resultado != CONEXION_OK) {
        pthread_mutex_unlock(&conexiones_planificador.mutex_placa);
        log_error(planificador_logger, "la Placa no respondio ESCRIBIR_MEMORIA del JID %u (direccion %u)", jid, pedazo->direccion);
        return false;
    }

    pthread_mutex_unlock(&conexiones_planificador.mutex_placa);  // la respuesta ya salio del socket

    bool ok = false;

    if (op_code == ESCRIBIR_MEMORIA_OK) {
        ok = true;
    } else if (op_code == MEMORIA_ERROR && buffer->size == sizeof(uint32_t)) {
        log_error(planificador_logger, "La Placa rechazo la escritura del JID %u (direccion %u): motivo %u", jid, pedazo->direccion, buffer_read_uint32(buffer));
    } else {
        log_error(planificador_logger, "La Placa respondio %u a ESCRIBIR_MEMORIA del JID %u (direccion %u)", op_code, jid, pedazo->direccion);
    }

    eliminar_buffer(buffer);
    return ok;
}